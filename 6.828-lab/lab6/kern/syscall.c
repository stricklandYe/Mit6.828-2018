/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, 0);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0

	// LAB 4: Your code here.

	// cprintf("syscall.c:enter sys_exofork \n");
	struct Env* child;
	int ret_value;
	if((ret_value = env_alloc(&child,curenv->env_id)) < 0) {
		return ret_value ;
	}
	child->env_status = ENV_NOT_RUNNABLE;
	child->env_tf = curenv->env_tf;
	// cprintf("ip:%x\n",curenv->env_tf.tf_eip);
	child->env_tf.tf_regs.reg_eax = 0;
	return child->env_id;
	// panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	int ret_value;
	struct Env* proc;
	if ((ret_value = envid2env(envid,&proc,1)) < 0) {
		return -E_BAD_ENV;
	}
	if(status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE) {
		return -E_INVAL;
	}
	proc->env_status = status;
	return 0;
	// panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	// struct Env *env;
	// int r; //根据ID获得进程
	// if ((r = envid2env(envid,&env,1)) < 0) {
	// 	return -E_BAD_ENV;
	// }
	// user_mem_assert(env,(const void*) tf, sizeof(struct Trapframe),PTE_U);
    // env->env_tf = *tf;
    // env->env_tf.tf_cs |= 0x3; 
    // env->env_tf.tf_eflags &=  (~FL_IOPL_MASK);
    // env->env_tf.tf_eflags |= FL_IF;
	// return 0;
	// panic("sys_env_set_trapframe not implemented");
		struct Env *env;
	int r;
	if ( (r = envid2env(envid, &env, 1)) < 0)
		return r;

	// 什么时候会出现没有权限访问的问题？
	user_mem_assert(env, tf, sizeof(struct Trapframe), PTE_U);
	// 直接整个结构体也是可以赋值的
	
	// tf->tf_cs |= (GD_UT | 0x3); 
	tf->tf_eflags |= FL_IF;
	// 普通进程无I/O权限
	tf->tf_eflags &=  ~FL_IOPL_MASK;
	
	env->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* e;
	if(envid2env(envid,&e,1) < 0) {
		return -E_BAD_ENV;
	}
	e->env_pgfault_upcall = func;
	return 0;
	// panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.

	if((uintptr_t)va >= UTOP || PGOFF(va)) {
		return -E_INVAL;
	}
	struct Env* env;
	int ret_value;
	if ((ret_value = envid2env(envid,&env,1)) < 0) {
		return -E_BAD_ENV;
	}
	if(!(perm & PTE_SYSCALL)) {
		return -E_INVAL;
	}
	struct PageInfo* new_page = page_alloc(ALLOC_ZERO);
	if(new_page == NULL) {
		return -E_NO_MEM;
	}
	if( (ret_value = page_insert(env->env_pgdir,new_page,va,perm)) < 0) {
		page_free(new_page);
		return ret_value;
	}
	return 0;
	// panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	struct Env *src_env,*dst_env;
	int ret_value;
	pte_t*pg_table_entry;
	struct PageInfo* page;

	// if srcenvid or dstenvid does not currently exist
	if( envid2env(srcenvid,&src_env,1) < 0 ||envid2env(dstenvid,&dst_env,1) < 0) {
		return -E_BAD_ENV;
	}
	//return -E_INVAL if srcva >= UTOP or srcva is not page-aligned, or dstva >= UTOP or dstva is not page-aligned.
	if((uintptr_t)srcva >= UTOP || PGOFF(srcva) != 0 || (uintptr_t)dstva >= UTOP || PGOFF(dstva) != 0 ) {
		return -E_INVAL;
	}
	// -E_INVAL if perm is inappropriate (see sys_page_alloc).
	if((perm | PTE_SYSCALL) != PTE_SYSCALL) {
		return -E_INVAL;
	}

	page = page_lookup(src_env->env_pgdir,srcva,&pg_table_entry);
	// 	return -E_INVAL is srcva is not mapped in srcenvid's address space.
	if(page == NULL) {
		return -E_INVAL;
	}

	if((perm & PTE_W) && !(*pg_table_entry & PTE_W)) {
		return -E_INVAL;
	}
	// return -E_NO_MEM if there's no memory to allocate any necessary page tables
	if(page_insert(dst_env->env_pgdir,page,dstva,perm) < 0) {
		return -E_NO_MEM ;
	}
	return 0;

	// panic("sys_page_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env* env;
	if(envid2env(envid,&env,1) < 0) {
		return -E_BAD_ENV;
	}
	if( (uintptr_t)va >= UTOP || PGOFF(va)) {
		return -E_INVAL;
	}
	page_remove(env->env_pgdir,va);
	return 0;
	// panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	int result;
	struct Env* proc; //目标进程
	uint32_t src_addr = (uint32_t)srcva; // the page  address that will be sent to target process
	struct PageInfo *page; //phyiscal page 
	pte_t *pg_table_entry; // page table entry
	if ((result = envid2env(envid, &proc, 0)) < 0)
		return result;	
	if(proc->env_ipc_recving == 0) {
		// sys_ipc_recv()中设置了recving=1来表明这个进程想接受数据, 如果target process的recving == 0
		// 说明target process并不想接收数据，所以return -E_IPC_NOT_RECV;
		return -E_IPC_NOT_RECV;
	}
	if(src_addr < UTOP) {
		if (PGOFF(srcva) > 0) {
			//not page-aligned
            return -E_INVAL;
		}
        if ((perm | PTE_SYSCALL) != PTE_SYSCALL) {
			//perm is inapporiate
            return -E_INVAL;  
		}  
		page = page_lookup(curenv->env_pgdir, srcva, &pg_table_entry);
		if (page == NULL ) {
			//-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
			//address space.
            return -E_INVAL; 
		}
		if ((perm & PTE_W) && !(*pg_table_entry & PTE_W)){
			// if (perm & PTE_W), but srcva is read-only in the
			// current environment's address space.
			//(*pg_table_entry & PTE_W)如果这个条件成立，说明是writetable的
			//所以如果我们要判断它是否是一个read-only的，只需要！即可
            return -E_INVAL;
		} 
		if(proc->env_ipc_dstva) {
			// 如果src_addr < UTOP,才可以使用页来传递数据
			//接下来要做的在目标进程插入页,这样就完成了页的共享.
			//proc->env_ipc_dstva是进程自己设置好的,它表明期望将数据接受到哪里
			result = page_insert(proc->env_pgdir, page, proc->env_ipc_dstva, perm);
			if(result < 0) {
				//no avaiable memory 
				return -E_NO_MEM;
			}
			proc->env_ipc_perm = perm;
		}
	}
	proc->env_ipc_recving = 0; //表示接受完毕
	proc->env_ipc_value = value;
	proc->env_ipc_from = curenv->env_id;
	proc->env_status = ENV_RUNNABLE; //接收数据完毕后设置为RUNNABLE，接受调度
	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	// panic("sys_ipc_recv not implemented");
	uint32_t dst_addr = (uint32_t)dstva;
	if(dst_addr < UTOP && PGOFF(dst_addr) > 0) {
		// not page-aligned, return -E_INVAL;
		return -E_INVAL;
	}
	curenv->env_ipc_recving = 1; // 表示当前进程正在接受信息
	curenv->env_ipc_dstva = dstva; //表明想接收数据到dstva这个虚拟地址
	curenv->env_status = ENV_NOT_RUNNABLE; // block until a message has been received
	sched_yield();
	return 0;
    // return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	panic("sys_time_msec not implemented");
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{

	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	switch (syscallno) {
	case SYS_cputs:
		sys_cputs((char *)a1, a2);
		return 0;
	case SYS_cgetc:
		return sys_cgetc();
	case SYS_getenvid:
		return sys_getenvid();
	case SYS_env_destroy:
		return sys_env_destroy(a1);
	case SYS_page_alloc:
		return sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
	case SYS_page_map:
		return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
	case SYS_page_unmap:
		return sys_page_unmap((envid_t)a1, (void *)a2);
	case SYS_exofork:
		return sys_exofork();
	case SYS_env_set_status:
		return sys_env_set_status((envid_t)a1, (int)a2);
	case SYS_env_set_pgfault_upcall:
		return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
	case SYS_yield:
		sys_yield();
		return 0;
	case SYS_ipc_try_send:
		return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
	case SYS_ipc_recv:
		return sys_ipc_recv((void *)a1);
	case SYS_env_set_trapframe:
		return sys_env_set_trapframe((envid_t)a1,(struct Trapframe*)a2);		
	default:
		return -E_INVAL;
}
}


