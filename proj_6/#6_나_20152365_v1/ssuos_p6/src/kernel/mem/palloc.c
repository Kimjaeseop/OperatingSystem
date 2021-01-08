#include <mem/palloc.h>
#include <bitmap.h>
#include <type.h>
#include <round.h>
#include <mem/mm.h>
#include <synch.h>
#include <device/console.h>
#include <mem/paging.h>
#include <mem/swap.h>

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  
 */
/* pool for memory */
struct memory_pool
{
	struct lock lock;                   
	struct bitmap *bitmap; 
	uint32_t *addr;                    
};
/* kernel heap page struct */
struct khpage{
	uint16_t page_type;
	uint16_t nalloc;
	uint32_t used_bit[4];
	struct khpage *next;
};

/* free list */
struct freelist{
	struct khpage *list;
	int nfree;
};

static uint32_t upage_alloc_index;
static uint32_t kpage_alloc_index;
struct memory_pool kernel_pool;
struct memory_pool user_pool;

/* Initializes the page allocator. */
//
	void
init_palloc (void) 
{
	upage_alloc_index = 0;
	kpage_alloc_index = 0;
	size_t bm_struct_size;

	// 비트맵 초기화 과정
	// 각 pool 별로 초기화
	// 매개변수로 할당 될 수 있는 최대 페이지 개수, 시작주소, block size를 넘김

	// 비트맵 필요 바이트수 계산
	bm_struct_size = DIV_ROUND_UP(bitmap_struct_size((USER_POOL_START-KERNEL_ADDR)/PAGE_SIZE), PAGE_SIZE);
	kernel_pool.bitmap = create_bitmap((USER_POOL_START-KERNEL_ADDR-PAGE_SIZE)/PAGE_SIZE,
			KERNEL_ADDR,
			bm_struct_size*PAGE_SIZE);

	// 페이지 할당시에 사용할 시작주소 // 비트맵이 할당된 공간이후를 시작점으로 할당
	kernel_pool.addr = KERNEL_ADDR + bm_struct_size * PAGE_SIZE;

	// 비트맵 필요 바이트수 계산
	bm_struct_size = DIV_ROUND_UP(bitmap_struct_size((RKERNEL_HEAP_START-USER_POOL_START)/PAGE_SIZE), PAGE_SIZE);
	user_pool.bitmap = create_bitmap((RKERNEL_HEAP_START-USER_POOL_START-PAGE_SIZE)/PAGE_SIZE, 
			USER_POOL_START,
			bm_struct_size*PAGE_SIZE);

	// 페이지 할당시에 사용할 시작주소 // 비트맵이 할당된 공간이후를 시작점으로 할당
	user_pool.addr = USER_POOL_START + bm_struct_size * PAGE_SIZE;
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
 */
	uint32_t *
palloc_get_multiple_page (enum palloc_flags flag, size_t page_cnt)
{
	void *pages = NULL;
	size_t page_idx = 0;

	if (page_cnt == 0)
		return NULL;

	// USER POOL
	if (flag == user_area) {
		// 비어있는 비트맵 위치 찾아서 idx로 할당
		//upage_alloc_index = find_set_bitmap(user_pool.bitmap, 0, page_cnt, 0);
		page_idx = find_set_bitmap(user_pool.bitmap, 0, page_cnt, 0);
		// 가능한 index (bitmap bit)가 없을 경우 에러메세지 출력 후 종료
		if (upage_alloc_index == BITMAP_ERROR) { 
			printk("BITMAP_ERROR\n");
			return -1;
		}
		// 페이지 주소 계산 후 할당
		pages = (void *)((uint32_t)user_pool.addr + page_idx * PAGE_SIZE);
	} // KERNEL_POOL
	else if (flag == kernel_area) {
		// 비어있는 비트맵 위치 찾아서 idx로 할당
		//kpage_alloc_index = find_set_bitmap(kernel_pool.bitmap, 0, page_cnt, 0);
		page_idx = find_set_bitmap(kernel_pool.bitmap, 0, page_cnt, 0);
		// 가능한 index (bitmap bit)가 없을 경우 에러메세지를 출력 후 종료
		if (kpage_alloc_index == BITMAP_ERROR) {
			printk("BITMAP_ERROR\n");
			return -1;
		}
		// 페이지 주소 계산 후 할당
		pages = (void *)((uint32_t)kernel_pool.addr + page_idx * PAGE_SIZE);
	}

	if (pages != NULL) // 페이지 초기화
		memset (pages, 0, PAGE_SIZE * page_cnt);

	// 페이지 주소 할당 후 반환
	return (uint32_t*)pages; 
}

/* Obtains a single free page and returns its address.
 */
	uint32_t *
palloc_get_one_page (enum palloc_flags flag) 
{
	return palloc_get_multiple_page (flag, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
	void
palloc_free_multiple_page (void *pages, size_t page_cnt) 
{
	if (pages == NULL || page_cnt == 0) // 페이지 주소가 비어 있거나 count == 0일 때 종료
		return;

	// 비트맵 해제 과정
	if (pages < USER_POOL_START)  // kernel pool
		set_multi_bitmap(kernel_pool.bitmap, (size_t)((uint32_t)pages-(uint32_t)kernel_pool.addr)/PAGE_SIZE, page_cnt, 0);
	else if (pages > USER_POOL_START) // user pool
		set_multi_bitmap(user_pool.bitmap, (size_t)((uint32_t)pages-(uint32_t)user_pool.addr)/PAGE_SIZE, page_cnt, 0);
}

/* Frees the page at PAGE. */
	void
palloc_free_one_page (void *page) 
{
	palloc_free_multiple_page (page, 1);
}


void palloc_pf_test(void)
{
	uint32_t *one_page1 = palloc_get_one_page(user_area);
	uint32_t *one_page2 = palloc_get_one_page(user_area);
	uint32_t *two_page1 = palloc_get_multiple_page(user_area,2);
	uint32_t *three_page;

	printk("one_page1 = %x\n", one_page1); 
	printk("one_page2 = %x\n", one_page2); 
	printk("two_page1 = %x\n", two_page1);

	printk("=----------------------------------=\n");
	palloc_free_one_page(one_page1);
	palloc_free_one_page(one_page2);
	palloc_free_multiple_page(two_page1,2);

	one_page1 = palloc_get_one_page(user_area);
	one_page2 = palloc_get_one_page(user_area);
	two_page1 = palloc_get_multiple_page(user_area,2);

	printk("one_page1 = %x\n", one_page1);
	printk("one_page2 = %x\n", one_page2);
	printk("two_page1 = %x\n", two_page1);

	printk("=----------------------------------=\n");
	palloc_free_multiple_page(one_page2, 3);
	one_page2 = palloc_get_one_page(user_area);
	three_page = palloc_get_multiple_page(user_area,3);

	printk("one_page1 = %x\n", one_page1);
	printk("one_page2 = %x\n", one_page2);
	printk("three_page = %x\n", three_page);

	palloc_free_one_page(one_page1);
	palloc_free_one_page(three_page);
	three_page = (uint32_t*)((uint32_t)three_page + 0x1000);
	palloc_free_one_page(three_page);
	three_page = (uint32_t*)((uint32_t)three_page + 0x1000);
	palloc_free_one_page(three_page);
	palloc_free_one_page(one_page2);
}
