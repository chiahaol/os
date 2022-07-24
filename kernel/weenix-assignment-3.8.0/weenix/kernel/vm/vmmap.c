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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        vmmap_t *new_vmmap = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
        list_init(&new_vmmap->vmm_list);
        new_vmmap->vmm_proc = NULL;
        // NOT_YET_IMPLEMENTED("VM: vmmap_create");
		dbg(DBG_PRINT, "(GRADING3B)\n");
        return new_vmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        vmarea_t *vma;
		KASSERT(NULL != map); /* function argument must not be NULL */
		dbg(DBG_PRINT, "(GRADING3A 3.a)\n");
        // remove all vmareas from the address space
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
			list_remove(&vma->vma_plink);
			list_remove(&vma->vma_olink);
			vma->vma_obj->mmo_ops->put(vma->vma_obj);
			vmarea_free(vma);
			dbg(DBG_PRINT, "(GRADING3A)\n");
        } list_iterate_end();
        // free the vmmap struct
		if (map->vmm_proc){
			map->vmm_proc->p_vmmap = NULL;
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
        slab_obj_free(vmmap_allocator, map);
		dbg(DBG_PRINT, "(GRADING3A)\n");
        // NOT_YET_IMPLEMENTED("VM: vmmap_destroy");
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
		KASSERT(NULL != map && NULL != newvma); /* both function arguments must not be NULL */
		dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
		KASSERT(NULL == newvma->vma_vmmap); /* newvma must be newly create and must not be part of any existing vmmap */
		dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
		newvma->vma_vmmap = map;
        vmarea_t *nxt_vma;
		// iterate through vma's. The first time we see a vma with
		// larger starting address than newvma's ending address,
		// we have encountered the vma we want to insert before, assuming valid newvma
        list_iterate_begin(&map->vmm_list, nxt_vma, vmarea_t, vma_plink) {
			if (nxt_vma->vma_start >= newvma->vma_end){
				list_insert_before(&nxt_vma->vma_plink, &newvma->vma_plink);
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return ;
			}
			dbg(DBG_PRINT, "(GRADING3A)\n");
        }list_iterate_end();
		KASSERT(newvma->vma_start < newvma->vma_end); /* newvma must not be empty */
		dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
		/* addresses in this memory segment must lie completely within the user space */
		KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
		dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        // NOT_YET_IMPLEMENTED("VM: vmmap_insert");
		// if reach this point, either
			// the vmm_list is empty, or
			// the new vmarea has a very high address
		// in either case, append new vmarea to the end of the list
		list_insert_tail(&map->vmm_list, &newvma->vma_plink);
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return ;
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
		uint32_t starting_vfn = -1;
		vmarea_t *cur_vma;
		if (list_empty(&map->vmm_list)){
			if (dir == VMMAP_DIR_HILO ){
				// if size not too large
				if (ADDR_TO_PN(USER_MEM_HIGH)- npages >= ADDR_TO_PN(USER_MEM_LOW)){
					dbg(DBG_PRINT, "(GRADING3C)\n");
					return (int) ADDR_TO_PN(USER_MEM_HIGH)- npages;
				} else {
					dbg(DBG_PRINT, "(GRADING3D)\n");
					return -1;
				}
				dbg(DBG_PRINT, "(GRADING3D)\n");
			} 
			else if (dir == VMMAP_DIR_LOHI){
				// if size not too large
				if (npages+ADDR_TO_PN(USER_MEM_LOW < ADDR_TO_PN(USER_MEM_HIGH))){
					dbg(DBG_PRINT, "(GRADING3C)\n");
					return (int) ADDR_TO_PN(USER_MEM_LOW);
				} else {
					dbg(DBG_PRINT, "(GRADING3D)\n");
					return -1;
				}
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			dbg(DBG_PRINT, "(GRADING3D)\n");
		}
		// If dir is VMMAP_DIR_HILO, find a gap as high in the address space as possible; 
		// i.e. allocate a space that is later in the list of vm ares
		if (dir == VMMAP_DIR_HILO){
			uint32_t prev_start_vfn = ADDR_TO_PN(USER_MEM_HIGH);
			uint32_t cur_end_vfn;
			list_iterate_reverse(&map->vmm_list, cur_vma, vmarea_t, vma_plink) {
				cur_end_vfn = cur_vma->vma_end;
				uint32_t npage_diff = prev_start_vfn - cur_end_vfn;
				if (npage_diff >= npages){
					dbg(DBG_PRINT, "(GRADING3D)\n");
					return prev_start_vfn-npages;
				}
				prev_start_vfn = cur_vma->vma_start;
			}list_iterate_end();
			// check whether beginning segment from USER_MEM_LOW to the first vma to have enoguh
			// if have enough, change starting_vfn
			if (cur_vma->vma_start - ADDR_TO_PN(USER_MEM_LOW) >= npages){
				dbg(DBG_PRINT, "(GRADING3D)\n");
				starting_vfn = cur_vma->vma_start;
			}
			dbg(DBG_PRINT, "(GRADING3D)\n");
			return (int) starting_vfn-npages;
		} 
		// if dir is VMMAP_DIR_LOHI, the gap should be as low as possible.
		// i.e. first fit
		else if (dir == VMMAP_DIR_LOHI){
			uint32_t prev_end_vfn = ADDR_TO_PN(USER_MEM_LOW);
			uint32_t cur_start_vfn;
			list_iterate_begin(&map->vmm_list, cur_vma, vmarea_t, vma_plink) {
				cur_start_vfn = cur_vma->vma_start;
				// num_pages from start to end addr is the difference between
				// page_start of current vmarea and pange_end of previous vmarea
				// e.g. current vmarea starts at page 5 and prev vmarea ends at page 3
				// then number of pages between them is 5-3==2 pages (page 3 and page 4, respectively)
				uint32_t npage_diff = cur_start_vfn - prev_end_vfn;
				if (npage_diff >= npages){
					dbg(DBG_PRINT, "(GRADING3D)\n");
					// if dir is VMMAP_DIR_LOHI, the gap should be as low as possible.
					// i.e. first fit
					return prev_end_vfn;
				}
				prev_end_vfn = cur_vma->vma_end;
			}list_iterate_end();
			// check cur end to USER_MEM_HIGH have enoguh (i.e. fit at end of vmareas list)
			// if have enough, change starting_vfn
			if (ADDR_TO_PN(USER_MEM_HIGH) - cur_vma->vma_end >= npages){
				starting_vfn = cur_vma->vma_end;
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			dbg(DBG_PRINT, "(GRADING3D)\n");
			return (int) starting_vfn;
		}
        
        // NOT_YET_IMPLEMENTED("VM: vmmap_find_range");
		dbg(DBG_PRINT, "(GRADING3D)\n");
		return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
		vmarea_t *vma;
		KASSERT(NULL != map); /* the first function argument must not be NULL */
		dbg(DBG_PRINT, "(GRADING3A 3.c)\n");
		list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
			if (vfn < vma->vma_start){
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return NULL;
			// vma_start is inclusive (i.e. closed internval) 
			// vma_end is exclusive (i.e. open interval)
			} else if (vfn >= vma->vma_start && vfn < vma->vma_end){
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return vma;
			}
        }list_iterate_end();
		dbg(DBG_PRINT, "(GRADING3C)\n");
        // NOT_YET_IMPLEMENTED("VM: vmmap_lookup");
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
		vmmap_t * new_map = vmmap_create();
		vmarea_t *vma;
		list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
			vmarea_t * new_vma = vmarea_alloc();
			new_vma->vma_start = vma->vma_start;
			new_vma->vma_end = vma->vma_end;
			new_vma->vma_off = vma->vma_off;
			new_vma->vma_prot = vma->vma_prot;
			new_vma->vma_flags = vma->vma_flags;
			//new_vma->vma_vmmap = new_map;
			// The areas should have no mmobjs set yet
			new_vma->vma_obj = NULL;
			// vma_olink is the list of processes that map to a page frame in the bottom object
			// when we clone the current vmmap, the new vmmap does NOT point to any mmobj and thus 
			// not to any pframe, so we shouldn't bother with olink
			vmmap_insert(new_map, new_vma);
			dbg(DBG_PRINT, "(GRADING3A)\n");
        }list_iterate_end();
		dbg(DBG_PRINT, "(GRADING3A)\n");
        // NOT_YET_IMPLEMENTED("VM: vmmap_clone");
        return new_map;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_map");
		KASSERT(NULL != map); /* must not add a memory segment into a non-existing vmmap */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
		KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
		KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags)); /* must specify whether the memory segment is shared or private */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
		KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)); /* if lopage is not zero, it must be a user space vpn */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
		KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages))); /* if lopage is not zero, the specified page range must lie completely within the user space */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
		KASSERT(PAGE_ALIGNED(off)); /* the off argument must be page aligned */
		dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
                                    
		// If lopage is zero, we will find a range of virtual addresses in the process that is big enough, by using vmmap_find_range with the same dir argument.  
		if (lopage == 0){
			int tgt_addr = vmmap_find_range(map, npages, dir);
			if (tgt_addr < 0) {
				dbg(DBG_PRINT, "(GRADING3D)\n");
				return -1;
			}
			lopage = (uint32_t) tgt_addr;
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		else if (!vmmap_is_range_empty(map, lopage, npages)){
			dbg(DBG_PRINT, "(GRADING3A)\n");
			vmmap_remove(map, lopage, npages);
		}
		// create vmarea with updated lopage
		vmarea_t * new_vma = vmarea_alloc();
		new_vma->vma_start = lopage;
		new_vma->vma_end = lopage + npages;
		new_vma->vma_off = off / PAGE_SIZE; // off is a multiple of PAGE_SIZE
		new_vma->vma_prot = prot;
		new_vma->vma_flags = flags;
		//new_vma->vma_vmmap = map;
		mmobj_t *shadow_obj = NULL;
		mmobj_t *bottom_obj = NULL;

		// If file is NULL an anon mmobj will be used to create a mapping of 0's. 
		if (file==NULL){
			bottom_obj = anon_create();
			dbg(DBG_PRINT, "(GRADING3A)\n");
		// If file is non-null that vnode's file will be mapped in for the given range.  
		} else {
			// Use the vnode's mmap operation to get the mmobj for the file; do not assume it is file->vn_obj. 
			// Make sure all of the area's fields except for vma_obj have been set before calling mmap.
			file->vn_ops->mmap(file, new_vma, &bottom_obj);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		list_insert_tail(&bottom_obj->mmo_un.mmo_vmas, &new_vma->vma_olink);
		bottom_obj->mmo_shadowed = NULL;
		
		// If MAP_PRIVATE is specified set up a shadow object for the mmobj.
		if ((flags&MAP_PRIVATE)>0){
			shadow_obj = shadow_create();
			new_vma->vma_obj = shadow_obj;
			shadow_obj->mmo_shadowed = bottom_obj;
			shadow_obj->mmo_un.mmo_bottom_obj = bottom_obj;
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} else {
			new_vma->vma_obj = bottom_obj;
			dbg(DBG_PRINT, "(GRADING3D)\n");
		}
		if (file){
			dbg(DBG_PRINT, "(GRADING3B)\n");
			dbg(DBG_TEMP, "Vmmapped file %d at lopage address %p, with shadow obj %p and bottom obj %p\n", file->vn_vno, (void*)lopage, shadow_obj, bottom_obj);
		}
		else {
			dbg(DBG_PRINT, "(GRADING3B)\n");
			dbg(DBG_TEMP, "Vmmapped anonymous obj at lopage address %p, with shadow obj %p and bottom obj %p\n", (void*)lopage, shadow_obj, bottom_obj);
		}
		// If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
		if (new!=NULL){
			dbg(DBG_PRINT, "(GRADING3A)\n");
			*new = new_vma;
		}
		
		dbg(DBG_PRINT, "(GRADING3A)\n");
		vmmap_insert(map, new_vma);
		
        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_remove");
		uint32_t remove_start = lopage;
		uint32_t remove_end = remove_start + npages;
		vmarea_t *vma;
		dbg(DBG_TEMP, "About to remove vmareas from %p to %p\n", (void*)remove_start, (void*) remove_end);
		list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {

			uint32_t cur_start = vma->vma_start;
			uint32_t cur_end = vma->vma_end;
			// Case 1:  [   ******    ]
			if (cur_start<remove_start && cur_end>remove_end){
				// Split one vma into two
				// Create first half as new vma (with same start vfn but earlier end vfn)
				vmarea_t * new_vma = vmarea_alloc();
				new_vma->vma_start = cur_start;
				new_vma->vma_end = remove_start;
				new_vma->vma_off = vma->vma_off; // set offset here, before modifying the offset of old vma
				new_vma->vma_prot = vma->vma_prot;
				new_vma->vma_flags = vma->vma_flags;
				//new_vma->vma_vmmap = vma->vma_vmmap;
				new_vma->vma_obj = vma->vma_obj;
				// make sure to increment the reference count to the mmobj associated with the vmarea.
				new_vma->vma_obj->mmo_ops->ref(new_vma->vma_obj);

				// modify cur_vma to correspond to latter half (later start vfn with same end vfn)
				// NOT SURE if also need to modify vma_off according to kernel FAQ
				vma->vma_start = remove_end;
				vma->vma_off += (remove_end - cur_start);

				// after finished setting up the new and old vmas, we can insert properly using
				// the updated vfn's
				vmmap_insert(map, new_vma);

				// also insert olink to bottom obj of current vma
				list_insert_before(&vma->vma_olink, &new_vma->vma_olink);
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			// Case 4: *[*************]**
			else if (cur_start>=remove_start && cur_end<=remove_end){
				// free cur vma
				list_remove(&vma->vma_plink);
				list_remove(&vma->vma_olink);
				vma->vma_obj->mmo_ops->put(vma->vma_obj);
				vmarea_free(vma);
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			// Case 2:  [      *******]**
			else if (cur_start<remove_start && cur_end<=remove_end && cur_end>remove_start){
				// modify cur vma with earlier vma_end
				vma->vma_end = remove_start;
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			// Case 3: *[*****        ]
			else if (remove_start<=cur_start && cur_end>remove_end && remove_end>cur_start){
				// modify cur_vma with later vma_start
				// NOT SURE if need to modify part of vm_off to according to kernel FAQ
				vma->vma_start = remove_end;
				vma->vma_off += (remove_end - cur_start);
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
        }list_iterate_end();
		dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
		if (npages==0){
			return 1;
		}
		// no mappings defined as no overlap between 
		// given address interval and the current vm areas
		/* the specified page range must not be empty and lie completely within the user space */
		uint32_t endvfn = startvfn+npages;
		KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");                          
		uint32_t tgt_start = startvfn;
		uint32_t tgt_end = startvfn + npages;
		vmarea_t *vma;
		list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
			uint32_t cur_start = vma->vma_start;
			uint32_t cur_end = vma->vma_end;
			// Case 1: overlapped with tgt interval before or after current vma
			if (cur_start<tgt_end && tgt_start<cur_end){
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return 0;
			}
			// Case 2: overlapped with tgt interval inside current vma
			else if (tgt_start>=cur_start && tgt_end<=cur_end){
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return 0;
			}
			// Case 3: overlapped with current vma inside tgt interval
			else if (tgt_start<=cur_start && tgt_end>=cur_end){
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return 0;
			}
        }list_iterate_end();
		dbg(DBG_PRINT, "(GRADING3A)\n");
        // NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");
        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_read");
		size_t count_copied = 0;
		void * cur_vaddr = (void *)(size_t) vaddr;
		while (count_copied < count) {
			vmarea_t * tgt_vmarea = vmmap_lookup(map, ADDR_TO_PN(cur_vaddr));
			if (tgt_vmarea == NULL) {
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return -1;
			}

			for (uint32_t vfn = ADDR_TO_PN(cur_vaddr); vfn < tgt_vmarea->vma_end; vfn++) {
				size_t offset = PAGE_OFFSET(cur_vaddr);
				size_t count_to_copy = ((PAGE_SIZE - offset) <= (count - count_copied)) ? PAGE_SIZE - offset : count - count_copied;
				pframe_t * tgt_pframe = NULL;
				pframe_lookup(tgt_vmarea->vma_obj, vfn - tgt_vmarea->vma_start + tgt_vmarea->vma_off, 0, &tgt_pframe);
				void * src_addr = (void *) ((size_t) tgt_pframe->pf_addr + offset);
				void * des_addr = (void *) ((size_t) buf + count_copied);
				memcpy(des_addr, src_addr, count_to_copy);
				cur_vaddr = (void *) ((size_t) cur_vaddr + count_to_copy);
				count_copied += count_to_copy;
				if (count_copied >= count) {
					dbg(DBG_PRINT, "(GRADING3A)\n");
					break;
				}
				dbg(DBG_PRINT, "(GRADING3B)\n");	
			}
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_write");
		size_t count_copied = 0;
		void * cur_vaddr = (void *)(size_t) vaddr;
		while (count_copied < count) {
			vmarea_t * tgt_vmarea = vmmap_lookup(map, ADDR_TO_PN(cur_vaddr));
			if (tgt_vmarea == NULL) {
				dbg(DBG_PRINT, "(GRADING3A)\n");
				return -1;
			}

			for (uint32_t vfn = ADDR_TO_PN(cur_vaddr); vfn < tgt_vmarea->vma_end; vfn++) {
				size_t offset = PAGE_OFFSET(cur_vaddr);
				size_t count_to_copy = ((PAGE_SIZE - offset) <= (count - count_copied)) ? PAGE_SIZE - offset : count - count_copied;
				pframe_t * tgt_pframe = NULL;
				pframe_lookup(tgt_vmarea->vma_obj, vfn - tgt_vmarea->vma_start + tgt_vmarea->vma_off, 1, &tgt_pframe);
				pframe_dirty(tgt_pframe);
				void * des_addr = (void *) ((size_t) tgt_pframe->pf_addr + offset);
				void * src_addr = (void *) ((size_t) buf + count_copied);
				memcpy(des_addr, src_addr, count_to_copy);
				cur_vaddr = (void *) ((size_t) cur_vaddr + count_to_copy);
				count_copied += count_to_copy;
				if (count_copied >= count) {
					dbg(DBG_PRINT, "(GRADING3A)\n");
					break;
				}
				dbg(DBG_PRINT, "(GRADING3A)\n");	
			}
			dbg(DBG_PRINT, "(GRADING3A)\n");
		}
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return 0;
}
