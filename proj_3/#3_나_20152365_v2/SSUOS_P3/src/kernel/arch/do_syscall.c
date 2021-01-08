#include <proc/sched.h>
#include <proc/proc.h>
#include <device/device.h>
#include <interrupt.h>
#include <device/kbd.h>
#include <filesys/file.h>

pid_t do_fork(proc_func func, void* aux1, void* aux2)
{
	pid_t pid;
	struct proc_option opt;

	opt.priority = cur_process-> priority;
	pid = proc_create(func, &opt, aux1, aux2);

	return pid;
}

void do_exit(int status)
{
	cur_process->exit_status = status; 	//종료 상태 저장
	proc_free();						//프로세스 자원 해제
	do_sched_on_return();				//인터럽트 종료시 스케줄링
}

pid_t do_wait(int *status)
{
	while(cur_process->child_pid != -1)
		schedule();
	//SSUMUST : schedule 제거.

	int pid = cur_process->child_pid;
	cur_process->child_pid = -1;

	extern struct process procs[];
	procs[pid].state = PROC_UNUSED;

	if(!status)
		*status = procs[pid].exit_status;

	return pid;
}

void do_shutdown(void)
{
	dev_shutdown();
	return;
}

int do_ssuread(void)
{
	return kbd_read_char();
}

int do_open(const char *pathname, int flags)
{
	struct inode *inode;
	struct ssufile **file_cursor = cur_process->file;
	int fd;

	for(fd = 0; fd < NR_FILEDES; fd++)
		if(file_cursor[fd] == NULL) break;

	if (fd == NR_FILEDES)
		return -1;

	if ( (inode = inode_open(pathname, flags)) == NULL)
		return -1;

	if (inode->sn_type == SSU_TYPE_DIR)
		return -1;

	fd = file_open(inode,flags,0);

	return fd;
}

int do_read(int fd, char *buf, int len)
{
	return generic_read(fd, (void *)buf, len);
}
int do_write(int fd, const char *buf, int len)
{
	return generic_write(fd, (void *)buf, len);
}

int do_fcntl(int fd, int cmd, long arg)
{
	int flag = -1;
	struct ssufile **file_cursor = cur_process->file;

	if (cmd & F_DUPFD){ // DUP과 같은 기능, fd 복사
		for (; arg < NR_FILEDES && file_cursor[arg] != NULL; arg++); // arg가 가능한 범위이고 ssu_file이 이미 할당되어 있다면 arg++로 하나씩 늘려가며 빈 fd를 찾음
		
		if (arg >= NR_FILEDES) // 만약 위의 반복문이 끝까지 돌았거나 arg가 가능한 범위보다 크다면 더 이상 할당 불가능하기 때문에 -1을 반환
			return -1;

		file_cursor[arg] = (struct ssufile *)palloc_get_page(); // 모든 예외를 지났다면 새로운 ssu_file에 메모리를 할당

		// 모든 구조체 멤버 복사
		file_cursor[arg]->inode = file_cursor[fd]->inode;
		file_cursor[arg]->pos = file_cursor[fd]->pos;
		file_cursor[arg]->flags = file_cursor[fd]->flags;
		file_cursor[arg]->unused = file_cursor[fd]->unused;

		return arg; // 할당에 성공한 fd 반환
	}
	else if (cmd & F_GETFL){ // 매개변수로 전달받은 fd 파일의 flags를 얻기 위한 플래그
		return file_cursor[fd]->flags; // file_cursor구조체 배열에 매개변수로 전달받은 fd를 인덱스로 사용하여 구조체 멤버 flags 반환
	}
	else if(cmd & F_SETFL){ // 매개변수로 O_APPEND를 전달 받았다면 fd 파일의 플래그에 추가
		if (arg & O_APPEND) {
			file_cursor[fd]->pos += file_cursor[fd]->inode->sn_size; // pos값에 파일크기를 더해서  끝으로 이동
			file_cursor[fd]->flags |= O_APPEND; // file_cursor의 flags에 O_APPEND 추가
			return file_cursor[fd]->flags; // 변경에 성공한 플래그 반환
		}
		else
			return -1; // 만약 O_APPEND가 아닌 다른 플래그가 들어온 경우 -1반환 예외처리
	}
	else{
		return -1; // 가능한 플래그 이외에 다른 값이 들어오는 경우의 예외처리
	}

}
