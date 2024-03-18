@---------------------------------------------------------------------------------
	.section ".init"
	.global _start
@---------------------------------------------------------------------------------
	.align	4
	.arm
@---------------------------------------------------------------------------------
_start:
@---------------------------------------------------------------------------------
	@ Jump to firmware if in GBA mode
	ldr		r0, =0x04000136		@ if X_KEYS == 0
	ldrb	r0, [r0]		@ Read the byte
	cmp		r0, #0x00
	bne		nds_mode
	
gba_mode:
	mov		r0, #0x50		@ Switch to user mode
	msr		cpsr, r0
	mov		sp, #0x03000000		@ Set stack pointer
	add		sp, sp, #0x7f00
	mov		r0, #0x02000000		@ Original firmware loader start address
	bx		r0			@ Jump back to firmware

nds_mode:	
	@ Copy program to expected location
	@ DMA part 1 to SIWRAM
	.equ	PART_1_START,	0x02101100
	.equ	PART_1_SIZE, 0x1FC0
	mov		r0, #0x04000000		@ DMA3
	ldr		r1, =PART_1_START
	str		r1, [r0, #0xD4]		@ src
	ldr		r1, =_start		@ location complied for
	str		r1, [r0, #0xD8]		@ dest
	ldr		r1, =1<<31 | PART_1_SIZE>>1
	str		r1, [r0, #0xDC]		@ control
	@ DMA part 2 to SIWRAM
	.equ	PART_2_START,	0x021030E0
	.equ	PART_2_SIZE, 0xD20
	mov		r0, #0x04000000		@ DMA3
	ldr		r1, =PART_2_START
	str		r1, [r0, #0xD4]		@ src
	ldr		r1, =_start + PART_1_SIZE		@ location complied for
	str		r1, [r0, #0xD8]		@ dest
	ldr		r1, =1<<31 | PART_2_SIZE>>1
	str		r1, [r0, #0xDC]		@ control
	
	@ Jump to correct memory location
	ldr		r0, =_correct_start
	bx		r0
	_correct_start:
	
	mov	r0, #0x04000000		@ IME = 0;
	add	r0, r0, #0x208
	strh	r0, [r0]

	mov	r0, #0x12		@ Switch to IRQ Mode
	msr	cpsr, r0
	ldr	sp, =__sp_irq		@ Set IRQ stack

	mov	r0, #0x13		@ Switch to SVC Mode
	msr	cpsr, r0
	ldr	sp, =__sp_svc		@ Set SVC stack

	mov	r0, #0x1F		@ Switch to System Mode
	msr	cpsr, r0
	ldr	sp, =__sp_usr		@ Set user stack

	ldr	r0, =__bss_start	@ Clear BSS section to 0x00
	ldr	r1, =__bss_end
	sub	r1, r1, r0
	bl	ClearMem

	ldr	r3, =_init		@ global constructors
	bl	_call_via_r3

	mov	r0, #0			@ int argc
	mov	r1, #0			@ char *argv[]
	ldr	r3, =main
	bl	_call_via_r3		@ jump to user code
		
	@ If the user ever returns, return to flash cartridge
	mov	r0, #0x08000000
	bx	r0

@---------------------------------------------------------------------------------
@ Clear memory to 0x00 if length != 0
@  r0 = Start Address
@  r1 = Length
@---------------------------------------------------------------------------------
ClearMem:
@---------------------------------------------------------------------------------
	mov	r2, #3			@ Round down to nearest word boundary
	add	r1, r1, r2		@ Shouldn't be needed
	bics	r1, r1, r2		@ Clear 2 LSB (and set Z)
	bxeq	lr			@ Quit if copy size is 0

	mov	r2, #0
ClrLoop:
	stmia	r0!, {r2}
	subs	r1, r1, #4
	bne	ClrLoop
	bx	lr

@---------------------------------------------------------------------------------
@ Copy memory if length	!= 0
@  r1 = Source Address
@  r2 = Dest Address
@  r4 = Dest Address + Length
@---------------------------------------------------------------------------------
CopyMemCheck:
@---------------------------------------------------------------------------------
	sub	r3, r4, r2		@ Is there any data to copy?
@---------------------------------------------------------------------------------
@ Copy memory
@  r1 = Source Address
@  r2 = Dest Address
@  r3 = Length
@---------------------------------------------------------------------------------
CopyMem:
@---------------------------------------------------------------------------------
	mov	r0, #3			@ These commands are used in cases where
	add	r3, r3, r0		@ the length is not a multiple of 4,
	bics	r3, r3, r0		@ even though it should be.
	bxeq	lr			@ Length is zero, so exit
CIDLoop:
	ldmia	r1!, {r0}
	stmia	r2!, {r0}
	subs	r3, r3, #4
	bne	CIDLoop
	bx	lr

@---------------------------------------------------------------------------------
	.align
	.pool
	.end
@---------------------------------------------------------------------------------
