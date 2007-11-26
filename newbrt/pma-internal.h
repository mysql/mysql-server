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
    int dup_mode;
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
    pma_compare_fun_t compare_fun;
    pma_compare_fun_t dup_compare_fun;
    DB *db;            /* Passed to the compare functions. */
    FILENUM filenum;   /* Passed to logging. */
    void *skey, *sval; /* used in dbts */
    struct mempool kvspace;
};

int toku_pmainternal_count_region (struct kv_pair *pairs[], int lo, int hi);
void toku_pmainternal_calculate_parameters (PMA pma);
int toku_pmainternal_smooth_region (struct kv_pair *pairs[], int n, int idx, int base, PMA pma);
int toku_pmainternal_printpairs (struct kv_pair *pairs[], int N);
int toku_pmainternal_make_space_at (PMA pma, int idx);
int toku_pmainternal_find (PMA pma, DBT *); // The DB is so the comparison fuction can be called.
void toku_print_pma (PMA pma); /* useful for debugging, so keep the name short. I.e., not pmainternal_print_pma() */

/* density thresholds */
#define PMA_LDT_HIGH  0.25
#define PMA_LDT_LOW  0.40
#define PMA_UDT_HIGH  1.00
#define PMA_UDT_LOW  0.50

/* minimum array size */
#define PMA_MIN_ARRAY_SIZE 4
