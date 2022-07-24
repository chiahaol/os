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

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        //NOT_YET_IMPLEMENTED("VM: shadow_init");
        shadow_allocator = slab_allocator_create("shadow", sizeof(mmobj_t));
        KASSERT(NULL != shadow_allocator && "failed to create shadow allocator!");
		KASSERT(shadow_allocator); /* after initialization, shadow_allocator must not be NULL */
		dbg(DBG_PRINT,"(GRADING3A 6.a)\n");
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros or functions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        //NOT_YET_IMPLEMENTED("VM: shadow_create");
		mmobj_t* shadow = slab_obj_alloc(shadow_allocator);
		mmobj_init(shadow, &shadow_mmobj_ops);
		shadow->mmo_refcount = 1;
		shadow->mmo_nrespages = 0;
		list_init(&shadow->mmo_respages);
		shadow_count++;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        return shadow;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
	//NOT_YET_IMPLEMENTED("VM: shadow_ref");
	/* the o function argument must be non-NULL, has a positive refcount, and is a shadow object */
	KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
	dbg(DBG_PRINT, "(GRADING3A 6.b)\n");
	o->mmo_refcount++;
	dbg(DBG_PRINT, "(GRADING3A)\n");
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
	/* the o function argument must be non-NULL, has a positive refcount, and is a shadow object */
	KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
	dbg(DBG_PRINT, "(GRADING3A 6.c)\n");                         
	if (o->mmo_refcount == (o->mmo_nrespages+1)) {
		pframe_t *pf;
		list_iterate_begin(&o->mmo_respages, pf, pframe_t, pf_olink) {
			pframe_unpin(pf);
			// the bottom object	
			pframe_free(pf);
			dbg(DBG_PRINT, "(GRADING3A)\n");			
		} list_iterate_end();
	}
	o->mmo_refcount--;
	if (o->mmo_refcount == 0) {
		shadow_count--;
		// also might need to put the count for parent mmobj since the refernece
		// to parent is down by one after we free the current shadow object
		// this means it is possible that our freeing of current object
		// will trigger a chain of cleanup all the way down to
		o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
		slab_obj_free(shadow_allocator, o);
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}
	dbg(DBG_PRINT, "(GRADING3A)\n");
	//NOT_YET_IMPLEMENTED("VM: shadow_put");
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. It is important to
 * use iteration rather than recursion here as a recursive implementation
 * can overflow the kernel stack when looking down a long shadow chain */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
	// If for-write, MUST create a copy of pframe at current shadow object
	// then write to it. This is copy-on-write 
	if (forwrite) {
		dbg(DBG_PRINT, "(GRADING3A)\n");
		return pframe_get(o, pagenum, pf);
	}

	// Else not for write, we can choose to get the 
	// first available page along the shadow chain
	mmobj_t * cur_mmobj = o;
	pframe_t * cur_pf = NULL;
	while (cur_mmobj != o->mmo_un.mmo_bottom_obj) {
		cur_pf = pframe_get_resident(cur_mmobj, pagenum);
		// Found the first hadow object in the chain which has the given page resident
		if (cur_pf != NULL) {
			*pf = cur_pf;
			dbg(DBG_PRINT, "(GRADING3A)\n");
			return 0;
		}
		cur_mmobj = cur_mmobj->mmo_shadowed;
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}

	int ret = pframe_lookup(o->mmo_un.mmo_bottom_obj, pagenum, forwrite, pf);
    //NOT_YET_IMPLEMENTED("VM: shadow_lookuppage");
	if (ret==0){
		KASSERT(NULL != (*pf));
		dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
		KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
		dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
	}
	dbg(DBG_PRINT, "(GRADING3D)\n");
	return ret;
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * It is important to use iteration rather than recursion here as a
 * recursive implementation can overflow the kernel stack when
 * looking down a long shadow chain */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
	// pf is the page frame that we want to WRITE INTO
	// so this function should find the existing pframe with existing data
	// in the chain of mmobj, and write the existing data to pf
	// this is inconsistent with the comment above, which seems to suggest
	// us to use the pf to write to another pf. The problem is that if we look
	// at how this function is used in shadow_lookuppage and the definition
	// of fillpage in vnode.h and anon.c, we can understand that this 
	// really is asking us to load data into pf
	// if we want to modify a pf, we can use vmmap_write() instead
	pframe_t *tgt_pframe = NULL;
	// cur_mmobj set to be o's shadowed object, because pf->pf_pagenum is resident in o at this point
	// however we need to find the mmobj that has the actual data
	mmobj_t * cur_mmobj = o->mmo_shadowed;
	KASSERT(pframe_is_busy(pf)); /* can only "fill" a page frame when the page frame is in the "busy" state */
	KASSERT(!pframe_is_pinned(pf)); /* must not fill a page frame that's already pinned */
	dbg(DBG_PRINT, "(GRADING3A 6.e)\n");
	while (cur_mmobj != o->mmo_un.mmo_bottom_obj) {
		tgt_pframe = pframe_get_resident(cur_mmobj, pf->pf_pagenum);
		if (tgt_pframe != NULL) { 
			dbg(DBG_PRINT, "(GRADING3A)\n");
			break;
		}
		cur_mmobj = cur_mmobj->mmo_shadowed;
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}
	
	if (tgt_pframe == NULL){
		if (pframe_get(o->mmo_un.mmo_bottom_obj, pf->pf_pagenum, &tgt_pframe) < 0) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
			return -1;
		}
	}
	
	memcpy(pf->pf_addr, tgt_pframe->pf_addr, PAGE_SIZE);
	if (!pframe_is_pinned(pf)){
		dbg(DBG_PRINT, "(GRADING3A)\n");
		pframe_pin(pf);
	}
	//NOT_YET_IMPLEMENTED("VM: shadow_fillpage");
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return 0;
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
		dbg(DBG_PRINT, "(GRADING3A)\n");
        //NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");
        return 0;
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
		dbg(DBG_PRINT, "(GRADING3A)\n");
        //NOT_YET_IMPLEMENTED("VM: shadow_cleanpage");
        return 0;
}
