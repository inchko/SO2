/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <myerror.h>
#include <system.h>
#include <entry.h>

#define USELESS 0
#define LECTURA 0
#define ESCRIPTURA 1
#define FIRST_FREE_PAGE (PAG_LOG_INIT_DATA + NUM_PAG_DATA)

int pid = 2; //0 y 1 son init e idle.
extern int time;
struct list_head readqueue;

int check_fd(int fd, int permissions)
{
  if (fd!=1) return 9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return 13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
 
	return current()->PID;
}

int return_from_fork(){
	return 0;
}


int sys_fork(){

	if (list_empty(&freequeue)) return -EAGAIN;
	struct list_head *e = list_first(&freequeue);
	list_del(e);
	struct task_struct *son = list_entry(e, struct task_struct, list);
	union task_union *unison = (union task_union*) son;
	struct task_struct *act = current();
	union task_union *uact = (union task_union*) act;
	copy_data(uact, unison, KERNEL_STACK_SIZE*sizeof(long));
	allocate_DIR(son);
	
	page_table_entry * process_PT =  get_PT(son);
	page_table_entry * process_APT =  get_PT(act);
	for (int i = 0; i < NUM_PAG_CODE; i++) 
		set_ss_pag(process_PT, PAG_LOG_INIT_CODE + i, get_frame(process_APT, PAG_LOG_INIT_CODE+i));
	for (int i = 0; i < NUM_PAG_KERNEL; i++)
		set_ss_pag(process_PT, i, get_frame(process_APT, i));

	int tmp[NUM_PAG_DATA];
	for(int i=0;i<NUM_PAG_DATA;i++){
		tmp[i]=alloc_frame();
		if(tmp[i]<0){
			for(int j=0;j<i;j++){ free_frame(tmp[j]);} //marcar com lliures els frames que habia marcat alloc
			list_add_tail(e, &freequeue);
			return -ENOMEM;
		}
	}	

	int page= PAG_LOG_INIT_DATA + NUM_PAG_DATA; 
	for(int k=0;k<NUM_PAG_DATA;k++){
		set_ss_pag(process_PT, PAG_LOG_INIT_DATA + k, tmp[k]);
		set_ss_pag(process_APT, page + k, tmp[k]); // assignació temporal
		copy_data((void*)((PAG_LOG_INIT_DATA + k)*PAGE_SIZE), (void*)((page+k)*PAGE_SIZE), PAGE_SIZE); //copiar les pagines del pare
		del_ss_pag(process_APT, page + k); //bye bye assignacio temp
		set_cr3(act->dir_pages_baseAddr); //flush tlb
	}

	son->PID=pid++;
	int ebp_offset = getEBP() & 0xfff;
	int new_ebp = ebp_offset + (int)son;

	unison->task.kernel_ebp = new_ebp - 1*sizeof(long);
	unison->stack[ebp_offset/sizeof(long) - 0] = (long)&return_from_fork;
	unison->stack[ebp_offset/sizeof(long) - 1] = 0;


	init_stat(son);
	list_add_tail(&son->list, &readyqueue);
	return son->PID;
}


int gettime(){ 
return zeos_ticks;
}




#define tambuff 4
int sys_write(int fd,char *buffer,int size){

 int num;
 int check;
 check= check_fd (fd,ESCRIPTURA);
 if(check!=0) return -check;
 if( buffer==NULL) return -EFAULT;
 if(size<0) return -EINVAL;
 char tmpbuf[tambuff];
 int acum = 0;
 while (acum + tambuff < size) {
	copy_from_user(buffer+acum, tmpbuf, tambuff);
	num= sys_write_console(tmpbuf, tambuff);
	acum += tambuff;
 }
 if (acum < size) {			//recollida de dades sobrants
	int resta = size - acum;
	copy_from_user(buffer+acum, tmpbuf, resta);
	num= sys_write_console(tmpbuf, resta);
 } 
 return 0;
}

struct task_struct *get_task_PID(int pid) {
	for (int i = 0; i < NR_TASKS; i++) {
		if ((&task[i])->task.PID == pid) return (struct task_struct *)&task[i];
	}
	return NULL;
}

int sys_get_stats(int pid, struct stats* st) {
	
	if(pid<0) return -EINVAL;//-PID
	if(st==NULL) return -EFAULT; //st null
	if(st<PH_USER_START) return -EFAULT; //st in kernel space
	if(st>= (PAG_LOG_INIT_DATA + NUM_PAG_DATA)*PAGE_SIZE) return -EFAULT; //st out of user space;
	struct task_struct *t = get_task_PID (pid);
	struct list_head * e;
	current()->pstats.remaining_ticks=time;
	if (t == NULL) return -ESRCH; // INVALID PID

 	if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EACCES; //comprovem si es pot escriure.

	copy_to_user(&t->pstats, st, sizeof(struct stats)); //perque st es un punter a espai user
	return 0;
}

