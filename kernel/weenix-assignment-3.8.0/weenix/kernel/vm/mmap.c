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
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        if (addr != NULL && ((uint32_t) addr < USER_MEM_LOW || (uint32_t) addr >= USER_MEM_HIGH)) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        if (USER_MEM_LOW + len <= USER_MEM_LOW || USER_MEM_LOW + len > USER_MEM_HIGH) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }
        
        // len should be greater than 0, off should be a multiple of PAGE_SIZE
        if (((fd < 0 || fd >= NFILES) && !(flags & MAP_ANON)) || !PAGE_ALIGNED(off)) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        // flags must have either MAP_SHARED or MAP_PRIVATE, and can be ORed with MAP_FIXED, and MAP_ANON
        if ((flags >> 4) > 0 || ((flags & 3) != MAP_SHARED && (flags & 3) != MAP_PRIVATE)) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }
        /*Don't interpret addr as a hint: place the mapping at exactly that address.  addr must be a multiple of the page size.  If the
        memory region specified by addr and len overlaps pages of any existing mapping(s), then the overlapped part of  the  existing
        mapping(s)  will be discarded.  If the specified address cannot be used, mmap() will fail.  Because requiring a fixed address
        for a mapping is less portable, the use of this option is discouraged.*/
        if ((flags & MAP_FIXED) && addr == NULL) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        // prot must be ORed value of PROT_NONE, PROT_READ, PROT_WRITE , PROT_EXEC
        if ((prot >> 3) > 0) {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }
        
        file_t * f = NULL;
        vnode_t * vn = NULL;
        if (!(flags & MAP_ANON)) {
                f = fget(fd);
                if (f == NULL) {
                        *ret = MAP_FAILED;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return -EINVAL;
                }
                
                if (((prot & PROT_READ) && !(f->f_mode & FMODE_READ)) || ((prot & PROT_WRITE) && !(f->f_mode & FMODE_WRITE))) {
                        fput(f);
                        *ret = MAP_FAILED;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return -EINVAL;
                }
                vn = f->f_vnode;
                fput(f);
                dbg(DBG_PRINT, "(GRADING3A)\n");
        }
        
        //If addr is NULL, then the kernel chooses the (page-aligned) 
        //address at which to create the mapping;
        uint32_t lopage = (addr != NULL) ? ADDR_TO_PN((uint32_t) addr) : 0;
        uint32_t npages = ADDR_TO_PN(PAGE_ALIGN_UP(len));

        int res;
        vmarea_t * new_vma = NULL;
        if ((res = vmmap_map(curproc->p_vmmap, vn, lopage, npages, prot, flags, off, VMMAP_DIR_HILO, &new_vma)) == 0) {
                *ret =(void*) PN_TO_ADDR(new_vma->vma_start);
                pt_unmap(curproc->p_pagedir, (uintptr_t) *ret);
                tlb_flush((uintptr_t) *ret);
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        else {
                *ret = MAP_FAILED;
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        KASSERT(NULL != curproc->p_pagedir); /* page table must be valid after a memory segment is mapped into the address space */
        dbg(DBG_PRINT, "(GRADING3A 2.a)\n");
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return res;
        // NOT_YET_IMPLEMENTED("VM: do_mmap");
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        // according to linux man page, addr should be page aligned
        //KASSERT(PAGE_ALIGNED((uint32_t) addr));
        if (addr == NULL || (uint32_t) addr < USER_MEM_LOW || (uint32_t) addr >= USER_MEM_HIGH || !PAGE_ALIGNED((uint32_t) addr)) {
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        if ((size_t) addr + len <= (size_t) addr || (size_t) addr + len > USER_MEM_HIGH) {
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        int res;
        uint32_t lopage = ADDR_TO_PN((uint32_t) addr);
        uint32_t npages = ADDR_TO_PN(PAGE_ALIGN_UP(len));

        if ((res = vmmap_remove(curproc->p_vmmap, lopage, npages)) == 0) {
                pt_unmap(curproc->p_pagedir, (uintptr_t) addr);
                tlb_flush((uintptr_t) addr);
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return res;
        //NOT_YET_IMPLEMENTED("VM: do_munmap");
}

