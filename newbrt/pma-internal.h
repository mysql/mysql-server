#include "pma.h"

struct pair {
    bytevec key; /* NULL for empty slots */
    int keylen;
    bytevec val;
    int vallen;
};

struct pma_cursor {
    PMA pma;
    int position; /* -1 if the position is undefined. */
    PMA_CURSOR next,prev;
    void *skey, *sval; /* used in dbts. */ 
};

struct pma {
    enum typ_tag tag;
    int N;               /* How long is the array? Always a power of two >= 4. */
    int n_pairs_present; /* How many array elements are non-null.         */
    struct pair *pairs;
    int uplgN;           /* The smallest power of two >= lg(N)            */
    double densitystep;  /* Each doubling decreases the density by densitystep.
			  * For example if array_len=256 and uplgN=8 then there are 5 doublings.
			  * Regions of size 8 are full.  Regions of size 16 are 90% full.
			  *  Regions of size 32 are 80% full.  Regions of size 64 are 70% full.
			  *  Regions of size 128 are 60% full.  Regions of size 256 are 50% full.
			  *  The densitystep is 0.10. */
    PMA_CURSOR cursors_head, cursors_tail;
    int (*compare_fun)(DB*,const DBT*,const DBT*);
    void *skey, *sval; /* used in dbts */
};

int pmainternal_count_region (struct pair *pairs, int lo, int hi);
void pmainternal_calculate_parameters (PMA pma);
int pmainternal_smooth_region (struct pair *pairs, int n, int idx);
int pmainternal_printpairs (struct pair *pairs, int N);
int pmainternal_make_space_at (PMA pma, int idx);
int pmainternal_find (PMA pma, DBT *, DB*); // The DB is so the comparison fuction can be called.
void print_pma (PMA pma); /* useful for debugging, so keep the name short. I.e., not pmainternal_print_pma() */
int pmainternal_init_array(PMA pma, int asksize);
struct pair *pmainternal_extract_pairs(PMA pma);
