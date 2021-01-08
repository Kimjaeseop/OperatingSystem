#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <proc/switch.h>
#include <interrupt.h>
#include <stdlib.h>

extern struct list plist; // 전체 프로세스
extern struct list rlist; // running 프로세스
extern struct list runq[RQ_NQS];

extern struct process procs[PROC_NUM_MAX];
extern struct process *idle_process;
struct process *latest;

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux);
int scheduling; 					// interrupt.c

struct process* get_next_proc(void) 
{
	bool found = false;
	struct process *next = NULL;
	struct list_elem *elem;

	/* 
	   You shoud modify this function...
	   Browse the 'runq' array 
	 */

	// 못 찾을 경우 idle process return 
	next = &procs[0];

	// runq 범위 탐색
	for(int i=0; i < RQ_NQS; i++) {
		if (!list_empty(&runq[i])) { // 비어있지 않은 runq list find
			for(elem = list_begin(&runq[i]); elem != list_end(&runq[i]); elem = list_next(elem))
			{
				struct process *p = list_entry(elem, struct process, elem_stat);
				// 런큐내에서 실행 가능한 process return
				if(p->state == PROC_RUN)
					return p;
			}
		}
	}

	// 못 찾을 경우 idle process return
	return next;
}

void schedule(void)
{
	struct process *cur;
	struct process *next;

	/* You shoud modify this function.... */

	scheduling=1; // tick 계산을 멈추기 위한 flag check
	proc_wake(); // sleep중인 process 중에 sleep time이 지난 process를 깨우기 위한 작업
	intr_disable(); // Context Switching 중 다른 인터럽트가 발생하더라도 인터럽트 핸들러가 수행되지 않도록 함.

	int i=0;

	if (cur_process->pid == 0) { // 현재 프로세스가 idle 프로세스인 경우
		// 프로세스 리스트 내에서 실행가능한 상태이고 idle 프로세스가 아닌 경우 linked list를 옮기면서 프로세스 상태 출력
		for(struct list_elem *elem = list_begin(&plist); elem != list_end(&plist); elem = list_next(elem)) {
			struct process *p = list_entry(elem, struct process, elem_all);
			if (p->state == PROC_RUN && p->pid != 0) {
				if (i==0)
					printk("# = %3d p = %3d c = %3d u = %3d", p->pid, p->priority, p->time_slice, p->time_used);
				else
					printk(", # = %3d p = %3d c = %3d u = %3d", p->pid, p->priority, p->time_slice, p->time_used);
				i++;
			}
		}
	}
	else {
		// 현재 프로세스가 idle 프로세스가 아닌 경우
		for (struct list_elem *elem = list_begin(&plist); elem != list_end(&plist); elem = list_next(elem)) {
			struct process *p = list_entry(elem, struct process, elem_all);
			// 현재 프로세스가 IO중이라면 IO를 시작한 시점 출력
			if (p->state == PROC_BLOCK && cur_process->pid == p->pid)
				printk("Proc %d I/O at %d\n", p->pid, p->time_used);
		}
	}

	cur = cur_process; // 현재 프로세스 저장

	if (cur_process->pid == 0) { // 현재 프로세스가 idle 프로세스인 경우
		next = get_next_proc(); // 다음 프로세스 포인터를 받아옴
		cur_process = next; // 현재 프로세스 포인터에 다음 프로세스 저장
		cur_process->time_slice = 0; // time_slice 0으로 저장
	}
	else { // 현재 프로세스가 idle이 아니라면 idle로 전환
		next = &procs[0]; // 다음 프로세스에 idle 프로세스를 넣음
		cur_process = next; // 현재 프로세스 포인터에 idle 프로세스 저장
	}

	if (next->pid != 0) // 다음 process가 idle 프로세스가 아니라면 프로세스 pid 출력
		printk("\nSeleted # = %d\n", next->pid);

	intr_enable(); // 다시 인터럽트가 들어오면 인터럽트 핸들러가 실행되도록 조치
	scheduling=0; // 다시 tick계산 시작
	switch_process(cur, next); // process context switching
}
