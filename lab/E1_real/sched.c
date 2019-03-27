/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <libc.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

struct element {

};

struct task_struct * idle_task;

struct task_struct * ini_task;

struct list_head freequeue;

struct list_head readyqueue;

extern struct list_head blocked;

struct task_struct * idle_task;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}

int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
		;
	}
}


void init_idle (void)
{
	struct list_head * e = list_first( &freequeue );
	idle_task = list_entry(e, struct task_struct, list); // Get the task_struct
	idle_task->PID = 0; // Assign a PID
	allocate_DIR(idle_task); // Assign memory pages
	list_del(e); // Delete form freequeue
	union task_union *task_union_idle_task = ( union task_union *) idle_task; // Get the task_union of the task

	task_union_idle_task->stack[KERNEL_STACK_SIZE- 1] = (unsigned long)&cpu_idle; // Set the return addr
	task_union_idle_task->stack[KERNEL_STACK_SIZE - 2] = 0;

	idle_task->kernel_esp = (int)&task_union_idle_task->stack[KERNEL_STACK_SIZE - 2];
}

void update_esp(int esp) {
  	tss.esp0 = esp;
	writeMSR(0x175, esp);
}

void init_task1(void)
{
	struct list_head * e = list_first( &freequeue );
	list_del(e);
	ini_task = list_entry(e, struct task_struct, list);

	ini_task-> PID = 1; // Assing PID
	allocate_DIR(ini_task); // Assign memory pages
	set_user_pages(ini_task); // set 

	union task_union *u = (union task_union *) ini_task;

	update_esp((int) &u->stack[KERNEL_STACK_SIZE]);

	set_cr3(ini_task->dir_pages_baseAddr);
}


void init_sched(){
	INIT_LIST_HEAD( &freequeue );
	INIT_LIST_HEAD( &readyqueue );

	for(int i = 0; i < NR_TASKS; ++i){
		struct task_struct *t = (struct task_struct *) &task[i];

		t->quantum = DEFAULT_QUANTUM;
		t->PID = i+2;
		t->state = -1;

		list_add_tail( &t->list, &freequeue );
	}
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void inner_task_switch(union task_union*new){
	update_esp((int) &new->stack[KERNEL_STACK_SIZE]);
	set_cr3(new->task.dir_pages_baseAddr);
	inner_inner_task_switch(&(new->task.kernel_esp), &(current()->kernel_esp));
}

void set_quantum(struct task_struct *t){
	t->quantum = DEFAULT_QUANTUM;
}

int get_quantum(struct task_struct *t){
	return t->quantum;
}

void update_sched_data_rr(){
	current->execution_time++;
}

int needs_sched_rr(){
	return (current->execution_time >= current->quantum);
}

void update_process_state_rr(struct task_struct *t, struct list_head *dest){

}