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
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page. Make sure that if the
 * user writes to the page it will be handled correctly. This
 * includes your shadow objects' copy-on-write magic working
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        // NOT_YET_IMPLEMENTED("VM: handle_pagefault");
		dbg(DBG_TEMP, "Pagefault trying to address %p\n", (void*) vaddr);
		// NOT SURE what below two faults are. Also not sure if 'cause' can have multiple bits at once
		// in kernel FAQ, it says anything that's "reserved" means that it's not suppose to be used
		// so just kill process
		if (cause & FAULT_RESERVED || cause & FAULT_EXEC){
			dbg(DBG_TEMP, "Accessed invalid memory address %p\n", (void*) vaddr);
			dbg(DBG_PRINT, "(GRADING3D)\n");
			proc_kill(curproc, EFAULT);
		}
		if (vaddr < USER_MEM_LOW || vaddr >= USER_MEM_HIGH) {
			dbg(DBG_TEMP, "Accessed invalid memory address %p\n", (void*) vaddr);
			dbg(DBG_PRINT, "(GRADING3D)\n");
			proc_kill(curproc, EFAULT);
		}

        // Before you can do anything you need to find the vmarea that
        // contains the address that was faulted on. 
		pagedir_t *cur_pd = pt_get();
		pframe_t *tgt_pframe = NULL;
        vmarea_t * tgt_vmarea = vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(vaddr));

		if (tgt_vmarea==NULL){
			dbg(DBG_TEMP, "Accessed invalid memory address %p\n", (void*) vaddr);
			dbg(DBG_PRINT, "(GRADING3D)\n");
			proc_kill(curproc, EFAULT);
		}
		// The PTE bits/flags we should modify are
		// PT_PRESENT: whether can be used for addr translation, seems to be validity bit
		// PT_WRITE: 
		// PT_USER:
		// PT_ACCESSED: Seems to be reference bit. CPU sets it to 1 before a read or write operation to a page.
		// PT_DIRTY:
		uint32_t flags_to_add = PT_PRESENT | PT_USER;

		// FAULT_PRESENT 0 => pagefault caused by not-present page
		// FAULT_PRESENT 1 => pagefault caused by a page-level protection violation
		if ((cause & FAULT_PRESENT) == 0) {
			if (!(tgt_vmarea->vma_prot & PROT_READ)) {
				dbg(DBG_TEMP, "Trying to read an address that does not allow read-access %p\n", (void*) vaddr);
				dbg(DBG_PRINT, "(GRADING3D)\n");
				proc_kill(curproc, EFAULT);
			}
			if ((tgt_vmarea->vma_flags & MAP_SHARED) && (tgt_vmarea->vma_prot & PROT_WRITE)) {
				flags_to_add |= PT_WRITE;
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			pframe_lookup(tgt_vmarea->vma_obj, ADDR_TO_PN(vaddr) - tgt_vmarea->vma_start + tgt_vmarea->vma_off, 0, &tgt_pframe);
			dbg(DBG_PRINT, "(GRADING3D)\n");
		}
		else if (cause & FAULT_WRITE) {
			if (!(tgt_vmarea->vma_prot & PROT_WRITE)) {
				dbg(DBG_TEMP, "Trying to write to a read-only address %p\n", (void*) vaddr);
				dbg(DBG_PRINT, "(GRADING3D)\n");
				proc_kill(curproc, EFAULT);
			}
			flags_to_add |= PT_WRITE;
			pframe_lookup(tgt_vmarea->vma_obj, ADDR_TO_PN(vaddr) - tgt_vmarea->vma_start + tgt_vmarea->vma_off, 1, &tgt_pframe);
			dbg(DBG_PRINT, "(GRADING3D)\n");
		}
		if (tgt_pframe == NULL) {
			dbg(DBG_PRINT, "(GRADING3D)\n");
			proc_kill(curproc, EFAULT);
		}
		KASSERT(tgt_pframe); /* this page frame must be non-NULL */
		dbg(DBG_PRINT, "(GRADING3A 5.a)\n");
		KASSERT(tgt_pframe->pf_addr); /* this page frame's pf_addr must be non-NULL */
		dbg(DBG_PRINT, "(GRADING3A 5.a)\n");

		uintptr_t kaddr = (uintptr_t)(tgt_pframe->pf_addr) + PAGE_OFFSET(vaddr);
		uintptr_t paddr = pt_virt_to_phys(kaddr);
		// When call pt_map(), the values of pdflags and ptflags should be the same.
		pt_map(cur_pd, (uintptr_t) PAGE_ALIGN_DOWN(vaddr), (uintptr_t) PAGE_ALIGN_DOWN(paddr), flags_to_add, flags_to_add);
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return;
}
