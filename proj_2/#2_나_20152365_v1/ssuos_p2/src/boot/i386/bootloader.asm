; 8개 범용 register
; AX (Accumulaor register) : 산술 연산에 사용
; CX (Counter register) : 시프트/회전 연산과 루프에서 사용
; DX (Data register) : 산술연산과 I/O 명령에서 사용
; BX (Base register) : 데이터의 주소를 가르키는 포인터
; SP (Stack Pointer register) : 스택의 최상단을 가르키는 포인터로 사용
; BP (Stack Base Pointer register) : 스택의 베이스를 가리키는 포인터로 사용
; SI (Source Index register) : 스트림 명령에서 소스를 가리키는 포인터로 사용
; DI (Destination Index register) : 스트림 명령에서 도착점을 가르키는 포인터로 사용

org	0x7c00   

[BITS 16]

START:
		cli		; Clear Interrupt Flag
		mov		byte[partition_num], 0 ; byte[partition], 0으로 초기화 ; 키보드로 위치 설정하기 위해
REPEAT: ; je(jump) 후 되돌아오기 위한 레이블
		call	cls
		cmp		byte[partition_num], 0 ; kernel 선택 커서가 ssuos_1에 있는지 비교
		je		T1 ; 있을 경우
		jne		T2 ; 아닌 경우
FIRST:
		cmp		byte[partition_num], 1 ; ssuos_2에 있는지 비교
		je		T3 ; 있을 경우
		jne		T4 ; 아닌 경우
SECOND:
		cmp		byte[partition_num], 2 ; ssuos_3에 있는지 비교
		je		T5 ; 있을 경우
		jne		T6 ; 아닌 경우
THIRD:
		call	INPUT ; 키보드 입력을 받기 위한 함수 호출

		jmp		$

; (T1, T2):ssuos_1, (T3, T4):ssuos_2, (T5, T6):ssuos_3

; 선택되어 있는 경우
T1:
		mov		bx, select ; 출력하기 위해 bx에 select 대입
		call	print_string ; select 출력
		mov		bx,	ssuos_1 ; ssuos_1을 bx에 대입
		add		bx, 3 ; bx를 3칸 옮겨서 괄호를 지움
		call	print_string ; ssuos_1을 출력
		jmp		FIRST ; 호출 부분으로 되돌아감

; 선택되어 있지 않은 경우
T2:
		mov		bx, ssuos_1 ; 출력을 위해 ssuos_1을 bx에 대입
		call	print_string ; 출력
		jmp		FIRST ; 함수 호출부로 되돌아감

T3:
		mov		bx, select
		call	print_string
		mov		bx,	ssuos_2
		add		bx, 3
		call	print_string
		jmp		SECOND

T4:
		mov		bx, ssuos_2
		call	print_string
		jmp		SECOND
T5:
		mov		bx, select
		call	print_string
		mov		bx,	ssuos_3
		add		bx, 3
		call	print_string
		jmp		THIRD

T6:
		mov		bx, ssuos_3
		call	print_string
		jmp		THIRD

; 화면을 지우기 위한 함수
cls:
		pusha
		mov ah, 0x00
		mov al, 0x03
		int 0x10
		popa
		ret

INPUT:
		; keyboard input을 interrupt로 받는다
		mov		ah, 00h
		int		16h

		cmp		ah,0x48 ; up key
		je		up ; up키 함수 호출

		cmp		ah,0x4B ; left key
		je		left ; left키 함수 호출
		
		cmp		ah,0x4D ; right key
		je		right ; right키 함수 호출
		
		cmp		ah,0x50 ; down key
		je		down ; down키 함수 호출

		cmp		ah,0x1C ; enter key
		je		enter ; enter키 함수 호출

		ret

; 방향키 윗 키가 가능 한 경우는 ssuos_3에 커서(byte[partition_num])이 2인 경우만 가능
up:
		cmp		byte[partition_num], 2 ; 커서 위치 확인
		je		up_1 ; 커서 위치가 ssuos_3에 있다면
		jne		REPEAT ; 없다면 REPEAT LABLE(출력부)로 다시 돌아감
