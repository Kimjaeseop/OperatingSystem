org	0x9000  

[BITS 16]  

		cli		; Clear Interrupt Flag

		mov     ax, 0xb800 ; ax 레지스터에 0xb800값 복사 ; 비디오 메모리 세그먼트를 복사
        mov     es, ax ; es 레지스터에 ax 레지스터 값 복사; es 레지스터 메모리 주소지정을 다루는 스트링 연산에서 사용된다
        mov     ax, 0x00 ; ax 레지스터 0x00 으로 초기화
        mov     bx, 0 ; bx 레지스터 0으로 초기화
        mov     cx, 80*25*2 ; cx register를 사용하여 화면크기만큼 루프를 돌린다

; 피연산자가 지시한 포트로 AX의 데이터를 출력시킨다.
CLS:	
		; ex를 전부 0으로 초기화 ; loop 구조
        mov     [es:bx], ax
        add     bx, 1 ; bx를 1씩 늘려간다. 
        loop    CLS 

Initialize_PIC:
		;ICW1 - 두 개의 PIC를 초기화 
		mov		al, 0x11
		out		0x20, al
		out		0xa0, al

		;ICW2 - 발생된 인터럽트 번호에 얼마를 더할지 결정
		mov		al, 0x20
		out		0x21, al
		mov		al, 0x28
		out		0xa1, al

		;ICW3 - 마스터/슬레이브 연결 핀 정보 전달
		mov		al, 0x04
		out		0x21, al
		mov		al, 0x02
		out		0xa1, al

		;ICW4 - 기타 옵션 
		mov		al, 0x01
		out		0x21, al
		out		0xa1, al

		mov		al, 0xFF
		;out		0x21, al
		out		0xa1, al

; 포트 초기화
Initialize_Serial_port:
		xor		ax, ax ; ax의 값을 0으로 바꿈
		xor		dx, dx ; dx의 값을 0으로 바꿈

		; port 초기화
		mov		al, 0xe3 ; al에 0xe3을 넣어서 초기화
		int		0x14 ; 직렬 포트 서비스 인터럽트

READY_TO_PRINT:
		xor		si, si ; si 레지스터 초기화
		xor		bh, bh ; bh 레지스터 초기화
; 시리얼에 출력
PRINT_TO_SERIAL:
		mov		al, [msgRMode+si] ; msgRMode db의 주소를 al에 넣는다
		mov		ah, 0x01 ; 읽을 섹터수 1로 지정 ; send to character
		int		0x14 ; 시리얼 포트 접근 인터럽트
		add		si, 1 ; si에 1 더함 ; 다음문자 출력
		cmp		al, 0 ;  다 출력했는지 검사해서 loop(jne)
		jne		PRINT_TO_SERIAL
PRINT_NEW_LINE: ; line_feed(개행 출력)
		mov		al, 0x0a ; 개행 아스키코드 (hex) LF
		mov		ah, 0x01 ; 추적 플래그가 설정되어 있는 동안 모든 명령을 마친 후 실행된다
		int		0x14 ; 직렬 포트 서비스 인터럽트
		mov		al, 0x0d ; CR
		mov		ah, 0x01 
		int		0x14

; A20 Gate 활성화 ; A20 Gate는 AND GATE이다.
Activate_A20Gate:
		mov		ax,	0x2401 ; A20 Gate 활성화
		int		0x15 ; 기타 시스템 서비스 인터럽트

;Detecting_Memory:
;		mov		ax, 0xe801
;		int		0x15

PROTECTED:
        xor		ax, ax ; ax를 0으로 초기화
        mov		ds, ax ; ds에 ax값 넣는다.

		call	SETUP_GDT ; GDT descriptor를 불러오기 위한 콜

		; pe 비트 설정, 보호모드 전환에 필요
		; cr0에는 보호모드 전환에 관련된 필드, 캐시, 페이징 등에 관련된 필드가 있다.
		; cr0 비트는 cpu에게 protected mode로 전환됨을 알린다.
        mov		eax, cr0 ; eax에 cr0를 넣는다
        or		eax, 1	; cr0 비트에 set 해주기 위한 작업  
        mov		cr0, eax  ; protected mode 활성화

		; protected mode 전환은 되었으니 cpu에는 파이프라이닝으로 인해 16비트의 명령어가 남아있을 수 있다. jmp로 코드를 clear한다.
		jmp		$+2 ; 현재 주소에서 2를 더한만큼 점프
		nop			; 아무 작업 X
		nop
		; 32비트로 넘어가기 위한 작업
		jmp		CODEDESCRIPTOR:ENTRY32

