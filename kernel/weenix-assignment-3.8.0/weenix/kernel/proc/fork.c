/******************************************************************************/
/* Important Spring 2022 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"
#include "mm/kmalloc.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}



static int
vmarea_consistent(vmmap_t* parent_map, vmmap_t* child_map){
	vmarea_t*vma = NULL;
	list_iterate_begin(&parent_map->vmm_list, vma, vmarea_t, vma_plink) {
		vmarea_t *cvma = vmmap_lookup(child_map, vma->vma_start);
		KASSERT(vma->vma_start==cvma->vma_start);
		KASSERT(vma->vma_end==cvma->vma_end);
		KASSERT(vma->vma_off==cvma->vma_off);
		KASSERT(vma->vma_prot==cvma->vma_prot);
		KASSERT(vma->vma_flags==cvma->vma_flags);
		KASSERT(vma->vma_vmmap!=cvma->vma_vmmap);
		KASSERT(vma->vma_obj->mmo_refcount==cvma->vma_obj->mmo_refcount);
		KASSERT(vma->vma_obj->mmo_nrespages==cvma->vma_obj->mmo_nrespages);
		if (vma->vma_obj->mmo_shadowed){
			KASSERT(vma->vma_obj->mmo_shadowed->mmo_refcount - vma->vma_obj->mmo_shadowed->mmo_nrespages >= 2 );
			KASSERT(vma->vma_obj->mmo_un.mmo_bottom_obj==cvma->vma_obj->mmo_un.mmo_bottom_obj);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} else {
			KASSERT((cvma->vma_flags&MAP_SHARED)>0);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		if ((vma-> vma_flags & MAP_PRIVATE)) {
			KASSERT(vma->vma_obj!=cvma->vma_obj);
			KASSERT(vma->vma_obj->mmo_shadowed==cvma->vma_obj->mmo_shadowed);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} else{
			KASSERT(vma->vma_obj==cvma->vma_obj);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		dbg(DBG_PRINT, "(GRADING3A)\n");
	} list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return 1;
}


static void 
init_child_proc_thread(proc_t* proc, kthread_t*thr, struct regs *regs){
	/*Set up the new process thread context (kt_ctx). You will need to set the following:
	c_pdptr - the page table pointer
	c_eip - function pointer for the userland_entry() function
	c_esp - the value returned by fork_setup_stack()
	c_kstack - the top of the new thread's kernel stack
	c_kstacksz - size of the new thread's kernel stack*/
	memcpy(thr->kt_kstack, curthr->kt_kstack, curthr->kt_ctx.c_kstacksz);
	void (*userlandPtr)(const regs_t *) = &userland_entry;
	regs_t* childReg = (regs_t*)kmalloc(sizeof(regs_t));
	memcpy(childReg, regs, sizeof(regs_t));	
	childReg->r_eax = 0; //all of reg should be the same except for eax child return value
	thr->kt_ctx.c_eip = (uint32_t) userlandPtr;
	thr->kt_ctx.c_esp = fork_setup_stack(childReg, thr->kt_kstack);
	thr->kt_ctx.c_pdptr = proc->p_pagedir;
	thr->kt_proc = proc;
	// thr->kt_retval = 0;
	list_insert_tail(&proc->p_threads, &thr->kt_plink);
	//Copy the file descriptor table of the parent into the child. Remember to use fref() here.
	for (int i = 0; i < 32; i++) {
		if (curproc->p_files[i]) {
			fref(curproc->p_files[i]);
			proc->p_files[i] = curproc->p_files[i];
			dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
		}
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}
	// NO NEED SET WORKING DIRECTORY SINCE IT's ALREADY DONE in proc_create
	//Set the child's working directory to point to the parent's working directory (once again, remember reference counts).
	// proc->p_cwd = curproc->p_cwd;
	// vref(curproc->p_cwd);
	
	//Set any other fields in the new process which need to be set.
	proc->p_pproc = curproc;
	proc->p_brk = curproc->p_brk;
	proc->p_start_brk = curproc->p_start_brk;
	dbg(DBG_PRINT, "(GRADING3A)\n");
	kfree(childReg);
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        vmarea_t *vma, *cvma;
        pframe_t *pf;
        mmobj_t *temp, *new_shadowed;
		KASSERT(regs != NULL); /* the function argument must be non-NULL */
		dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
		KASSERT(curproc != NULL); /* the parent process, which is curproc, must be non-NULL */
		dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
		KASSERT(curproc->p_state == PROC_RUNNING); /* the parent process must be in the running state and not in the zombie state */
		dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

		//Allocate a proc_t out of the procs structure using proc_create().
		//just let child name = its pid
		char str[80];
		sprintf(str, "child-%d", curproc->p_pid); 
		proc_t * child = proc_create(&str[0]);
		//Copy the vmmap_t from the parent process into the child using vmmap_clone()
		child->p_vmmap = vmmap_clone(curproc->p_vmmap);
		//increase the reference counts on the underlying mmobj_t 
		// Increment the ref count of the top-most layer mmobj of parent vmareas
		// for weenix, vmareas are ordered: stack ->text -> data+bss -> heap
		list_iterate_begin(&curproc->p_vmmap->vmm_list, vma, vmarea_t, vma_plink) {
			vmarea_t *cvma = vmmap_lookup(child->p_vmmap, vma->vma_start);
			if ((vma->vma_flags & MAP_PRIVATE)) {
				//add child shadow object 
				new_shadowed = shadow_create();
				new_shadowed->mmo_un.mmo_bottom_obj = vma->vma_obj->mmo_un.mmo_bottom_obj;
				new_shadowed->mmo_shadowed = vma->vma_obj;
				vma->vma_obj->mmo_ops->ref(vma->vma_obj);
				cvma->vma_obj = new_shadowed;
				
				//now process parent vma 
				//Be careful with reference counts. Also note that for shared mappings, there is no need to copy the mmobj_t.
				new_shadowed = shadow_create();
				new_shadowed->mmo_un.mmo_bottom_obj = vma->vma_obj->mmo_un.mmo_bottom_obj;
				new_shadowed->mmo_shadowed = vma->vma_obj;
				vma->vma_obj = new_shadowed;
				
				list_insert_tail(&vma->vma_obj->mmo_un.mmo_bottom_obj->mmo_un.mmo_vmas, &cvma->vma_olink);
				//This is how you know that the pages corresponding to this mapping are copy-on-write. 
				dbg(DBG_PRINT, "(GRADING3A)\n");
			} else {
				cvma->vma_obj = vma->vma_obj;
				cvma->vma_obj->mmo_ops->ref(cvma->vma_obj);
				list_insert_tail(&vma->vma_obj->mmo_un.mmo_vmas, &cvma->vma_olink);
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} list_iterate_end();
		/*Unmap the user land page table entries and flush the TLB (using pt_unmap_range() and tlb_flush_all()). 
		This is necessary because the parent process might still have some entries marked as "writable", 
		but since we are implementing copy-on-write we would like access to these pages to cause a trap.*/
		/* Flush the process pagetables and TLB */
        pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();
		vmarea_consistent(curproc->p_vmmap, child->p_vmmap);
		KASSERT(child->p_pagedir != NULL); /* new child process must have a valid page table */
		dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
		//Use kthread_clone() to copy the thread from the parent process into the child process.
		kthread_t *ckthread = kthread_clone(curthr);
		init_child_proc_thread(child, ckthread, regs);
		KASSERT(child->p_state == PROC_RUNNING); /* new child process starts in the running state */
		KASSERT(ckthread->kt_kstack != NULL); /* thread in the new child process must have a valid kernel stack */
		dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
		//Make the new thread runnable.
		sched_make_runnable(ckthread);
		//return parents
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return child->p_pid;
}
