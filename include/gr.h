#ifndef _GR_H_
#define _GR_H_
/*
 * Memory resource manager.
   It basically automates the following method:
   1. Make every local variable which is a pointer STATIC:
          static char *buf = NULL;
   2. Before doing anything else in your function, free these pointers:
          if (buf) free(buf);

   You are guaranteed to not leak memory, (until the program exits, 
   see mr_finish())

   This is of course not thread safe...
 */

#include <stdio.h>


typedef int (*iter_return_p)(void *);

/* a function to free a resource.
   Return 0 on success.
   Return >0 on failure.  */
typedef int (*free_func)(void *);

struct resource_t {
	void *data;
	free_func ff;
};

struct genresource {
	struct resource_t *ptrs;
	int num;
	int max;
	struct genresource *next;
	struct genresource *chain;
};

typedef struct genresource *gr_t;

struct iterator_context {
	gr_t gr;
	int cur;
	iter_return_p f;
};

typedef struct iterator_context *iter_t;

int gr_destroy(gr_t gr);
int gr_clear(gr_t gr);
void *gr_malloc(gr_t gr, size_t size);
int gr_track(gr_t gr, void *ptr, free_func ff);
gr_t gr_init(int max);
gr_t gr_get(int max); /* like gr_init, but register to be
                         destroyed by gr_finish */
int gr_finish();
void gr_stats(gr_t gr);

iter_t gr_iter_start(gr_t gr, iter_return_p f);
/* NOTE: once gr_iter_next returns NULL, you cannot use it anymore. */
void *gr_iter_next(iter_t iter);


/* for file descriptors and file pointers. Use like this:

   struct filep_wrapper *fp = calloc(1,sizeof(struct filep_wrapper));
   gr_track(gr, fp, gr_free_filep);
   fp->filep = fopen("/etc/passwd", "r");
   fread(fp->filep, ...);
   ...
   gr_clear(gr); or gr_destroy(gr);
*/
struct filed_wrapper {
	int filed;
};
int gr_free_filed(void *foo);

struct filep_wrapper {
	FILE *filep;
};
int gr_free_filep(void *foo);

#endif
