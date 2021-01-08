#ifndef __HASHING_H__
#define __HASHING_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <type.h>
#include <proc/proc.h>
#include <ssulib.h>

#define SLOT_NUM 4							// The number of slots in a bucket
#define CAPACITY 256						// level hash table capacity

typedef struct entry{
    uint32_t key;
    uint32_t value;
} entry;

typedef struct level_bucket
{
    uint8_t token[SLOT_NUM];
    entry slot[SLOT_NUM];
} level_bucket;

typedef struct level_hash {
    level_bucket top_buckets[CAPACITY];
    level_bucket bottom_buckets[CAPACITY / 2];
} level_hash;

level_hash hash_table;
//int order;

void init_hash_table(void);
uint32_t F_IDX(uint32_t addr, uint32_t capacity);	// Get first index to use at table
uint32_t S_IDX(uint32_t addr, uint32_t capacity);	// Get second index to use at table
void delete_hash_table(uint32_t addr, int key); // 해시테이블에서 삭제하는 함수
void insert_hash_table(uint32_t addr, int key); // 해시테이블에 삽입하는 함수
int get_side(uint32_t addr, int *bucket_idx, int *slot_idx, int *loc); // f_idx, s_idx, top, bottom 어디에 삽입해야하는지 찾아주는 함수
void insert_value(int bucket_idx, int slot_idx, entry input, int loc); // 값을 구조체에 삽입해주는 함수
int check_opposite(int32_t addr, int *idx, int loc); // top, bottom 전부 꽉 찬 경우 주소값의 다른 해시함수의 slot에 옮기기 위한 함수 
void swap_entry(entry *a, entry *b, entry c); // top, bottom에 전부 꽉 찬 경우 값을 옮기고 새로운 값을 옮긴 자리에 삽입하는 함수

#endif
