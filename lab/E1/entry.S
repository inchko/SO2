/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include <zeos_error.h>


/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %ebx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %edx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %edx; \
      pushl %ecx; \
      pushl %ebx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define RESTORE_ALL \
	pop %ebx; \
	pop %ecx; \
	pop %edx; \
	pop %esi; \
	pop %edi; \
	pop %ebp; \
	pop %eax; \
	pop %ds; \
	pop %es; \
	pop %fs; \
	pop %gs; \
	
#define EOI      \
	movb $0x20, %al; \
	outb %al, $0x20 ;
	

ENTRY(keyboard_handler)
	SAVE_ALL
	EOI //End Of Interrupt
	call keyboard_RSI;
	RESTORE_ALL
	iret;

ENTRY(clock_handler)
	SAVE_ALL;
	EOI; //End Of Interrupt
	call clock_RSI;
	RESTORE_ALL;
	iret;

ENTRY(system_call_handler)
	SAVE_ALL
	cmpl $0, %eax;
	jl err;
	cmpl $MAX_SYSCALL, %eax;
	jg err;
	call *sys_call_table(, %eax, 0x04); //call syscall from sys_call_table
	jmp fin;
err:
	movl $-38, %eax;
fin:
	movl %EAX, 0x18(%esp);
	RESTORE_ALL
	iret;

ENTRY(writeMSR)
	push %ebp;
	movl %esp,%ebp;
	movl 12(%ebp), %eax;
	movl 8(%ebp), %ecx;
	wrmsr;
	pop %ebp;
	ret;

ENTRY(syscall_handler_sysenter)
	push $__USER_DS;
	push %EBP;
	// User stack addr
	pushfl;
	push $__USER_CS;
	push 4(%EBP);
	// User return addr
	SAVE_ALL
	cmpl $0, %EAX ;// Test the error
	jl sysenter_err;
	cmpl $MAX_SYSCALL, %EAX;
	jg sysenter_err;
	call *sys_call_table(, %EAX, 0x04); // Call syscall form sys_call_table
	jmp sysenter_fin;
sysenter_err:
	movl $-ENOSYS, %EAX;
sysenter_fin:
	movl %EAX, 0x18(%ESP);
	RESTORE_ALL
	movl (%ESP), %EDX;
	// Return address
	movl 12(%ESP), %ECX;// User stack addr
	sti; // Enable interrupts
	sysexit
