#include <device/io.h>
#include <mem/mm.h>
#include <mem/paging.h>
#include <device/console.h>
#include <proc/proc.h>
#include <interrupt.h>
#include <mem/palloc.h>
#include <ssulib.h>
#include <mem/hashing.h>

uint32_t F_IDX(uint32_t addr, uint32_t capacity) {
	return addr % ((capacity / 2) - 1);
}

uint32_t S_IDX(uint32_t addr, uint32_t capacity) {
	return (addr * 7) % ((capacity / 2) - 1) + capacity / 2;
}

void init_hash_table(void)
{
	// TODO : OS_P5 assignment
	memset(&hash_table, 0, sizeof(struct level_hash)); // 해시 테이블 초기화
}

void delete_hash_table(uint32_t addr, int key) // 해시 테이블 삭제 함수
{
	int f_idx = F_IDX(addr, CAPACITY); // 주소에 대한 해시함수 결과를 미리 저장
	int s_idx = S_IDX(addr, CAPACITY);

	for (int i=0; i<SLOT_NUM; i++) {
		// input으로 들어온 key 값을 각 해쉬함수의 top (F_IDX, S_IDX), bottom (F_IDX, S_IDX)에서 찾아준다.
		if (hash_table.top_buckets[f_idx].slot[i].key == key) { 
			// 삭제 문구를 출력하고 key, token 초기화
			printk("hash value deleted : idx : %d, key : %d, value : %x\n", f_idx, hash_table.top_buckets[f_idx].slot[i].key, hash_table.top_buckets[f_idx].slot[i].value);
			hash_table.top_buckets[f_idx].slot[i].key = 0;
			hash_table.top_buckets[f_idx].token[i] = 0;
			return ;
		}
		else if (hash_table.top_buckets[s_idx].slot[i].key == key) {
			printk("hash value deleted : idx : %d, key : %d, value : %x\n", s_idx, hash_table.top_buckets[s_idx].slot[i].key, hash_table.top_buckets[s_idx].slot[i].value);
			hash_table.top_buckets[s_idx].slot[i].key = 0;
			hash_table.top_buckets[s_idx].token[i] = 0;
			return ;
		}
		else if (hash_table.bottom_buckets[f_idx/2].slot[i].key == key) {
			printk("hash value deleted : idx : %d, key : %d, value : %x\n", f_idx/2, hash_table.bottom_buckets[f_idx/2].slot[i].key, hash_table.bottom_buckets[f_idx/2].slot[i].value);
			hash_table.bottom_buckets[f_idx/2].slot[i].key = 0;
			hash_table.bottom_buckets[f_idx/2].token[i] = 0;
			return ;
		}
		else if (hash_table.bottom_buckets[s_idx/2].slot[i].key == key) {
			printk("hash value deleted : idx : %d, key : %d, value : %x\n", s_idx/2, hash_table.bottom_buckets[s_idx/2].slot[i].key, hash_table.bottom_buckets[s_idx/2].slot[i].value);
			hash_table.bottom_buckets[s_idx/2].slot[i].key =0;
			hash_table.bottom_buckets[s_idx/2].token[i] = 0;
			return ;
		}
	}
}

void insert_hash_table(uint32_t addr, int key) // 해시 테이블 insert 함수
{
	int f_idx = F_IDX(addr, CAPACITY); // 주소에 대한 각 해시함수의 결과값을 미리저장
	int s_idx = S_IDX(addr, CAPACITY);
	int bucket_idx; // top bucket이 꽉 찼는지 검사하는 함수의 결과를 받기 위한 변수
	int slot_idx;
	int loc=-1;

	entry input = {key, VH_TO_RH(addr)}; // input으로 받는 key와 가상주소를 변환한 물리 주소를 함수간 이동의 편의를 위해 임의의 구조체 안에 넣음.

	if (get_side(addr, &bucket_idx, &slot_idx, &loc) != -1) // 삽입 가능한지 찾음. 가능하다면 포인터를 통해 삽입 가능한 위치를 리턴받음
		insert_value(bucket_idx, slot_idx, input, loc); // 데이터 삽입
	else
		move(addr, input); // 데이터 삽입이 불가능한 경우라면 슬롯이동 후 데이터 삽입
}