; lgdt : Load Global Descriptor Table
; GDT : Global Descriptor Table
SETUP_GDT:
		lgdt	[GDT_DESC] ; load GDT descriptor
		ret ; return

[BITS 32]  
; 세그먼트 셀렉터 - CS, DS, SS, ES, FS, GS ...
; 세그먼트 레지스터를 이용해 디스크립터 테이블에서 세그먼트 디스크립터를 선택하기에 세그먼트 셀렉터라고 부른다. 디스크립터 데이블이 세그먼트를 가르키는 역할.
; 세그먼트 셀렉터가 바로 세그먼트를 가리키는 것이 아니라 세그먼트를 정의하는 세그먼트 디스크립터를 가르킨다.

ENTRY32:
		mov		ax, 0x10 ; 보호모드 커널용 32bit data segment descriptor를 AX 레지스터에 SAVE
		mov		ds, ax ; DS 세그먼트 셀렉터에 설정
		mov		es, ax ; ES 세그먼트 셀렉터에 설정
		mov		fs, ax ; FS 세그먼트 셀렉터에 설정
		mov		gs, ax ; GS 세그먼트 셀렉터에 설정

		; 스택을 0x00000000 ~ 0x0000FFFF 영역에 64KB 크기로 생성
		mov		ss, ax
		; ESP 레지스터의 어드레스를 0XFFFE로 설정
  		mov		esp, 0xFFFE
		; EBP 레지스터의 어드레스를 0XFFFE로 설정
		mov		ebp, 0xFFFE	
		; edi에 한 컬럼의 크기 (80 * 2) 할당
		mov		edi, 80*2
		; esi에 msgPMode의 주소값 할당
		lea		esi, [msgPMode]
		; 출력함수 호출
		call	PRINT

		;IDT TABLE
		; 메모리상에 IDT 테이블 생성
	    cld
		mov		ax,	IDTDESCRIPTOR
		mov		es, ax
		xor		eax, eax
		xor		ecx, ecx
		mov		ax, 256 ; IDT영역에 256개의 디스크립터 복사
		mov		edi, 0
 
IDT_LOOP:
		lea		esi, [IDT_IGNORE] ; 디스크립터의 샘플이 있는 주소를 넣는다.
		mov		cx, 8 ; 디스크립터 하나는 8바이트 씩
		rep		movsb ; cx에 8이 들어가 있으므로 8바이트가 DS:ESI -> ES:EDI의 방향으로 복사
		dec		ax ; ax 하나 감소
		jnz		IDT_LOOP
		
		; IDT 프로세스레 로드
		lidt	[IDTR]
		; 인터럽트 활성화
		sti
		jmp	CODEDESCRIPTOR:0x10000 ; 0x08로 점프 32bit jmp 세그먼트:오프셋

PRINT: ; 32bit 보호모드에서 문자열 출력을 위한 함수
		push	eax ; 각 레지스터 push
		push	ebx
		push	edx
		push	es
		mov		ax, VIDEODESCRIPTOR ; es 레지스터에 0x18(비디오 디스크립터 저장)
		mov		es, ax
PRINT_LOOP:
		or		al, al ; or 값 확인
		jz		PRINT_END ; or 값이 0이라면 jump
		mov		al, byte[esi] ; 문자열을 하나씩 0x07 (검정 배경에 흰 글씨로 출력)하는 loop 구조
		mov		byte [es:edi], al
		inc		edi
		mov		byte [es:edi], 0x07

OUT_TO_SERIAL:
		mov		bl, al
		mov		dx, 0x3fd ; 라인 상태 레지스터를 dx(데이터 레지스터)에 넣는다.
CHECK_LINE_STATUS:
		in		al, dx ; input from port 명령어로 dx 레지스터 (0x3fd)값을 al 레지스터에 받아온다
		and		al, 0x20 ; al에 들어있는 문자가 space인 경우 (0x20 : space ascii) al에 1을 넣음
		cmp		al, 0 ; 윗 문장이 false라면 재 호출
		jz		CHECK_LINE_STATUS
		mov		dx, 0x3f8 ; 첫 번째 시리얼 포트의 I/O 기준 어드레스, 0x3f8을 넣는다. ; 시리얼 디버그 준비 상태, COM1 SERIAL PORT
		mov		al, bl 
		out		dx, al

		inc		esi ; 다음문자 출력
		inc		edi
		jmp		PRINT_LOOP
PRINT_END:
LINE_FEED:
		mov		dx, 0x3fd ; 라인 상태 레지스터를 통해 출력하기 위해 0x3fd를 넣는다.
		in		al, dx 
		and		al, 0x20 ; 빈칸인지 확인 후 아니라면 재호출
		cmp		al, 0
		jz		LINE_FEED
		mov		dx, 0x3f8 ; 
		mov		al, 0x0a
		out		dx, al ; line feed serial port 출력
