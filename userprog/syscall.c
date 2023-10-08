#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "userprog/process.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f)
{	struct thread* curr = thread_current();
	// TODO: Your implementation goes here.
	// printf ("system call!\n");

	switch (f->R.rax)
	{
	case SYS_WRITE:
		// f->R.rax = call_write(f->R.rsi, f->R.rdx);
		// printf("rid, rsi, rdx : %d, %p, %d\n", f->R.rdi, f->R.rsi, f->R.rdx);
		f->R.rax = call_write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_EXIT:
		call_exit(curr, f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = call_create(f->R.rdi, f->R.rsi);
		break;
	case SYS_HALT:
		call_halt();
		break;
	case SYS_OPEN:
		f->R.rax = call_open(f->R.rdi);
		break;
	case SYS_CLOSE:
		call_close(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = call_read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_FILESIZE:
		f->R.rax = call_filesize(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = call_wait(f->R.rdi);
		break;
	case SYS_FORK:
		memcpy(&curr->ptf, f, sizeof(struct intr_frame));
		f->R.rax = call_fork(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = call_exec(f->R.rdi);
		break;
	case SYS_REMOVE:
		f->R.rax = call_remove(f->R.rdi);
		break;
	case SYS_SEEK:
		call_seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = call_tell(f->R.rdi);
		break;



	default:
		call_exit(curr,-1);
		break;
	}
}

void call_exit(struct thread *curr, uint64_t status)
{	
	curr->exit_status = status;
	
	curr->tf.R.rax = status;
	// printf("%s: exit(%d)\n", curr->name, status);
	
	thread_exit();
	return;
}


int call_write(int fd, const void *buffer, unsigned size)
{
	check_addr(buffer);
	
	if (fd == 1) // fd 0 : 표준입력, fd 1 : 표준 출력
	{
		putbuf(buffer, size);
		return size;
	}
	if(fd == 0){
		return -1;
	}
	struct file *file = find_file_by_Fd(fd);
	if(file == NULL){
		call_exit(thread_current(), -1);
	}
	
	if (!file->deny_write) {	
		return 0;
	}

	return file_write(file, buffer, size);
}
bool call_create(const char *file, unsigned initial_size)
{
	check_addr(file);	
	return filesys_create(file, initial_size);
}

void call_halt(void)
{
	power_off();
}

int call_open(const char *file)
{
	

	check_addr(file);
	struct file *open_file = filesys_open(file);

	if (open_file == NULL)
	{
		return -1;
	}

	int fd = add_file_to_fdt(open_file);

	// fd table 가득 찼다면
	if (fd == -1)
	{
		file_close(open_file);
	}
	return fd;
}

void call_close(int fd)
{

	struct thread *cur = thread_current();
	struct file *file = find_file_by_Fd(fd);

	if (file == NULL)
	{
		call_exit(thread_current(),-1);
		return;
	}
	file_close(file);
	cur->fd_table[fd] = NULL;
}

int call_read(int fd, void *buffer, unsigned size)
{
	check_addr(buffer);

	if(fd == 1){
		return -1;
	}
	if(fd == 0){
		int byte = input_getc();
		return byte;
	}


	struct file *file = find_file_by_Fd(fd);
	int read_result;
	
	
	if (file == NULL)
	{
		call_exit(thread_current(),-1);
		return -1;
	}
	else
	{
		read_result = file_read(find_file_by_Fd(fd), buffer, size); // TODO : lock걸어야함
	}

	return read_result;
}

int call_wait (tid_t child_tid) {
	return process_wait(child_tid);
}

int call_filesize(int fd)
{
	struct file *file = find_file_by_Fd(fd);

	if (file == NULL)
	{
		return 0;
	}

	return file_length(file);
}

int call_fork (const char *thread_name){
	check_addr(thread_name);
	return process_fork(thread_name, &thread_current()->ptf);
}

int call_exec (const char *file){
	char *file_copy;
	check_addr(file);
	// int a = 0;


	file_copy = palloc_get_page(0);
	// file_copy = file

	strlcpy(file_copy, file, strlen(file)+1);
	
	return process_exec(file_copy);;
}


//--------------
bool call_remove(const char *file){

	check_addr(file);

	return filesys_remove(file);
}

void call_seek(int fd, unsigned new_pos){
	struct file *cur = thread_current()->fd_table[fd];
	
	if(cur != NULL){
		file_seek(cur, new_pos);
	}
}

unsigned call_tell(int fd){
	struct file *cur = thread_current()->fd_table[fd];
	
	if(cur != NULL){
		return file_tell(cur);
	}
}
//----------

// fd값을 return, 실패시 -1을 return
// void exit(int status)
// {
// 	struct thread *curr = thread_current();
// 	curr->exit_status = status;
// 	curr->tf.R.rax = status;
// 	thread_exit();
// }

int add_file_to_fdt(struct file *file)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;
	// fd의 위치가 제한 범위를 넘지않고, fdtable의 인덱스 위치와 일치하면 fd index += 1
	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx])
	{
		cur->fd_idx++;
	}

	// fdt가 가득 찼다면 -1 return
	if (cur->fd_idx >= FDCOUNT_LIMIT)
	{
		return -1;
	}

	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}

struct file *find_file_by_Fd(int fd)
{
	struct thread *cur = thread_current();

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	{
		return NULL;
	}
	return cur->fd_table[fd];
}

void check_addr(const uint64_t *addr)
{
	struct thread *curr = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(curr->pml4, addr) == NULL)
	{
		//exit(-1);
		call_exit(curr, -1);
	}
}

