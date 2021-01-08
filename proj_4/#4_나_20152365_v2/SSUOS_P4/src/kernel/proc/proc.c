#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <ssulib.h>
#include <interrupt.h>
#include <proc/sched.h>
#include <syscall.h>
#include <mem/palloc.h>

#define STACK_SIZE 512

struct list plist;				// All Process List
struct list slist;				// Sleep Process List
struct list rlist;				// Running Process List
struct list runq[RQ_NQS];		// Priority array

struct process procs[PROC_NUM_MAX];
struct process *cur_process;
struct process *idle_process;
int pid_num_max;
uint32_t process_stack_ofs;
static int lock_pid_simple; 
static int lately_pid;

bool more_prio(const struct list_elem *a, const struct list_elem *b, void *aux);
bool less_time_sleep(const struct list_elem *a, const struct list_elem *b, void *aux);
pid_t getValidPid(int *idx);
void proc_start(void);
void proc_end(void);
void proc_getpos(int, int *);	

void kernel1_proc(void *aux);
void kernel2_proc(void *aux);
void kernel3_proc(void *aux);

/*
Hint : use 'printk' function to trace function call
 */

void init_proc()
{
	int i;
	process_stack_ofs = offsetof (struct process, stack);

	lock_pid_simple = 0;
	lately_pid = -1;

	list_init(&plist);
	list_init(&rlist);
	list_init(&slist);

	for (i = 0; i < PROC_NUM_MAX; i++)
	{
		procs[i].pid = i;
		procs[i].state = PROC_UNUSED;
		procs[i].parent = NULL;
	}

	pid_t pid = getValidPid(&i);
	cur_process = (int *)&procs[0];
	idle_process = (int *)&procs[0];

	cur_process->pid = pid;
	cur_process->parent = NULL;
	cur_process->state = PROC_RUN;

	cur_process -> priority = 99;

	cur_process->stack = 0;
	cur_process->pd = (void*)read_cr3();
	cur_process -> elem_all.prev = NULL;
	cur_process -> elem_all.next = NULL;
	cur_process -> elem_stat.prev = NULL;
	cur_process -> elem_stat.next = NULL;

	/* You should modify this function... */
	for (int i=0; i<RQ_NQS; i++) // runq list init
		list_init(&(runq[i]));

	list_push_back(&plist, &cur_process->elem_all); // process list 맨 뒤에 추가
	list_push_back(&runq[cur_process->priority/RQ_PPQ], &cur_process->elem_stat); // runq 해당 index 맨 뒤에 추가 (idle priority : 99, 99/4 = 24, runq[24]의 맨 뒤에 추가)
}

pid_t getValidPid(int *idx) {
	pid_t pid = -1;
	int i;

	while(lock_pid_simple);

	lock_pid_simple++;

	for(i = 0; i < PROC_NUM_MAX; i++)
	{
		int tmp = i + lately_pid + 1;
		if(procs[tmp % PROC_NUM_MAX].state == PROC_UNUSED) { 
			pid = lately_pid + 1;
			*idx = tmp % PROC_NUM_MAX;
			break;
		}
	}

	if(pid != -1)
		lately_pid = pid;	

	lock_pid_simple = 0;

	return pid;
}

