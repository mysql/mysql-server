#include "memory.h"

struct gpma {
    enum typ_tag tag;
    unsigned int N;      /* How long is the array? Always a power of two >= 4. */
    u_int32_t n_items_present; /* How many array elements are non-null.         */
    struct gitem *items; /* A malloced array.  If any item's DATA is null, then it's not in use. */
    

    double udt_step;     /* upper density threshold step */     
                         /* Each doubling decreases the density by density step.
			  * For example if array_len=256 and uplgN=8 then there are 5 doublings.
			  * Regions of size 8 are full.  Regions of size 16 are 90% full.
			  *  Regions of size 32 are 80% full.  Regions of size 64 are 70% full.
			  *  Regions of size 128 are 60% full.  Regions of size 256 are 50% full.
			  *  The density step is 0.10. */
    double ldt_step;     /* lower density threshold step */
};

#define GPMA_MIN_ARRAY_SIZE 4

/* density thresholds */
#define GPMA_LDT_HIGH  0.25
#define GPMA_LDT_LOW  0.40
#define GPMA_UDT_HIGH  1.00
#define GPMA_UDT_LOW  0.50

/* Expose these for testing purposes */
u_int32_t toku_gpma_find_index_bes (GPMA pma, gpma_besselfun_t besf, int direction, void *extra, int *found);
u_int32_t toku_gpma_find_index (GPMA pma, u_int32_t len, void *data, gpma_compare_fun_t compare, void *extra, int *found);
int toku_lg (unsigned int n);
u_int32_t toku_hyperceil (u_int32_t v);
int toku_max_int (int, int);
int toku_gpma_smooth_region (GPMA pma,
			     u_int32_t lo, u_int32_t hi,
			     u_int32_t count, // The number of nonnull values
			     u_int32_t idx, u_int32_t *newidxp, gpma_renumber_callback_t rcall, void *extra,
			     u_int32_t old_N);
int toku_make_space_at (GPMA pma, u_int32_t idx, u_int32_t *newidx, gpma_renumber_callback_t rcall, void *extra);

void toku_gpma_distribute (GPMA pma,
			   u_int32_t lo, u_int32_t hi,
			   u_int32_t count,
			   struct gitem *items, // some of these may be NULL data, be we leave space for them anyway.
			   /*out*/ u_int32_t *tos   // the indices where the values end up (we fill this in)
			   );
int toku_smooth_deleted_region (GPMA pma, u_int32_t minidx, u_int32_t maxidx, gpma_renumber_callback_t renumberf, void *extra_for_renumberf);
