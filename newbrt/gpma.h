#ifndef GPMA_H
#define GPMA_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// Need this to get the u_int32_t types and so forth
#include <sys/types.h>

typedef struct gpma *GPMA;
struct gitem {
    u_int32_t len;
    void *data;
};

typedef int (*gpma_compare_fun_t)(u_int32_t alen, void *aval, u_int32_t blen, void *bval, void*extra);
typedef int (*gpma_besselfun_t)(u_int32_t dlen, void *dval, void *extra);  // return a number, not an error code.
typedef int (*gpma_delete_callback_t)(u_int32_t slotnum, u_int32_t deletelen, void*deletedata, void*extra); // return 0 if OK.
// If the pma moves things around and/or changes the size of the pma, it calls this function to indicate what happened.
typedef int (*gpma_renumber_callback_t)(u_int32_t nitems,  // How many things moved
					u_int32_t *froms,  // An array of indices indicating where things moved from
					u_int32_t *tos,    // An array of indices indicating where thigns moved to
					struct gitem *items, // The actual items that were moved
					u_int32_t old_N,     // The old size of the target array
					u_int32_t new_N,     // The new size of the target array
					void *extra);        // Context
typedef void (*gpma_free_callback_t)(u_int32_t len, void*freeme, void*extra);

// initial_index_limit must be zero or a power of two.
int toku_gpma_create (GPMA*, int initial_index_limit);
/* Return 0 if OK, and sets the referenced GPMA to NULL. */
void toku_gpma_free (GPMA*, gpma_free_callback_t, void*);
// How many items are present
u_int32_t toku_gpma_n_entries (GPMA);
// What is the maximum index limit
u_int32_t toku_gpma_index_limit (GPMA);

// Require that the item not be already present, according ot the compare function
// The data in the DBT is passed in.
int toku_gpma_insert (GPMA,
		      u_int32_t len, void*data,
		      gpma_compare_fun_t comparef, void*extra_for_comparef,
		      gpma_renumber_callback_t renumberf, void*extra_for_renumberf,   // if anything gets renumbered, let the caller know
		      u_int32_t *indexp // Where did the item get stored?
		      );
// Use a bessel function to determine where to insert the data.
// Puts the new value between the rightmost -1 and the leftmost +1.
// Requires:  Nothing in the pma returns 0.
int toku_gpma_insert_bessel (GPMA pma,
			     u_int32_t len, void *data,
			     gpma_besselfun_t, void *extra_for_besself,
			     gpma_renumber_callback_t renumberf, void*extra_for_renumberf,   // if anything gets renumbered, let the caller know
			     u_int32_t *indexp // Where did the item get stored?
			     );			     

// Delete a particular index, and rebalance the tree.
int toku_gpma_delete_at_index (GPMA pma, u_int32_t index,
			       gpma_renumber_callback_t renumberf,
			       void *extra_for_renumberf);
			       

// Delete anything for which the besselfun is zero.  The besselfun must be monotonically increasing compared to the comparison function.
// That is, if two othings compare to be < then their besselfun's must yield <=, and if the compare to be = their besselfuns must be =, and if they are > then their besselfuns must be >=
// Note the delete_callback would be responsible for calling free on the object.
int toku_gpma_delete_bessel (GPMA,
			     gpma_besselfun_t,
			     void*extra_for_besself,
			     gpma_delete_callback_t,
			     void*extra_for_deletef,
			     gpma_renumber_callback_t, // if anything gets renumbered, let the caller know
			     void*extra_for_renumberf);

// Delete any items for which the compare function says things are zero.
// For each item deleted, invoke deletef.
// For any items moved around, invoke renumberf.
int toku_gpma_delete_item (GPMA,
			   u_int32_t len, void *data,
			   gpma_compare_fun_t comparef,        void *extra_for_comparef,
			   gpma_delete_callback_t deletef,     void *extra_for_deletef,
			   gpma_renumber_callback_t renumberf, void *extra_for_renumberf);

// Look up a particular item, using the compare function.  Find some X such that compf(len,data, X.len, X.data)==0
//  (Note that the len and data passed here are always passed as the first pair of arguments to compf. )
//  The item being looked up is the second pair of arguments.
int toku_gpma_lookup_item (GPMA, u_int32_t len, void *data, gpma_compare_fun_t compf, void*extra, u_int32_t *resultlen, void **resultdata, u_int32_t *idx);

// Lookup something according to the besselfun.
// If direction==0 then return something for which the besselfun is zero (or return DB_NOTFOUND and set the idx to point at the spot where the item would go.  That spot may already have an element in it, or it may be off the end.)
//   If more than one value is zero, return the leftmost such value.
// If direction>0  then return the first thing for which the besselfun is positive (or return DB_NOTFOUND).
// If direction<0  then return the last thing for which the besselfun is negative (or return DB_NOTFOUND).
int toku_gpma_lookup_bessel (GPMA, gpma_besselfun_t, int direction, void*extra, u_int32_t *len, void **data, u_int32_t *idx);
void toku_gpma_iterate (GPMA, void(*)(u_int32_t len, void*data, void*extra), void*extra);
#define GPMA_ITERATE(table,idx,vallen,val,body) ({                  \
  u_int32_t idx;                                                    \
  for (idx=0; idx<toku_gpma_index_limit(table); idx++) {            \
      u_int32_t vallen; void*val;                                   \
      if (0==toku_gpma_get_from_index(table, idx, &vallen, &val)) { \
          body;                                                     \
      } } })

int toku_gpma_valididx (GPMA, u_int32_t idx);
int toku_gpma_get_from_index (GPMA, u_int32_t idx, u_int32_t *len, void **data);

// Whatever is in the slot gets overwritten.  Watch out that you free the thing before overwriting it.
void toku_gpma_set_at_index (GPMA, u_int32_t idx, u_int32_t len, void*data);
// Clears the item at a particular index without rebalancing the PMA.
void toku_gpma_clear_at_index (GPMA, u_int32_t idx);

int toku_gpma_move_inside_pma_by_renumbering (GPMA,
					      u_int32_t nitems,
					      u_int32_t *froms, u_int32_t *tos);

int toku_gpma_split (GPMA pma, GPMA newpma, u_int32_t overhead,
		     int (*realloc_data)(u_int32_t len, void *odata, void **ndata, void *extra),
		     void *extra_realloc,
		     gpma_renumber_callback_t rcall,
		     void *extra_rcall,
		     gpma_renumber_callback_t rcall_across_pmas, // This one is called for everything that moved.  It is called first (before the rcall).  The old_N is the size of pma before resizing.
		     void *extra_rcall_across);

void toku_verify_gpma (GPMA pma);

// Change the size of the PMA.  Anything beyond the oldsize is discarded (if the newsize is smaller) or zerod (if the newsize is larger)
int toku_resize_gpma_exactly (GPMA pma, u_int32_t newsize);

#endif
