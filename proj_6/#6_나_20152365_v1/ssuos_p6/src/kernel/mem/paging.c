#include <device/io.h>
#include <mem/mm.h>
#include <mem/paging.h>
#include <device/console.h>
#include <proc/proc.h>
#include <interrupt.h>
#include <mem/palloc.h>
#include <ssulib.h>
#include <device/ide.h>

uint32_t *PID0_PAGE_DIR;

intr_handler_func pf_handler;

//모든 함수는 수정이 가능가능

uint32_t scale_up(uint32_t base, uint32_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask + size;

	return base;
}
//해당 코드를 사용하지 않고 구현해도 무관함
// void pagememcpy(void* from, void* to, uint32_t len)
// {
// 	uint32_t *p1 = (uint32_t*)from;
// 	uint32_t *p2 = (uint32_t*)to;
// 	int i, e;

// 	e = len/sizeof(uint32_t);
// 	for(i = 0; i<e; i++)
// 		p2[i] = p1[i];

// 	e = len%sizeof(uint32_t);
// 	if( e != 0)
// 	{
// 		uint8_t *p3 = (uint8_t*)p1;
// 		uint8_t *p4 = (uint8_t*)p2;

// 		for(i = 0; i<e; i++)
// 			p4[i] = p3[i];
// 	}
// }

uint32_t scale_down(uint32_t base, uint32_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask - size;

	return base;
}

void init_paging()
{
	// page_dir, page_tbl kernel_addr에 생성
	uint32_t *page_dir = palloc_get_one_page(kernel_area);
	uint32_t *page_tbl = palloc_get_one_page(kernel_area);
	PID0_PAGE_DIR = page_dir; // pid 0 (idle)의 페이지 디렉터리의 주소를 지정

	int NUM_PT, NUM_PE;
	uint32_t cr0_paging_set;
	int i;

	NUM_PE = mem_size() / PAGE_SIZE;
	NUM_PT = NUM_PE / 1024;

	//페이지 디렉토리 구성
	page_dir[0] = (uint32_t)page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;

	// RKERNEL_HEAP_START까지 관리 
	NUM_PE = RKERNEL_HEAP_START / PAGE_SIZE;
	//페이지 테이블 구성
	for ( i = 0; i < NUM_PE; i++ ) {
		page_tbl[i] = (PAGE_SIZE * i)
			| PAGE_FLAG_RW
			| PAGE_FLAG_PRESENT;
		//writable & present
	}


	//CR0레지스터 설정
	cr0_paging_set = read_cr0() | CR0_FLAG_PG;		// PG bit set // paging 사용

	//컨트롤 레지스터 저장
	write_cr3( (unsigned)page_dir );		// cr3 레지스터에 PDE 시작주소 저장
	write_cr0( cr0_paging_set );          // PG bit를 설정한 값을 cr0 레지스터에 저장

	reg_handler(14, pf_handler);
}

void memcpy_hard(void* from, void* to, uint32_t len)
{
	write_cr0( read_cr0() & ~CR0_FLAG_PG);
	memcpy(from, to, len);
	write_cr0( read_cr0() | CR0_FLAG_PG);
}

uint32_t* get_cur_pd()
{
	return (uint32_t*)read_cr3();
}

uint32_t pde_idx_addr(uint32_t* addr)
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PDE) >> PAGE_OFFSET_PDE;
	return ret;
}

uint32_t pte_idx_addr(uint32_t* addr)
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PTE) >> PAGE_OFFSET_PTE;
	return ret;
}
//page directory에서 index 위치의 page table 얻기
uint32_t* pt_pde(uint32_t pde)
{
	uint32_t * ret = (uint32_t*)(pde & PAGE_MASK_BASE);
	return ret;
}
//address에서 page table 얻기
uint32_t* pt_addr(uint32_t* addr)
{
	uint32_t idx = pde_idx_addr(addr);
	uint32_t* pd = get_cur_pd();
	return pt_pde(pd[idx]);
}
//address에서 page 주소 얻기
uint32_t* pg_addr(uint32_t* addr)
{
	uint32_t *pt = pt_addr(addr);
	uint32_t idx = pte_idx_addr(addr);
	uint32_t *ret = (uint32_t*)(pt[idx] & PAGE_MASK_BASE);
	return ret;
}

