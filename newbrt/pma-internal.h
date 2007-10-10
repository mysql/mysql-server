#include "pma.h"
#include "mempool.h"

struct pma_cursor {
    PMA pma;
    int position; /* -1 if the position is undefined. */
    struct list next;
    void *skey, *sval; /* used in dbts. */ 
};

struct pma {
    enum typ_tag tag;
    int N;               /* How long is the array? Always a power of two >= 4. */
    int n_pairs_present; /* How many array elements are non-null.         */
    struct kv_pair **pairs;
    int uplgN;           /* The smallest power of two >= lg(N)            */
    double udt_step;     /* upper density threshold step */     
                         /* Each doubling decreases the density by density step.
			  * For example if array_len=256 and uplgN=8 then there are 5 doublings.
			  * Regions of size 8 are full.  Regions of size 16 are 90% full.
			  *  Regions of size 32 are 80% full.  Regions of size 64 are 70% full.
			  *  Regions of size 128 are 60% full.  Regions of size 256 are 50% full.
			  *  The density step is 0.10. */
    double ldt_step;     /* lower density threshold step */
    struct list cursors;
    int (*compare_fun)(DB*,const DBT*,const DBT*);
    void *skey, *sval; /* used in dbts */
    struct mempool kvspace;
};

int pmainternal_count_region (struct kv_pair *pairs[], int lo, int hi);
void pmainternal_calculate_parameters (PMA pma);
int pmainternal_smooth_region (struct kv_pair *pairs[], int n, int idx, int base, PMA pma);
int pmainternal_printpairs (struct kv_pair *pairs[], int N);
int pmainternal_make_space_at (PMA pma, int idx);
int pmainternal_find (PMA pma, DBT *, DB*); // The DB is so the comparison fuction can be called.
void print_pma (PMA pma); /* useful for debugging, so keep the name short. I.e., not pmainternal_print_pma() */

/*
 * resize the pma array to asksize.  zero all array entries starting from startx.
 */
int __pma_resize_array(PMA pma, int asksize, int startx);

/*
 * extract pairs from the pma in the window delimited by lo and hi.
 */
struct kv_pair_tag *__pma_extract_pairs(PMA pma, int count, int lo, int hi);

/*
 * update the cursors in a cursor set given a set of tagged pairs.
 */
void __pma_update_cursors(PMA pma, struct list *cursorset, struct kv_pair_tag *tpairs, int n);

/*
 * update this pma's cursors given a set of tagged pairs.
 */
void __pma_update_my_cursors(PMA pma, struct kv_pair_tag *tpairs, int n);

/*
 * a deletion occured at index "here" in the pma.  rebalance the windows around "here".  if
 * necessary, shrink the pma.
 */
void __pma_delete_at(PMA pma, int here);

/*
 * if the pma entry at here is deleted and there are no more references to it
 * then finish the deletion
 */
void __pma_delete_resume(PMA pma, int here);

/*
 * finish a deletion from the pma. called when there are no cursor references
 * to the kv pair.
 */
void __pma_delete_finish(PMA pma, int here);

/*
 * count the number of cursors that reference a pma pair
 */
int __pma_count_cursor_refs(PMA pma, int here);

/* density thresholds */
#define PMA_LDT_HIGH  0.25
#define PMA_LDT_LOW  0.40
#define PMA_UDT_HIGH  1.00
#define PMA_UDT_LOW  0.50

/* minimum array size */
#define PMA_MIN_ARRAY_SIZE 4
