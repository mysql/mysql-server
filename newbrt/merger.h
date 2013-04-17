#ifndef TOKU_MERGER_H
#define TOKU_MERGER_H

/* This is a C header (no Cilk or C++ inside here) */

/* The merger abstraction:
 *
 * This module implements a multithreaded file merger, specialized for the temporary file format used by the loader.
 * The input files have rows stored as follows
 *     <keylen (4 byte integer in native order)>
 *     <key    (char[keylen])>
 *     <vallen (4 byte integer in native order)>
 *     <val    (char[vallen])>
 * The input files are sorted according to the comparison function.
 *
 * Given a bunch of input files each containing rows, the merger can produce the minimal row from all those files.
 *
 * The merger periodically asks for memory, and the allocated memory may go up or down.  If the memory allocation increases, the merger may malloc() more memory.
 * If the memory allocation decreases, the merger should free some memory.
 *
 * Implementation hints:  The merger should double buffer its input.
 *    That is, for each file, the merger should use two buffers.  It should fill the first buffer, and then in the background fill the other buffer.
 *    Whenever a buffer empties, we hope that the other buffer is full (if not we wait) and we swap buffers, and then have the background thread fill the other buffer.
 *    This strategy implies that there is a background thread filling those other buffers.
 *    The background thread may have several refillable buffers to choose from at any moment.
 *    There are two obvious approaches for choosing which buffer to refill next:
 *     1) Refill the one that's been empty the longest.
 *     2) Refill the one for which the "front" of the buffer is the most empty.
 *    The advantage of approach (1) is that it's simple, and less likely to have race conditions.
 *    The advantage of approach (2) is that if some buffer gets emptied quickly we start refilling it earlier, possibly avoiding a pipeline stall.
 *      This could be an issue if the data was already sorted, so that file[0] is always emptying first, then file[1], and so forth.
 */
#include "db.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef struct merger *MERGER;
typedef void (*MEMORY_ALLOCATION_UPDATER) (/*in */ size_t currently_using,
					   /*in */ size_t currently_requested,
					   /*out*/size_t *new_allocation);
typedef int  (*COMPARISON_FUNCTION) (DB *db, const DBT *keya, const DBT *keyb);
MERGER create_merger (int n_files, char *file_names[n_files], DB *db, COMPARISON_FUNCTION f, MEMORY_ALLOCATION_UPDATER mup);
// Effect: Create a new merger, which will merge the files named by file_names.  
// The comparison function, f, decides which rows are smaller when they come from different files.
// The merger calls mup, a memory allocation updater, periodically, with three arguments:
//     currently_using       how much memory is the merger currently using.
//     currently_requested   how much total memory would the merger like.
//     new_allocation        (out) how much memory the system says the merger may have.  If new_allocation is more than currently_using, then the merger
//                           may allocate more memory (up to the new allocation).  If new_allocation is less, then the merger must free some memory,
//                           (and it should call the mup function again to indicate that the memory has been reduced).

void merger_close (MERGER);
// Effect: Close the files and free the memory used by the merger.

int merger_pop (MERGER m,
		/*out*/ DBT *key,
		/*out*/ DBT *val);
// Effect: If there are any rows left then return the minimal row in *key and *val. 
//   The pointers to key and val remain valid until the next call to merger_pop or merger_close.  That is, we force the flags to be 0 in the DBT.
// Requires: The flags in the dbts must be zero.
// Rationale:  We are trying to make this path as fast as possible, so we don't want to copy the data unnecessarily, and we don't want to mess around with DB_DBT_MALLOC and so forth.
//   It is fairly straightforward to keep the key and val "live":  In most cases, the buffer is still valid.  In the case where the key and val are the last
//    item, then we must take care not to reuse the buffer until the next merger_pop.

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif


#endif