CARRIAGE_RETURN: ; 캐리지 리턴 ; 커서를 현재 행의 맨 좌측으로 옮김
		mov		dx, 0x3fd ; 시리얼 포트 가져옴 (I/O PORT)
		in		al, dx 
		and		al, 0x20 ; 0x20 인지 확인
		cmp		al, 0
		jz		CARRIAGE_RETURN
		mov		dx, 0x3f8 ; 시리얼 포트
		mov		al, 0x0d ; carriage return ascii code
		out		dx, al ; 

		; 각 레지스터들 pop
		pop		es
		pop		edx
		pop		ebx
		pop		eax
		; 호출부로 return 
		ret

GDT_DESC:
		; dw 명령어 : 메모리를 word 배열로 보낸다
		; GDT : 첫 번째 GDT의 위치
		; GDT_END : GDT의 끝을 나타냄
		; GDT 끝 위치 - GDT 시작 위치 - 1 == GDT의 LIMIT
        dw GDT_END - GDT - 1    
		; GDT 메모리를 double word 배열로 보낸다
        dd GDT                 

; GDT 테이블 작성 (정의)
GDT:
		NULLDESCRIPTOR equ 0x00 ; NULL descriptor 작성, 0으로 초기화해야 한다
			dw 0 
			dw 0 
			db 0 
			db 0 
			db 0 
			db 0
		; code descriptor 작성
		CODEDESCRIPTOR  equ 0x08 ; code segment descriptor 의 이름을 CODEDESCRIPTOR로 지정
			dw 0xffff ; Limit offset 0~15
			dw 0x0000 ; base address 0~15
			db 0x00  ; base address 16~25
			db 0x9a  ; P=1, DPL=0, 코드 세그먼트, Execute/Read                   
			db 0xcf  ; G=1, D=1, L=0, Limit 16~19              
			db 0x00  ; Base 24~31              
		; 보호모드 커널용 데이터 세그먼트 디스크립터 작성
		DATADESCRIPTOR  equ 0x10
			dw 0xffff ; Limit offset 0~15            
			dw 0x0000 ; 시작주소             
			db 0x00 ; base address 16~25               
			db 0x92	; P=1, DPL=0, 데이터 세그먼트, Read/Write                
			db 0xcf ; G=1, D=1, L=0, Limit 16~19               
			db 0x00 ; Base 24~31              
		; 비디오 디스크립터 작성 
		VIDEODESCRIPTOR equ 0x18
			dw 0xffff ; Limit offset 0~15             
			dw 0x8000 ; 시작주소              
			db 0x0b ; base address               
			db 0x92 ; P=1, DPL=0, 데이터 세그먼트, 읽기/쓰기 가능           
			db 0x40 ; 0x0, G:1, D:1                    
			;db 0xcf                    
			db 0x00	; Base 24~31
		; IDT 디스크립터 작성
		IDTDESCRIPTOR	equ 0x20
			dw 0xffff ; Limit offset 0~15
			dw 0x0000 ; 시작주소
			db 0x02 ; base address
			db 0x92 ; P=1, DPL=0,
			db 0xcf ; G=1, D=1, L=0 Limit 16~19
			db 0x00 ; Base 24~31
GDT_END:
; CPU가 인터럽트 걸렸을 경우 IDT를 참조 할 수 있도록 IDTR 레지스터에 등록
IDTR:
		dw 256*8-1 ; IDT의 LIMIT SIZE
		dd 0x00020000 ; IDT의 BASE ADDRESS
IDT_IGNORE:
		dw ISR_IGNORE ; 핸들러의 오프셋, 인터럽트가 수행될 오프셋이다.
		dw CODEDESCRIPTOR ; 2바이트 코드 세그먼트
		db 0 ; 1바이트 0x0
		db 0x8E ; type
		dw 0x0000 ; idt handler offset

; Interrupt Service Routine ; Interrupt 처리 루틴
; 콘텍스트의 저장하고 복구하는 역할
ISR_IGNORE:
		; register, Flag storage (CPU 레지스터 값, FLAG를 스택에 저장)
		push	gs 
		push	fs
		push	es
		push	ds
		pushad
		pushfd
		cli
		nop
		sti
		; CPU register 값 복구
		popfd 
		popad
		pop		ds
		pop		es
		pop		fs
		pop		gs
		iret



msgRMode db "Real Mode", 0
msgPMode db "Protected Mode", 0

 
times 	2048-($-$$) db 0x00