int sys_clone(void (*function), void *stack){

	if (!access_ok(VERIFY_READ, function, 1)) { //address function correct
		return -EFAULT;
	}

	if (!access_ok(VERIFY_WRITE, stack, 1)) {//address stack correct
		return -EFAULT;
	}
    
	if (list_empty(&freequeue)) { //Si no hi han espais lliures
		return -EAGAIN;
	}
    	struct list_head * e = list_first( &freequeue );
	list_del(e);
	struct task_struct * pipo = list_entry( e, struct task_struct, list );
	pipo->PID=pid++;
	union task_union *pipo_u = (union task_union *) pipo;
	struct task_struct *curr_t = current();
	union task_union *curr_u = (union task_union*) curr_t;
	
	copy_data(curr_u, pipo_u, KERNEL_STACK_SIZE * sizeof(long));

	pipo_u->stack[KERNEL_STACK_SIZE - 2] = stack;
	pipo_u->stack[KERNEL_STACK_SIZE - 5] = function;

	int ebp_offset = getEBP() & 0xfff;
	int new_ebp = ebp_offset + (int)pipo;
	pipo->kernel_ebp = new_ebp - 1*sizeof(long);
	pipo_u->stack[ebp_offset/sizeof(long) - 0] = (long)&return_from_fork;
	pipo_u->stack[ebp_offset/sizeof(long) - 1] = 0;

	init_stat(pipo);
	free_dirs[current()->dir_pos]++;
	pipo->dir_pos=current()->dir_pos;
	list_add_tail(&pipo->list, &readyqueue);
	pipo->clone_pid=current()->PID;
	return pipo->PID;
}

void sys_exit(){ 

	if(free_dirs[current()->dir_pos]<=1)  { //nomes queda el procces orignial
		free_user_pages(current());
		free_dirs[current()->dir_pos]--;
	}
	update_process_state_rr(current(), &freequeue);
	//eliminació de sem associats
	for (int i = 0; i < NR_SEMAPHORES; i++){
		if (semaphores[i].owner == current()->PID)
			sys_sem_destroy(semaphores[i].id);
	}

	current()->PID=-1;

	sched_next_rr();
}
struct semaphore *get_sem(int id) {
	for (int i = 0; i < NR_SEMAPHORES; i++) 
		if (semaphores[i].id == id && (semaphores[i].owner != -1))
			return &semaphores[i];
	return NULL;
}

struct semaphore *get_free_sem() {
	for (int i = 0; i < NR_SEMAPHORES; i++) 
		if (semaphores[i].owner == -1)
			return &semaphores[i];
	return NULL;
}

int sys_sem_init(int id, unsigned int value){
	if (get_sem(id) != NULL) return -EBUSY; // already used
	
	struct semaphore *s = get_free_sem();
	if (s == NULL) return -1; // no free semaphores
	
	s->id = id;
	s->value = value;
	s->owner = current()->PID;


	INIT_LIST_HEAD(&s->blockqueue);

	return 0;
}

int sys_sem_wait(int id){
	struct semaphore *s = get_sem(id);
	if (s == NULL) return -EINVAL; // invalid sem
	//if (s->owner == -1) return -1;
	
	if (s->value > 0) {
		s->value--;
	} else {
		update_process_state_rr(current(), &s->blockqueue);
		sched_next_rr();
		if( s->owner == -1) return  -1;
	}

	return 0;
}

int sys_sem_signal(int id){
	struct semaphore *s = get_sem(id);
	if (s == NULL) return -EINVAL; // invalid sem

	if (list_empty(&s->blockqueue)) {
		s->value++;
	} else {
		struct task_struct *t = list_head_to_task_struct(list_first(&s->blockqueue));
		update_process_state_rr(t, &readyqueue);
		}

	return 0;
}



int sys_sem_destroy(int id){
	struct semaphore *s = get_sem(id);
	if (s == NULL) return -EINVAL; // invalid sem
	if (s->owner != current()->PID) return -EPERM; 
	while (!list_empty(&s->blockqueue)) {
		struct task_struct *t = list_head_to_task_struct(list_first(&s->blockqueue));
		update_process_state_rr(t, &readyqueue);
		//s->id=-1;
	}
	s->owner = -1;
	s->id=-1;
	return 0;
}



void * sys_sbrk(int increment){
	
	int brkc = current()->brk;
	if(brkc+increment>=MAX_HEAP) return -ENOMEM;
	int unchanged= ff_page();
	
	current()->brk+=increment;
	
	if(current()->brk<0) current()->brk=0;
	page_table_entry *pt=get_PT(current());
	int changed = ff_page();
	int diff = changed-unchanged;
	
	for (int l = 0; l < diff; l++) {
		int heaps[diff];
		for(int i=0;i<diff;i++){
			heaps[i]= alloc_frame();
			if(heaps[i]<0) {
				for(int j=0;j<i;j++){
					free_frame(heaps[j]);
				}
				return -ENOMEM;
			}
		}
		for( int k = 0; k < diff; k++){
			set_ss_pag(pt,unchanged+k,heaps[k]);
		}
	}
	//delete pages
	for(int i=0;i<-diff;i++){
		free_frame(pt[unchanged-i-1].bits.pbase_addr);
		pt[unchanged-i-1].entry=0;
	}
	set_cr3(current()->dir_pages_baseAddr);
	
	return INIT_HEAP*PAGE_SIZE+brkc; 
	
	
}