up_1:
		mov		byte[partition_num], 0 ; 커서 위치를 ssuos_1로 옮김
		jmp		REPEAT

; 방향키 왼쪽 키를 눌렀을 때 ssuos_1에 있다면 위치 이동 x
left:
		cmp		byte[partition_num], 0  ; ssuos_1에 커서가 있는지 검사
		je		left_1 ; 맞다면
		jne		left_2 ; 아니라면
left_1:
		jmp		REPEAT ; 이동 X
left_2:
		dec		byte[partition_num] ; byte[partition_num]의 값을 1 줄인다
		jmp		REPEAT

; 방향키 오른쪽을 눌렀을 때 ssuos_3에 커서가 있다면 이동 x
right:
		cmp		byte[partition_num], 2 ; 커서 위치 확인
		je		right_1 ; 커서가 ssuos_3에 있다면
		jne		right_2 ; 아니라면
right_1:
		jmp		REPEAT ; 이동 X
right_2:
		inc		byte[partition_num] ; 커서 위치를 1을 증가시킴
		jmp		REPEAT

; 방향키 아래키를 누를 수 있는 경우는 ssuos_1에 있는 경우
down:
		cmp		byte[partition_num], 0 ; 커서 위치 확인
		je		down_1 ; ssuos_1에 있는 경우
		jne		REPEAT ; 아닌 경우
down_1:
		mov		byte[partition_num], 2 ; 커서 위치를 ssuos_3로 옮김
		jmp		REPEAT

; enter키를 눌렀을때 kernel load를 위해 BOOT1_LOAD로 점프
enter:
		jmp BOOT1_LOAD
		
; 문자열 출력 함수
print_string:
	pusha	; register에 있는 값들을 전부 push
CON:
	; 문자열을 하나씩 출력하기 위한 반복문
	loop:
		mov		al,	[bx] ; 출력하기 위해 bx에 char를 al로 옮김
		cmp		al,	0 ; 0인지 확인 ; 끝인지 확인
		je		return ; 끝이면 종료
		push	bx ; bx 값을 push ; 저장
		mov		ah,	0x0e ; 한 글자 출력 
		int		0x10 ; 출력하기 위한 인터럽트 설정
		pop		bx ; push했던 bx pop
		inc		bx ; bx한 칸 옮김
		jmp		loop ; 반복
		
return:
		popa ; 끝났으므로 pop all
		ret ; 호출부로 돌아감

BOOT1_LOAD:
		mov     ax, 0x0900 ; AX 레지스터에 0x0900의 값을 복사
        mov     es, ax ; es 레지스터(Extra Segment, 세그먼트 레지스터)에 0x0900의 값을 복사
        mov     bx, 0x0 ; bx에 0x0값 복사

        mov     ah, 2 ; 0x13 인터럽트 호출시 ah에 저장된 값에 따라 수행되는 결과가 다름. 2는 섹터 읽기
        mov     al, 0x4	; al 읽을 섹터 수를 지정, 1~128 사이의 값을 지정 가능
        mov     ch, 0 ; 실린더 번호 cl의 상위 2비트까지 사용가능하여 표현
        mov     cl, 2 ; 읽기 시작할 섹터의 번호 1~18 사이의 값, 1에는 부트로더가 있으니 2 이상부터
        mov     dh, 0 ; 읽기 시작할 헤드 번호 1~15의 값
        mov     dl, 0x80 ; 드라이버 번호. 0x00 - 플로피 ; 0x80 - 첫 번째 하드, 0x81 - 두 번째 하드

		; 0x13 인터럽트 ; XM SIMD Floating Point Exception; 0x13 인터럽트는 SIMD 부동 소수점 예외에 사용되는 인터럽트이다.
		; SIMD(Single instruction Multiple Data) 병령 프로세서의 한 종류, 하나의 명령어로 여러 개의 값을 동시에 계산하는 방식
		; CPU칩에 들어있는 SSE나 SSE2 유닛에서 부동 소수점을 연산할 때 에러 상황이 발생하여 이를 알려준 경우
        int     0x13 ; 0x13 인터럽트 호출
        jc      BOOT1_LOAD
		
		; 커널을 로드하기 전에 커서 위치 확인
		; 커서위치에 맞는 커널 로드
		cmp		byte[partition_num], 0 
		je		KERNEL_LOAD_1

		cmp		byte[partition_num], 1
		je		KERNEL_LOAD_2

		cmp		byte[partition_num], 2
		je		KERNEL_LOAD_3

