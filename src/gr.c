#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "gr.h"

static int dbg=0;
static int destroy_chains_on_clear=1;

static void gr_printstats(gr_t gr) 
{
	fprintf(stderr, "n=%d m=%d usage=%.2f%%\n",
	        gr->num, gr->max, 
		100*gr->num/(float)gr->max);

	if (gr->chain) gr_printstats(gr->chain);
}

/*
  clear the gr, then free its allocated ptrs array, and then
  free gr itself 
*/
int gr_destroy(gr_t gr) 
{
	int ret_val;
	if (!gr) return 0;

	ret_val = gr_clear(gr);
	free(gr->ptrs);
	free(gr);
	return ret_val;
}

/* frees all pointers in gr:
   if chain is present, destroy it.
   then loop through ptrs, calling the user supplied
   cleanup function on all pointers. If none was supplied
   (ie ff == NULL), then call free.
*/
int gr_clear(gr_t gr) 
{
	int i;
	int ret_val = 0; /* > 0 means there was a problem freeing
	                    some resources */

	assert(gr);
	assert(gr->ptrs);

	if (gr->chain) {
		if (dbg) fprintf(stderr, "gr_clear: clearing chain\n");
		if (destroy_chains_on_clear) {
			ret_val += gr_destroy(gr->chain);
			gr->chain = NULL;
		} else {
			ret_val += gr_clear(gr->chain);
		}
	}

	/* if ff (free_func) is NULL, assume that we
	   should just free the data */
	for (i = 0; i < gr->num; i++) {
		if (gr->ptrs[i].data) {
			if (gr->ptrs[i].ff)
				ret_val += (*gr->ptrs[i].ff)(gr->ptrs[i].data);
			else
				free(gr->ptrs[i].data);
			gr->ptrs[i].data = NULL;
		}
	}

	if (dbg) fprintf(stderr, "gr_clear: cleared %d\n", i);

	gr->num = 0;
	return ret_val;
}

/* mallocs a pointer and tracks it in gr, with NULL supplied
   as the cleanup function
*/
void *gr_malloc(gr_t gr, size_t size) 
{
	void *r;

	assert(gr);
	assert(gr->ptrs);

	r = calloc(1, size);
	if (!r) {
		if (dbg) fprintf(stderr, "gr_malloc: calloc failed\n");
		return r;
	}

	if (!gr_track(gr, r, NULL)) {
		if (dbg) fprintf(stderr, "gr_malloc: gr_track failed\n");
		free(r);
		return NULL;
	}

	return r;
}

/* if you get a mallocd pointer from somewhere and want to have
   it freed by gr_clear 
   If all the spots in gr->ptrs are taken, allocate/follow gr->chain.
   Note: I don't really like using goto, but it is cheaper than
   a recursive call.
   It is equivalent to replacing the goto with:
   return gr_track(gr->chain, ptr, ff);
*/
int gr_track(gr_t gr, void *ptr, free_func ff) 
{

	recurse:
	if (gr->num == gr->max) {
		if (!gr->chain) {
			if (dbg) fprintf(stderr, "gr_track: chaining %d\n",
		                 gr->max);
			/* create another gr of twice our size */
			gr->chain = gr_init(2*gr->max);
		}
		gr = gr->chain;
		goto recurse;
	}

	gr->ptrs[gr->num].data = ptr;
	gr->ptrs[gr->num++].ff = ff;
	return 1;
}

/* list of allocated memresources */
static struct genresource *gr_List;
#define gr_SIZE  sizeof(struct genresource)
#define resource_SIZE sizeof(struct resource_t)

/* 
   create a gr with a ptrs table of size max.
   The default size is 32 (if 0 is passed in).
*/
gr_t gr_init(int max) 
{
	gr_t gr; 

	if (max == 0) max = 32;
	gr = calloc(1, gr_SIZE);
	gr->ptrs = calloc(resource_SIZE, max);
	gr->max = max;
	gr->num = 0;
	return gr;
}

/* 
   same as gr_init, but puts the returned gr into
   gr_List. All gr's in this list are destroyed
   when gr_finish() is called.
*/
gr_t gr_get(int max) 
{
	gr_t gr; 

	gr = gr_init(max);
	/* LOCK */
	gr->next = gr_List;
	gr_List = gr;
	/* UNLOCK */
	return gr;
}

/* cleans up all allocated genresources 
   returns 0 on success, >0 if any errors
   were encountered freeing resources.
*/
int gr_finish() 
{
	gr_t cur; 
	gr_t prev = NULL;
	int ret_val = 0;
	int save = destroy_chains_on_clear;

	destroy_chains_on_clear = 1;

	/* LOCK */
	cur = gr_List;
	gr_List = NULL;
	/* UNLOCK */

	while (cur) {
		prev = cur;
		cur = cur->next;
		ret_val += gr_destroy(prev);
	}
	destroy_chains_on_clear = save;
	return ret_val;
}

void gr_stats(gr_t gr) 
{
	gr_t cur = gr;

	if (!cur) cur = gr_List;

	while (cur) {
		gr_printstats(cur);
		fprintf(stderr, "----------------------\n");
		cur = cur->next;
	}
}

#define iter_size sizeof(struct iterator_context)

/*
   returns an iterator_context positioned at the
   start of the items in gr.
   the function f is a predicate that gr_iter_next
   uses to decide whether to return a given
   item in gr.
*/
iter_t gr_iter_start(gr_t gr, iter_return_p f) 
{
	iter_t iter;

	iter = calloc(1, iter_size);
	if (!iter) {
		if (dbg) fprintf(stderr, "gr_iter_start: calloc failed.\n");
		return iter;
	}
	iter->gr = gr;
	iter->cur = 0;
	iter->f = f;
	return iter;
}

/*
  returns the next item in gr for which iter->f returns true.
  The ugly goto rears its head once again.
*/
void *gr_iter_next(iter_t iter) 
{
	/* sugar... */
	gr_t gr; 
	iter_return_p f = iter->f;

	recurse:
	gr = iter->gr;
	if (!gr) {
		free(iter); /* this is why you can't use an iter_t
		               after gr_iter_next has returned
			       NULL */
		return NULL;
	}

	/* f is a function which tells us whether or not
	   we should return gr->ptrs[iter->cur].data */
	while (iter->cur < gr->num) {
		if (f) {
			if ((*f)(gr->ptrs[iter->cur].data)) 
				return gr->ptrs[iter->cur++].data;
			/* f returned false. skip to next element */
			else iter->cur++;
		}
		/* f == NULL means return everything */
		else return gr->ptrs[iter->cur++].data;
	}

	/* at this point, iter->cur == mr->num */
	iter->gr = gr->chain;
	iter->cur = 0;
	goto recurse; /* saves stack space! yay! */
}

int gr_free_filed(void *foo)
{
	int ret_val = 0;
	struct filed_wrapper *fd = foo;

	if (fd >= 0 && close(fd->filed) != 0) ret_val = 1;
	free(foo);
	return ret_val;
}

int gr_free_filep(void *foo)
{
	int ret_val = 0;
	struct filep_wrapper *fp = foo;

	if (fp && fclose(fp->filep) != 0) ret_val = 1;
	free(foo);
	return ret_val;
}
