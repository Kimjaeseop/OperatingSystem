org		0x7c00

[BITS 16]

START:   
mov		ax, 0xb800 ; ax 레지스터에 0xb800 값 복사 ; 비디오 메모리(화면에 출력하기 위함) 세그먼트를 복사
mov		es, ax ; es 레지스터에 ax 레지스터 값 (0xb800)을 복사
mov		ax, 0x00 ; ax 레지스터에 0x00 값 복사 ; background를 검정색으로 바꿈
mov		bx, 0 ; bx 레지스터에 0 값 복사 ; 0x10000에 커널 로드 (Base register)
mov		cx, 80*25*2 ; counter register ; cx의 값만큼 loop가 돈다. ; 화면의 크기인 가로 80자 * 세로 25자 * 2 byte만큼 loop를 돌린다.
mov		di, 0 ; destination index register; 스트림 명령에서 도착점을 가르키는 포인터
mov		cx, 0x7FF ; cx를 한개씩 줄여가며 메모리상에서 다 썼는지 확인.
jmp		CLS

CLS:
mov		[es:bx], ax
add		bx, 1
loop 	CLS

mov		edi, 0 ; edi를 0으로 초기화
mov		byte[es:edi], 'H' ; es에 'H'를 넣음 ; H 출력
inc		edi ; edi 값 1증가 ; 주소를 한 칸 뒤로 옮
mov		byte[es:edi], 0x07 ; 글씨 색을 흰색으로 지정
inc		edi
mov		byte[es:edi], 'e'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'l'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'l'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'o'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], ','
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], ' '
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'J'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'a'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'e'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 's'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'e'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'o'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'p'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 96
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 's'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], ' '
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'W'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'o'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'r'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'l'
inc		edi
mov		byte[es:edi], 0x07
inc		edi
mov		byte[es:edi], 'd'
inc		edi
mov		byte[es:edi], 0x07
inc		edi



jmp		$ ; 계속 이곳으로 점프 (현재) ; while (1) 과 같은 무한루프 구조