pid_t proc_create(proc_func func, struct proc_option *opt, void* aux)
{
	struct process *p;
	int idx;

	enum intr_level old_level = intr_disable();

	pid_t pid = getValidPid(&idx);
	p = &procs[pid];
	p->pid = pid;
	p->state = PROC_RUN;

	if(opt != NULL) 
		p -> priority = opt -> priority;
	else 
		p -> priority = (unsigned char)99;

	p->time_used = 0;
	p->time_slice = 0;
	p->parent = cur_process;
	p->simple_lock = 0;
	p->child_pid = -1;
	p->pd = pd_create(p->pid);

	//init stack
	int *top = (int*)palloc_get_page();
	int stack = (int)top;
	top = (int*)stack + STACK_SIZE - 1;

	*(--top) = (int)aux;		//argument for func
	*(--top) = (int)proc_end;	//return address from func
	*(--top) = (int)func;		//return address from proc_start
	*(--top) = (int)proc_start; //return address from switch_process

	//process call stack : 
	//switch_process > proc_start > func(aux) > proc_end

	*(--top) = (int)((int*)stack + STACK_SIZE - 1); //ebp
	*(--top) = 1; //eax
	*(--top) = 2; //ebx
	*(--top) = 3; //ecx
	*(--top) = 4; //edx
	*(--top) = 5; //esi
	*(--top) = 6; //edi

	p -> stack = top;
	p -> elem_all.prev = NULL;
	p -> elem_all.next = NULL;
	p -> elem_stat.prev = NULL;
	p -> elem_stat.next = NULL;


	/* You should modify this function... */
	proc_getpos(p->priority, &idx); // 해당 우선순위의 index를 받아 온다.
	list_push_back(&plist, &p->elem_all); // process list 맨 뒤에 추가
	list_push_back(&runq[idx], &p->elem_stat); // runq index list 맨 뒤에 추가

	intr_set_level (old_level);
	return p->pid;
}

void* getEIP()
{
	return __builtin_return_address(0);
}
// process priority 재계산 // 30 ticks 마다 호출된다.
void recalculate_priority(void)
{
	int idx;
	struct process *p = cur_process;


	// idle process는 계산에서 제외
	if ((p->pid) != 0) {
		p->priority += (p->time_slice / 10); // 우선순위 재계산 공식
		// 현재 프로세스의 runq idx를 받아온다.
		proc_getpos(p->priority, &idx);
		list_push_back(&runq[idx], &p->elem_stat); // 재계산 이후 위치를 옮겨준다
	}
}

void  proc_start(void)
{
	intr_enable ();
	return;
}

void proc_free(void)
{
	uint32_t pt = *(uint32_t*)cur_process->pd;
	cur_process->parent->child_pid = cur_process->pid;
	cur_process->parent->simple_lock = 0;

	cur_process->state = PROC_ZOMBIE;

	palloc_free_page(cur_process->stack);
	palloc_free_page((void*)pt);
	palloc_free_page(cur_process->pd);

	list_remove(&cur_process->elem_stat);
	list_remove(&cur_process->elem_all);
}

void proc_end(void)
{
	proc_free();
	schedule();
	return;
}

void proc_wake(void)
{
	struct process* p;
	int idx;
	int old_level;
	unsigned long long t = get_ticks();

	// sleep list가 비어있지 않은 경우
	while(!list_empty(&slist))
	{
		// sleep list를 가져온다.
		p = list_entry(list_front(&slist), struct process, elem_stat);
		// time_sleep이 아직 지나지 않았으면 break
		if(p->time_sleep > t) {
			break;
		}

		/* You should modify this function... */
		// sleep list에서 삭제
		list_remove(&p->elem_stat);
		// 프로세스 상태를 바꾸고, running list에 다시 추가하기 위한 함수호출
		proc_unblock(p);
	}
}

void proc_sleep(unsigned ticks)
{
	unsigned long cur_ticks = get_ticks();

	/* You should modify this function... */

	cur_process->time_sleep = cur_ticks + ticks; // sleep에서 깨어날 시간 설정
	cur_process->time_slice = 0; // time slice 0으로 설정

	list_remove(&cur_process->elem_stat); // runq에서 제거
	list_insert_ordered(&slist, &cur_process->elem_stat,
			less_time_sleep, NULL);
	proc_block(); // process를 block으로 바꾸고 스케줄 함수를 호출하기 위한 함수호출
}

void proc_block(void)
{
	cur_process->state = PROC_BLOCK; // process 상태 변경
	schedule(); // 스케줄 함수 호출
}

void proc_unblock(struct process* proc)
{
	enum intr_level old_level;
	int idx;

	proc_getpos(proc->priority, &idx); // 우선순위에 따른 runq인덱스 계산

	/* You shoud modify this function... */
	proc->state = PROC_RUN; // 상태를 다시 RUN 상태로 바꿈
	list_push_back(&runq[idx], &proc->elem_stat); // runq의 맨 뒤에 다시 삽입
}     