; cylinders == 6, heads == 16, sectors == 63
; cylinders = LBA / (heads * sectors)
; head = (LBA / sector) % heads
; sector = (LBA % sector) + 1
; cylinder가 0일땐 +1을 하지 않음 

; cylinder = 6/16 * 63 = 0
; head = (6/63) % 16 = 0
; sector = (6 % 63) = 6
KERNEL_LOAD_1:
		mov     ax, 0x1000	
        mov     es, ax		
        mov     bx, 0x0		

        mov     ah, 2		; 디스크에 있는 데이터를 es:bx의 주소를 넣는다	
        mov     al, 0x3f	; 63만큼 섹터를 읽는다
        mov     ch, 0x00	; 0번째 실린더 ; 실린더 : 00
        mov     cl, 0x06	; 6번째 섹터부터 읽기 시작한다 ; 섹터 : 06
        mov     dh, 0X00	; 헤드는 0 ; 헤드 : 00
        mov     dl, 0x80	; 첫 번째 하드드라이브

        int     0x13 ; BIOS interrupt
        jc      KERNEL_LOAD_1 ; bios interrupt 실패시 재 실행
		jmp		LAST

; cylinder = 10000 / (16*63) = 9
; head = (10000/63) % 16 = 14
; sector = (10000%63) + 1 = 47
KERNEL_LOAD_2:
		mov     ax, 0x1000
        mov     es, ax		
        mov     bx, 0x0		

        mov     ah, 2		; 디스크에 있는 데이터를 es:bx의 주소를 넣는다	
        mov     al, 0x3f	; 63만큼 섹터를 읽는다
        mov     ch, 0x09	; 09번째 실린더
        mov     cl, 0x2F	; 47번째 섹터부터 읽기 시작한다 ; 47
        mov     dh, 0X0E	; 헤드는 0 ; 14
        mov     dl, 0x80	; 첫 번째 하드드라이브

        int     0x13
        jc      KERNEL_LOAD_2
		jmp		LAST

; cylinder = 15000 / (16*63) = 14
; head = (15000/63) % 16 = 14
; sector = (15000%63) + 1 = 7
KERNEL_LOAD_3:
		mov     ax, 0x1000
        mov     es, ax		
        mov     bx, 0x0		

        mov     ah, 2		; 디스크에 있는 데이터를 es:bx의 주소를 넣는다	
        mov     al, 0x3f	; 0x3f만큼 섹터를 읽는다
        mov     ch, 0x0E	; 14번째 실린더
        mov     cl, 0x07	; 7번째 섹터부터 읽기 시작한다 ; 7
        mov     dh, 0X0E	; 헤드는 0 ; 14
        mov     dl, 0x80	; 첫 번째 하드드라이브

        int     0x13
        jc      KERNEL_LOAD_3
		jmp		LAST

; real mode 에서 protected mode 전환을 위한 far jump
LAST:
jmp		0x0900:0x0000

select db "[O]", 0

ssuos_1 db "[] SSUOS_1",09h,09h, 0 ; 문자열 밑 빈 탭 출력
ssuos_2 db "[] SSUOS_2",0Dh,0Ah,0 ; 문자열 출력 밑 개행
ssuos_3 db "[] SSUOS_3",0 ; 문자열 출력 밑 끝을 표시 (0)
partition_num : resw 1

times   446-($-$$) db 0x00

PTE:														; LBA 0x0006 == 6
partition1 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
															; LBA 0x2710 == 10000
partition2 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
															; LBA 0x3a98 == 15000
partition3 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x98, 0x3a, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
partition4 db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
times 	510-($-$$) db 0x00
dw	0xaa55
