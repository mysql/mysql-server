#ifndef MEMARENA_H
#define MEMARENA_H
/* We have too many memory management tricks:
 *  mempool for a collection of objects that are all allocated together.
 *    It's pretty rigid about what happens when you run out of memory.
 *    There's a callback to compress data.
 *  memarena (this code) is for a collection of objects that cannot be moved.
 *    The pattern is allocate more and more stuff.
 *    Don't free items as you go.
 *    Free all the items at once.
 *    Then reuse the same buffer again.
 *    Allocated objects never move.
 *  A memarena (as currently implemented) is not suitable for interprocess memory sharing.  No reason it couldn't be made to work though.
 */

#include <sys/types.h>

typedef struct memarena *MEMARENA;
 
MEMARENA memarena_create (void);
// Effect: Create a memarena.  In case of ENOMEM, aborts.

void memarena_clear (MEMARENA ma);
// Effect: Reset the internal state so that the allocated memory can be used again.

void* malloc_in_memarena (MEMARENA ma, size_t size);
// Effect: Allocate some memory.  The returned value remains valid until the memarena is cleared or closed.
//  In case of ENOMEM, aborts.

void *memarena_memdup (MEMARENA ma, const void *v, size_t len);

void memarena_close(MEMARENA *ma);

void memarena_move_buffers(MEMARENA dest, MEMARENA source);
// Effect: Move all the memory from SOURCE into DEST.  When SOURCE is closed the memory won't be freed.  When DEST is closed, the memory will be freed.  (Unless DEST moves its memory to another memarena...)

#endif