void move(uint32_t addr, entry input) { // 데이터 삽입이 불가능한 경우에 슬롯이동과 이동한 자리에 데이터 삽입을 하는 함수
	int f_idx = F_IDX(addr, CAPACITY); // 각 해시함수의 결과를 미리 받아온다
	int s_idx = S_IDX(addr, CAPACITY);
	int bucket_idx;
	int slot_idx;
	int loc;
	int result;

	bucket_idx = f_idx; // 반대편위치를 받아오기 위한 변수
	for (int i=0; i<SLOT_NUM; i++) { // 반대편 (다른 해시함수)를 검사해서 빈자리가 있다면 삽입 // 이과정을 F_IDX, S_IDX, top, bottom 총 네번 수행
		if ((slot_idx = check_opposite(RH_TO_VH(hash_table.top_buckets[f_idx].slot[i].value), &bucket_idx, 0)!=-1)) {
			swap_entry(&(hash_table.top_buckets[f_idx].slot[i]), &(hash_table.top_buckets[bucket_idx].slot[slot_idx]), input);
			hash_table.top_buckets[f_idx].token[i]=1; // 옮기고 삽입한 위치에 token 1
			hash_table.top_buckets[bucket_idx].token[slot_idx]=1;  // 토큰 표시
			printk("hash value inserted in top level : idx : %d, key : %d, value : %x\n", f_idx, input.key, input.value); // 완료 문구 출력
			return ; // 수행 완료 후 종료
		}
	}
	// s_idx 쪽 find
	bucket_idx = s_idx;
	for (int i=0; i<SLOT_NUM; i++) {
		if ((slot_idx = check_opposite(RH_TO_VH(hash_table.top_buckets[s_idx].slot[i].value), &bucket_idx, 0)!=-1)) {
			swap_entry(&(hash_table.top_buckets[s_idx].slot[i]), &(hash_table.top_buckets[bucket_idx].slot[slot_idx]), input);
			hash_table.top_buckets[s_idx].token[i]=1;
			hash_table.top_buckets[bucket_idx].token[slot_idx]=1;
			printk("hash value inserted in top level : idx : %d, key : %d, value : %x\n", s_idx, input.key, input.value);
			return ;
		}
	}
	// bottom, f_idx쪽 find
	bucket_idx = f_idx/2;
	for (int i=0; i<SLOT_NUM; i++) {
		if ((slot_idx = check_opposite(RH_TO_VH(hash_table.bottom_buckets[f_idx/2].slot[i].value), &bucket_idx, 1)!=-1)) {
			swap_entry(&(hash_table.bottom_buckets[f_idx/2].slot[i]), &(hash_table.bottom_buckets[bucket_idx].slot[slot_idx]), input);
			hash_table.bottom_buckets[f_idx/2].token[i]=1;
			hash_table.bottom_buckets[bucket_idx].token[slot_idx]=1;
			printk("hash value inserted in bottom level : idx : %d, key : %d, value : %x\n", f_idx/2, input.key, input.value);
			return ;
		}
	}
	// bottom, s_idx쪽 find
	bucket_idx = s_idx/2;
	for (int i=0; i<SLOT_NUM; i++) {
		if ((slot_idx = check_opposite(RH_TO_VH(hash_table.bottom_buckets[s_idx/2].slot[i].value), &bucket_idx, 1)!=-1)) {
			swap_entry(&(hash_table.bottom_buckets[s_idx/2].slot[i]), &(hash_table.bottom_buckets[bucket_idx].slot[slot_idx]), input);
			hash_table.bottom_buckets[s_idx/2].token[i]=1;
			hash_table.bottom_buckets[bucket_idx].token[slot_idx]=1;
			printk("hash value inserted in bottom level : idx : %d, key : %d, value : %x\n", s_idx/2, input.key, input.value);
			return ;
		}
	}
}

