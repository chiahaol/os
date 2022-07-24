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

#include "globals.h"
#include "errno.h"
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"

#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "proc/proc.h"
#include "vm/anon.h"
#define validRange(lower, upper, x) ( ((uint32_t)(x) < lower) && (x > USER_MEM_HIGH) )
/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should "return" the current break. We use this to
 * implement sbrk(0) without writing a separate syscall. Look in
 * user/libc/syscall.c if you're curious.
 *
 * You should support combined use of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{
        if (!addr) {			
			*ret = curproc->p_brk;
			dbg(DBG_PRINT, "(GRADING3A)\n");
			return 0;
        }
		// addr exceeded user memory zone
		// Note that according to man page, the program break is the first location after the end of the uninitialized data segment).
		// i.e. p_brk is EXCLUSIVE
		if (addr >= (void *) USER_MEM_HIGH || addr < curproc->p_start_brk){
			dbg(DBG_PRINT, "(GRADING3D)\n");
			return -ENOMEM;
		}
        uint32_t  npages =  1, heap_lopage, heap_hipage; //we anticipate that stack should grow to high address 
        vmarea_t *new_vma = NULL;
        int res = 0;
        vmmap_t *map = NULL;
        
        dbg(DBG_TEMP, "Entering do_brk at addr %p\n", addr);


        // All possible cases for changing dynamic region vmarea:
			// 1. Initially, the heap takes up the empty space in the same vmarea as the "data+bss" region. 
				// this is the "initial" region used by the heap.
				// in this case, there is no dedicated vmarea for heap
			// 2. When we move p_brk outside of the initial region via a large input vfn value
				// in this case, we need to create a new vmarea for heap region while keeping the initial region
			// 3. When the input vfn exceeds the current p_brk
				// in this case, we need to extend the heap vmarea to the page immediately before p_brk (since p_brk is exclusive)
			// 4. When the input vfn i before current p_brk and within current heap vmarea
				// in this case, we need to shrink the heap vmarea just enough to include addr, 
				// and adjust p_brk to page immediately after addr
        	// 5. When we shrink the brk back inside the initial region, 
				// in this case, we should remove the "new" dynamic vmarea

		// When heap vmarea does not currently exist, which includes case (1) above
        if (ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_brk)) == ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk))) {
			// If We want to move p_brk around inside the initial region
			if (addr < PAGE_ALIGN_UP(curproc->p_start_brk)){
				curproc->p_brk = addr;
				dbg(DBG_PRINT, "(GRADING3A)\n");
			}
			// Case (2) above, we want to move p_brk outside of the initial region
			else {
				//create new heap vmarea
				heap_lopage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_brk));
				heap_hipage = ADDR_TO_PN(PAGE_ALIGN_UP(addr)); // NOT SURE IF addr is INCLUSIVE, if so, need addr+1 over here
				npages = heap_hipage - heap_lopage;
				//check vm address valid and not overlap 
				if (!vmmap_is_range_empty(curproc->p_vmmap, heap_lopage, npages)) {
					dbg(DBG_PRINT, "(GRADING3D)\n");
					return -ENOEXEC;
				}
				if (npages>0){
					res = vmmap_map(curproc->p_vmmap, NULL, heap_lopage, npages,
								PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, VMMAP_DIR_HILO, &new_vma);
					if (res < 0) {
						dbg(DBG_PRINT, "(GRADING3D)\n");
						return res;
					}
				}
				curproc->p_brk = addr;
				dbg(DBG_PRINT, "(GRADING3A)\n");
			}
        // Special case when vmarea doesn't exist even though we have modified p_brk beofre
		// this case is possible becase we swap curproc's vmmap for another, and the program
		// didn't pre-load relevant area in for us
        } 
		else if (vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk)))==NULL){
			//create new heap vmarea
			heap_lopage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk));
			heap_hipage = ADDR_TO_PN(PAGE_ALIGN_UP(addr)); // NOT SURE IF addr is INCLUSIVE, if so, need addr+1 over here
			npages = heap_hipage - heap_lopage;
			//check vm address valid and not overlap 
			if (!vmmap_is_range_empty(curproc->p_vmmap, heap_lopage, npages)) {
					dbg(DBG_PRINT, "(GRADING3A)\n");
					return -ENOEXEC;
			}
			res = vmmap_map(curproc->p_vmmap, NULL, heap_lopage, npages,
							PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, VMMAP_DIR_HILO, &new_vma);
			if (res < 0) {
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return res;
			}
			curproc->p_brk = addr;
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} 
		// When heap vmarea already exists
		else {
			// Case (3) above, When the input addr exceeds the current p_brk
			// we need to extend the heap vmarea to cover addr
			if (addr >= curproc->p_brk){
				heap_lopage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk));
				heap_hipage = ADDR_TO_PN(PAGE_ALIGN_UP(addr)); // NOT SURE IF addr is INCLUSIVE, if not exclusive, need addr+1 over here
				vmarea_t *cur_vma = vmmap_lookup(curproc->p_vmmap, heap_lopage);
				KASSERT(cur_vma!=NULL);
				if (!vmmap_is_range_empty(curproc->p_vmmap, cur_vma->vma_end, heap_hipage-cur_vma->vma_end)){
					dbg(DBG_TEMP, "Failed to extend heap because of blocking vmarea at %p\n", (void*)cur_vma->vma_end);
					dbg(DBG_PRINT, "(GRADING3A)\n");
					return -1;
				}
				cur_vma->vma_end = heap_hipage;
				curproc->p_brk = addr;
				dbg(DBG_PRINT, "(GRADING3A)\n");
			}
        	// Case (5) above. When we shrink the brk back inside the initial region, 
			// remove the "new" dynamic vmarea
			else if (addr < PAGE_ALIGN_UP(curproc->p_start_brk)){
				heap_lopage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk));
				heap_hipage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_brk));
				npages = heap_hipage - heap_lopage;
				vmmap_remove(curproc->p_vmmap, heap_lopage, npages);
				curproc->p_brk = addr;
				dbg(DBG_PRINT, "(GRADING3A)\n");
			}
			// Case (4) above. When the input addr is before current p_brk but within current heap vmarea
			// shrink the heap vmarea just enough to include addr and adjust p_brk to page immediately after addr
			else {
				heap_lopage = ADDR_TO_PN(PAGE_ALIGN_UP(curproc->p_start_brk));
				heap_hipage = ADDR_TO_PN(PAGE_ALIGN_UP(addr)); // NOT SURE IF addr is INCLUSIVE, if not exclusive, need addr+1 over here
				vmarea_t *cur_vma = vmmap_lookup(curproc->p_vmmap, heap_lopage);
				KASSERT(cur_vma!=NULL);
				cur_vma->vma_end = heap_hipage;
				curproc->p_brk = addr;
				dbg(DBG_PRINT, "(GRADING3A)\n");
			}
        }
		*ret = curproc->p_brk;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        // Flush the process pagetables and TLB 
		// void * hi_mem_to_unmap = curproc->p_brk;
		// if (addr > curproc->p_brk){
		// 	hi_mem_to_unmap = addr;
		// }
        // pt_unmap_range(curproc->p_pagedir, curproc->p_start_brk, hi_mem_to_unmap);
        // tlb_flush_all();

        //NOT_YET_IMPLEMENTED("VM: do_brk");
        return 0;
}