bool less_time_sleep(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);

	return p1->time_sleep < p2->time_sleep;
}

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);

	return p1->priority > p2->priority;
}

void kernel1_proc(void* aux)
{
	int passed = 0;

	while(1)
	{
		// IO 처리
		if (procs[1].time_used >= 140 && passed==0) {
			passed=1;
			proc_sleep(60);
		}
		// process end // IO가 이미 진행 되었고 200 틱이 지났다면 process end
		if (procs[1].time_used >= 200 && passed!=0)
			proc_end();

		/* Your code goes here... */
	}
}

void kernel2_proc(void* aux)
{
	int passed = 0;

	while(1)
	{
		// IO 처리
		if (procs[2].time_used >= 100 && passed==0) {
			passed=1;
			proc_sleep(60);
		}
		// 120틱이 지났고 IO처리가 끝났다면 프로세스 종료
		if (procs[2].time_used >= 120 && passed!=0)
			proc_end();
		/* Your code goes here... */
	}
}

void kernel3_proc(void* aux)
{
	int passed1 = 0, passed2 = 0;

	while(1)
	{
		// IO 처리
		if (procs[3].time_used >= 50 && passed1==0) {
			passed1=1;
			proc_sleep(60);
		}
		// IO 처리
		if (procs[3].time_used >= 100 && passed2==0) {
			passed2=1;
			proc_sleep(60);
		}
		// 위에서 IO 처리 두번이 끝났고, 150 틱 지났다면 process 종료
		if (procs[3].time_used >= 150 && passed1!=0 && passed2!=0)
					proc_end();
		/* Your code goes here... */
	}
}

/*
   Let's say RQ_NQS is 5 and RQ_PPQ is 4, then the location of process with priority 7 is : 
   7/4=1
   0 [ ]
   1 [*]
   2 [ ]
   3 [ ]
   4 [ ]
 */

// idle process, pid = 0
void idle(void* aux)
{
	struct proc_option opt1 = { .priority = 50 };
	struct proc_option opt2 = { .priority = 50 };
	//struct proc_option opt3 = { .priority = 30 };

	proc_create(kernel1_proc, &opt1, NULL);
	proc_create(kernel2_proc, &opt2, NULL);
	//proc_create(kernel3_proc, &opt3, NULL);

	while(1) {  
		schedule();    
	}
}

void proc_getpos(int priority, int *idx) 
{ 
	// 우선순위에 따른 runq 인덱스 포인터로 반환
	*idx = priority / RQ_PPQ;

	/* Your code goes here... */
}

void proc_print_data()
{
	int a, b, c, d, bp, si, di, sp;

	//eax ebx ecx edx
	__asm__ __volatile("mov %%eax ,%0": "=m"(a));

	__asm__ __volatile("mov %ebx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(b));

	__asm__ __volatile("mov %ecx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(c));

	__asm__ __volatile("mov %edx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(d));

	//ebp esi edi esp
	__asm__ __volatile("mov %ebp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(bp));

	__asm__ __volatile("mov %esi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(si));

	__asm__ __volatile("mov %edi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(di));

	__asm__ __volatile("mov %esp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(sp));

	printk(	"\neax %o ebx %o ecx %o edx %o"\
			"\nebp %o esi %o edi %o esp %o\n"\
			, a, b, c, d, bp, si, di, sp);
}

void hexDump (void *addr, int len) {
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char*)addr;

	if (len == 0) {
		printk("  ZERO LENGTH\n");
		return;
	}
	if (len < 0) {
		printk("  NEGATIVE LENGTH: %i\n",len);
		return;
	}

	for (i = 0; i < len; i++) {
		if ((i % 16) == 0) {
			if (i != 0)
				printk ("  %s\n", buff);

			printk ("  %04x ", i);
		}

		printk (" %02x", pc[i]);

		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printk ("   ");
		i++;
	}

	printk ("  %s\n", buff);
}
