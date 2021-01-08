#ifndef __PALLOC_H__
#define __PALLOC_H__

#include <ssulib.h>
#include <synch.h>
enum palloc_flags
{
	kernel_area= 0,			 /*Kernel page*/
	user_area = 1,           /* User page. */
};


void init_palloc (void);
uint32_t *palloc_get_one_page (enum palloc_flags flag);
uint32_t *palloc_get_multiple_page (enum palloc_flags flag, size_t page_cnt);
//void palloc_free_one_page (enum palloc_flags flags, void *);
//void palloc_free_multiple_page (enum palloc_flags flags, void *, size_t page_cnt);
void palloc_free_one_page (void *);
void palloc_free_multiple_page (void *, size_t page_cnt);
void palloc_pf_test(void);
void swap_test(void);

#endif /* palloc.h */