int check_opposite(int32_t addr, int *idx, int loc) { // 반대편 (다른 해시함수)쪽을 체크해주는 함수
	int f_idx = F_IDX(addr, CAPACITY);
	int s_idx = S_IDX(addr, CAPACITY);
	int slot_idx;

	if ((loc==0 && *idx == f_idx) || (loc==1 && *idx == f_idx/2)) { // 가득 차있는 쪽의 반대편 검사를 위한 조건문 // F_IDX, S_IDX 한번씩 검사
		for (int i=0; i<SLOT_NUM; i++) // 매개변수로 들어온 top or bottom 쪽을 검사
			if ((hash_table.top_buckets[s_idx].token[i]==0 && loc==0) ||
					(hash_table.bottom_buckets[s_idx/2].token[i]==0 && loc==1)) {
				*idx = s_idx; // 찾았다면 bucket idx 포인터에 위차 할당
				return i; // slot idx 리턴
			}
	}
	else if ((loc==0 && *idx == s_idx) || (loc==1 && *idx == s_idx/2)) {
		for (int i=0; i<SLOT_NUM; i++)
			if ((hash_table.top_buckets[f_idx].token[i]==0 && loc==0) ||
					(hash_table.bottom_buckets[f_idx/2].token[i]==0 && loc==1)) {
				*idx = f_idx;
				return i;
			}
	}
	return -1;
}

// 한 쪽 버켓이 다 찼을 경우 찬 쪽의 버켓을 빈 쪽으로 옮기고 새 값을 옮겨서 빈 위치에 넣음
void swap_entry(entry *a, entry *b, entry c) // a : 옮길 값, b : 빈 공간, c : 새로 들어오는 값
{
	entry t = *a; // 값 저장
	*b = t; // b에 a를 넣음
	*a = c; // a에 새로운 값 넣음
}
// 어떤 해시 함수를 써야하는지 결정하기 위해 주소에 대한 각 해시함수의 위치에 버켓이 몇개나 차있는지 count
int get_side(uint32_t addr, int *bucket_idx, int *slot_idx, int *loc) { // addr, top_bucket : loc->0, bottom_bucket : loc->1
	int f_idx = F_IDX(addr, CAPACITY);
	int s_idx = S_IDX(addr, CAPACITY);
	// top buckets의 f_idx, s_idx 검사 이후 찾지 못한다면 bottom 검사
	for (int i=0; i<SLOT_NUM; i++) {
		// 빈 공간을 찾음
		if (hash_table.top_buckets[f_idx].token[i]==0)
			*bucket_idx = f_idx;
		else if (hash_table.top_buckets[s_idx].token[i]==0) {
			*bucket_idx = s_idx;
		}
		else 
			continue;
		// 찾은 경우 위치를 저장 후 리턴
		*slot_idx = i;
		*loc = 0;
		return 0;
	}
	// bottom에서 찾음
	for (int i=0; i<SLOT_NUM; i++) {
		// 빈 공간을 찾음
		if (hash_table.bottom_buckets[f_idx/2].token[i]==0)
			*bucket_idx = f_idx;
		else if (hash_table.bottom_buckets[s_idx/2].token[i]==0)
			*bucket_idx = s_idx;
		else
			continue;
		// 찾은 경우 위치 저장후 리턴
		*slot_idx = i;
		*loc = 1;
		return 0;
	}

	return -1;
}

// bucket idx, slot idx, entry, top or bottom 을 매개변수로 받아서 값을 테이블에 넣는 함수
void insert_value(int bucket_idx, int slot_idx, entry input, int loc)
{ 
	if (loc == 0) { // top bucket의 경우 token에 표시 및 값 할당 후 출력
		hash_table.top_buckets[bucket_idx].token[slot_idx] = 1;
		hash_table.top_buckets[bucket_idx].slot[slot_idx].key = input.key;
		hash_table.top_buckets[bucket_idx].slot[slot_idx].value = input.value;
		printk("hash value inserted in top level : idx : %d, key : %d, value : %x\n", bucket_idx, input.key, input.value);
	} // bottom bucket의 경우 token에 표시 및 값 할당 후 출력
	else {
		hash_table.bottom_buckets[bucket_idx/2].token[slot_idx] = 1;
		hash_table.bottom_buckets[bucket_idx/2].slot[slot_idx].key = input.key;
		hash_table.bottom_buckets[bucket_idx/2].slot[slot_idx].value = input.value;
		printk("hash value inserted in bottom level : idx : %d, key : %d, value : %x\n", bucket_idx/2, input.key, input.value);
	}
}
