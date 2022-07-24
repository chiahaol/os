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

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
		anon_allocator = slab_allocator_create("anon", sizeof(mmobj_t));
		KASSERT(anon_allocator); /* after initialization, anon_allocator must not be NULL */
		dbg(DBG_PRINT, "(GRADING3A 4.a)\n");
		dbg(DBG_PRINT, "(GRADING3D)\n");
        // NOT_YET_IMPLEMENTED("VM: anon_init");
}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then initialize it. Take a look in mm/mmobj.h for
 * definitions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
        // NOT_YET_IMPLEMENTED("VM: anon_create");
		mmobj_t *anon = slab_obj_alloc(anon_allocator);
		anon_count++;
		mmobj_init(anon, &anon_mmobj_ops);
		// initialize reference count as 1 since when we create this anon obj
		// there must already be a virtual page that wants it
		// we can see that the vget() function for vnode also has refcount start at 1
		// for the file object it created
		anon->mmo_refcount = 1;
		anon->mmo_nrespages = 0;
		list_init(&anon->mmo_respages);
		// also initialize place to hold its associated vmas
		list_init(&anon->mmo_un.mmo_vmas);
		anon->mmo_shadowed = NULL;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        return anon;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: anon_ref");
		/* the o function argument must be non-NULL, has a positive refcount, and is an anonymous object */
		KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
		dbg(DBG_PRINT, "(GRADING3A 4.b)\n");
		o->mmo_refcount++;
		dbg(DBG_PRINT, "(GRADING3D)\n");
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
	/* the o function argument must be non-NULL, has a positive refcount, and is an anonymous object */
	KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));    
	dbg(DBG_PRINT, "(GRADING3A 4.c)\n");     
	// Code written following the format of vput in vnode.c
	if (o->mmo_refcount==(o->mmo_nrespages+1)){
		// unpin and uncache (i.e. free) all of the object's pages and then free the object itself.
		pframe_t *pf;
		list_iterate_begin(&o->mmo_respages, pf, pframe_t, pf_olink) {
			// a rather complucated recursion, see vput() and kernel FAQ for overall structure
			// basic idea is that in every recursive call, we remove an element 
			// from o->mmo_respages, so as we return from recursion
			// the next element would already be gone
			if (pframe_is_pinned(pf)){
				pframe_unpin(pf);
				dbg(DBG_PRINT, "(GRADING3D)\n");
			}
			pframe_free(pf);
			dbg(DBG_PRINT, "(GRADING3A)\n");
		} list_iterate_end();
		// Follow vnode.c vput assertion
		KASSERT(0 == o->mmo_nrespages);
		KASSERT(1 == o->mmo_refcount);
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}
	o->mmo_refcount--;
	if (o->mmo_refcount==0){
		slab_obj_free(anon_allocator, o);
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}
	dbg(DBG_PRINT, "(GRADING3A)\n");
	// NOT_YET_IMPLEMENTED("VM: anon_put");
}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_lookuppage");
		int return_code = pframe_get(o, pagenum, pf);
		dbg(DBG_PRINT, "(GRADING3A)\n");
        return return_code;
}

/* The following three functions should not be difficult. */

static int
anon_fillpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_fillpage");
		// Fill the page frame starting at address pf->pf_addr with the
		// contents of the page identified by pf->pf_obj and pf->pf_pagenum.
		KASSERT(pframe_is_busy(pf)); /* can only "fill" a page frame when the page frame is in the "busy" state */
		KASSERT(!pframe_is_pinned(pf)); /* must not fill a page frame that's already pinned */
		dbg(DBG_PRINT, "(GRADING3A 4.d)\n");
		int size_to_fill = PAGE_SIZE - PAGE_OFFSET(pf->pf_addr);
		memset(pf->pf_addr, 0, size_to_fill);
		// NOT SURE
		// hide all its associated pages from pageout daemon. Only
		// when the reference count goes to 0 in anon_put(), we will unpin its frames
		// and enable pageout daemon to free them
		if (!pframe_is_pinned(pf)){
			pframe_pin(pf);
			dbg(DBG_PRINT, "(GRADING3D)\n");
		}
		dbg(DBG_PRINT, "(GRADING3A)\n");
        return 0;
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_dirtypage");

		// Indicates that a page is about to be modified.
		// For vnode, dirtypage is to return zero on success and nonzero otherwise 
        dbg(DBG_PRINT, "(GRADING3D)\n");
		return 0;
}

static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_cleanpage");
		// Clean a dirty page. For files, write content back to disk, for anon object, update
		// swap object. However, Weenix doesn't implement swap space, so there's nothing we can
		// do here. We just keep the pin as before so that pageout daemon will not remove
		// this page
		// return 0 on success
        dbg(DBG_PRINT, "(GRADING3D)\n");
		return 0;
}
