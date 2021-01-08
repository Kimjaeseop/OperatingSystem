#include <filesys/inode.h>
#include <proc/proc.h>
#include <device/console.h>
#include <mem/palloc.h>


int file_open(struct inode *inode, int flags, int mode)
{
	int fd;
	struct ssufile **file_cursor = cur_process->file;

	for(fd = 0; fd < NR_FILEDES; fd++)
	{
		if(file_cursor[fd] == NULL)
		{
			if( (file_cursor[fd] = (struct ssufile *)palloc_get_page()) == NULL)
				return -1;
			break;
		}	
	}
	inode->sn_refcount++;

	file_cursor[fd]->inode = inode;
	file_cursor[fd]->pos = 0;


	if(flags & O_APPEND){ // O_APPEND 플래그가 파일 오픈시 들어오는 경우
		file_cursor[fd]->pos += inode->sn_size; // 기존에 있던 파일 크기만큼 pos에 더해주면 위치가 끝으로 이동
	}
	else if(flags & O_TRUNC){ // O_TRUNC 플래그가 파일 오픈시 들어오는 경우
		inode->sn_size=0; // inode에 있는 파일 크기를 0로 만들어서 초기화
		file_cursor[fd]->pos = 0; // 파일 크기가 0이니 기존 파일 오프셋도 0으로 초기화
	}

	file_cursor[fd]->flags = flags;
	file_cursor[fd]->unused = 0;

	return fd;
}

int file_write(struct inode *inode, size_t offset, void *buf, size_t len)
{
	return inode_write(inode, offset, buf, len);
}

int file_read(struct inode *inode, size_t offset, void *buf, size_t len)
{
	return inode_read(inode, offset, buf, len);
}