/*
   page table 복사
 */
void  pt_copy(uint32_t *pd, uint32_t *dest_pd, uint32_t idx, uint32_t* start, uint32_t* end, bool share)
{
	uint32_t *pt = pt_pde(pd[idx]); // 해당 페이지 테이블의 PTE를 반환받음
	uint32_t *new_pt;
	uint32_t *pte_s = start;
	uint32_t *pte_e = end;
	uint32_t s, e, i;
	uint32_t size = PAGE_SIZE;

	// 새로운 페이지 주소 할당
	new_pt = palloc_get_one_page(kernel_area);
	// 인덱스로 받은 페이지 디렉토리에 새로 할당한 페이지 테이블의 주소 할당
	dest_pd[idx] = (uint32_t)new_pt | (pd[idx] & ~PAGE_MASK_BASE & ~PAGE_FLAG_ACCESS);

	// 시작점과 끝점이 같으면 복사할 페이지 테이블이 없으므로 종료
	if (start == end)
		return 0;

	pte_s = (uint32_t *)scale_down((uint32_t)pte_s, size);
	pte_e = (uint32_t *)scale_up((uint32_t)pte_e, size);

	// 물리 메모리와 가상메모리(idx = 768)가 인덱스가 다르게 나와서 가상메모리(or page_fault) 처리가능
	s = pte_idx_addr(pte_s); // 복사 시작점
	e = pte_idx_addr(pte_e); // 복사 끝점

	if (e == 0 && start != end) // 끝점이 0인데 시작점과 같지 않다면
		e = PAGE_TBL_SIZE/sizeof(uint32_t); // 끝점 재지정

	for (i = s; i < e; i++)
		if (pt[i] & PAGE_FLAG_PRESENT) { // 각 페이지 테이블의 주소값에  present_bit(유효비트)확인
			if (share) // 공유의 경우
				new_pt[i] = pt[i]; // 페이지 테이블에 있는 내용(프레임 주소)를 copy해서 같은 주소를 갖도록 지정
			else { // 공유가 아닌경우
				uint32_t* pg = palloc_get_one_page(kernel_area); // 새로운 페이지엔트리 할당
				new_pt[i] = (uint32_t)pg | (pt[i] & ~PAGE_MASK_BASE & ~PAGE_FLAG_ACCESS); // 페이지 테이블에 새로운 페이지엔트리 주소를 할당해줌
				memcpy_hard((void *)(pt[i] & PAGE_MASK_BASE), (void *)pg, PAGE_SIZE); // 새로 할당받은 페이지테이블에 주소를 옮기는 것(공유)가 아닌 내용물만 하드카피
			}
		}

	dest_pd = &dest_pd[idx]; // 반환할 페이지 디렉터리 엔트리를 포인터 매개변수로 전달(반환)
}

/* 
   page directory 복사. 
   커널 영역 복사나 fork에서 사용
 */
void pd_copy(uint32_t* pd, uint32_t* dest_pd, uint32_t idx, uint32_t* start, uint32_t* end, bool share)
{
	uint32_t *pde_s = start; // 시작점
	uint32_t *pde_e = end; // 끝점
	uint32_t s, e, i;
	uint32_t size = PAGE_SIZE * PAGE_TBL_SIZE/sizeof(uint32_t); // 페이지 엔트리의 주소 개수

	pde_s = (uint32_t *)scale_down((uint32_t)pde_s, size);
	pde_e = (uint32_t *)scale_up((uint32_t)pde_e, size);

	// 물리 메모리와 가상 메모리가 인덱스가 다르게 리턴되어서 가상메모리(or page_fault) 처리가능
	s = pde_idx_addr(pde_s); // 복사 시작점
	e = pde_idx_addr(pde_e); // 복사 끝점

	for (i = s; i < e; i++) // 시작점과 끝점까지 카피
		if (pd[i] & PAGE_FLAG_PRESENT) // present(유효 비트)가 마스킹되어있는 pd만 pt_copy에 넘김
			pt_copy(pd, dest_pd, i, start, end, share);
}

