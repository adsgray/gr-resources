/*
 Suppose you have a very complex function, with many returns-on-error.
 It is difficult to know when to free things before returning etc.
 You also may call functions which malloc things for you that
 you have to remember to free as well.

 Solution: wrapper function.

 int orig_function_name(orig args) {
   int ret_val;
   gr_t gr;

   gr = gr_init(10);
   ret_val = orig_function_name_foo(gr, orig_args);
   gr_destroy(gr);
   return ret_val;
 }

 And in orig_function_name_foo, replace all malloc's with:
 gr_malloc(gr, orig_size);
 And if you call a function(..., &ptr, ...) which
 returns mallocd memory in ptr, do:
 gr_track(gr, ptr);

 Now, no need to remember to free *anything* before returning.
*/
#include <stdlib.h>
#include <stdio.h>
#include "gr.h"


struct int_wrapper {
	int blah;
};

int descrim(void *foo) 
{
	struct int_wrapper *iw = foo;
	if (iw->blah > RAND_MAX/2) return 1;
	return 0;
}

/* this "descriminating" function modifies the nodes
   that it is passed */
int descrim2(void *foo) 
{
	struct int_wrapper *iw = foo;
	int ret = 0;
	if (iw->blah > RAND_MAX/2) ret = 1;
	iw->blah = 42;
	return ret;
}

int iterate(gr_t gr) 
{
	iter_t iter;
	int count=0;
	struct int_wrapper *foo;

	iter = gr_iter_start(gr, descrim);
	while (foo = gr_iter_next(iter)) {
		/*printf("%d\t",foo->blah);*/
		count++;
	}
	printf("\niterate: count is %d\n", count);
	return count;
}

int my_free(void *foo) 
{
	struct int_wrapper *iw = foo;
	printf("freeing: %d\n", iw->blah);

	/* could do a "deep free" of some struct here */
	free(iw);
	/* if we had an error, we would return >0 */
	return 0;
}

/*
  The following would be a dangerous thing to do, because
  it would free the pointers but not set them to NULL in
  the gr. Double frees on gr_clear(gr) or gr_destroy(gr)

  iter_t iter = gr_iter_start(gr, my_free);
  while (gr_iter_next(iter)) ;
*/

int main() 
{
	int i,j;
	gr_t gr;
	struct int_wrapper *iw;
	int count_total = 0;
	float count_avg = 0;

	gr = gr_init(0); /* accept default size */
	srand(time(0));
	
	for (j=0; j<500; j++) {
		for (i = 0; i < 6000; i++) {
			iw = malloc(sizeof(struct int_wrapper));
			if (!iw) break;
			gr_track(gr, iw, my_free);
			iw->blah = rand();
		}
		gr_stats(gr);
		sleep(3);
		count_total += iterate(gr);
		gr_clear(gr);
	}

	gr_destroy(gr); /* must be called before program exits */

	count_avg = (float)count_total/500;
	printf("avg randomness: %.9f\n", count_avg/6000);
}