uint32_t* pd_create (pid_t pid)
{
	uint32_t *pd = palloc_get_one_page(kernel_area); // 새로 할당할 페이지 디렉토리 주소를 받

	// init_paging에서 할당한 주소(pid0 page_dir addr)를 기준으로 페이지 디렉터리를 sharing 하는 방식으로 copy
	pd_copy(PID0_PAGE_DIR, pd, (uint32_t)0, (uint32_t *)0, (uint32_t *)RKERNEL_HEAP_START, true);

	return pd; // page_directory의 실제주소 반환 (kernel_addr)
}

void pf_handler(struct intr_frame *iframe)
{
	void *fault_addr;

	asm ("movl %%cr2, %0" : "=r" (fault_addr));
#ifdef SCREEN_SCROLL
	refreshScreen();
#endif

	uint32_t pdi, pti;
	uint32_t *pta;
	uint32_t *pda = (uint32_t*)read_cr3(); // init_paging에서 지정한 page_directory 주소를 레지스터에서 읽어온다

	// 페이지 폴트 주소의 페이지 디렉터리와 테이블의 인덱스 계산
	pdi = pde_idx_addr(fault_addr); 
	pti = pte_idx_addr(fault_addr);

	if(pda == PID0_PAGE_DIR){ 
		write_cr0( read_cr0() & ~CR0_FLAG_PG); // PG bit 비활성화
		pta = pt_pde(pda[pdi]); // 페이지폴트 페이지디렉터리 인덱스의 페이지 테이블 엔트리 반환 받음
		write_cr0( read_cr0() | CR0_FLAG_PG); // PG bit set
	}
	else{
		pta = pt_pde(pda[pdi]); // 해당 페이지 디렉터리의 페이지 테이블 엔드리를 반환 받음
	}

	if(pta == NULL){
		write_cr0( read_cr0() & ~CR0_FLAG_PG); // PG bit 비활성화

		pta = palloc_get_one_page(kernel_area); // 새로운 페이지 테이블 주소
		memset(pta,0,PAGE_SIZE); // 할당 받은 주소 초기화

		pda[pdi] = (uint32_t)pta | PAGE_FLAG_RW | PAGE_FLAG_PRESENT; // RW, present bit set

		fault_addr = VH_TO_KA(fault_addr); // fault addr 가상주소 -> 물리주소
		// fault_addr : RH, pta : RH
		pta[pti] = (uint32_t)fault_addr | PAGE_FLAG_RW  | PAGE_FLAG_PRESENT;
		// 페이지 테이블에 페이지 폴트의 실주소 할당

		pdi = pde_idx_addr(pta); // 새로 할당한 주소의 페이지 디렉터리와 테이블의 index
		pti = pte_idx_addr(pta);

		/*
		uint32_t *tmp_pta = pt_pde(pda[pdi]); // 새로 할당한 페이지 테이블 엔트리 주소
		// tmp_pta : RH, pta : RH
		tmp_pta[pti] = (uint32_t)(pta) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT; // 페이지 테이블 엔트리 주소에 폴트주소 페이지 테이블 주소 할당
		*/

		write_cr0( read_cr0() | CR0_FLAG_PG); // PG bit set
	}
	else{
		fault_addr = VH_TO_KA(fault_addr); // 폴트 주소 실주소로 변환
		pta[pti] = (uint32_t)fault_addr | PAGE_FLAG_RW  | PAGE_FLAG_PRESENT; // 페이지 테이블에 폴트주소 추가
	}
}
