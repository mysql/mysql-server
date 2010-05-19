#ifndef TOKU_QUEUE_H
#define TOKU_QUEUE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brttypes.h"
#include "c_dialects.h"

C_BEGIN

// The abstraction:
//
// queue.h implements a queue suitable for a producer-consumer relationship between two pthreads.
// The enqueue/dequeue operation is fairly heavyweight (involving pthread condition variables) so it may be useful
// to enqueue large chunks rather than small chunks.
// It probably won't work right to have two consumer threads.
//
// Every item inserted into the queue has a weight.  If the weight
// gets too big, then the queue blocks on trying to insert more items.
// The weight can be used to limit the total number of items in the
// queue (weight of each item=1) or the total memory consumed by queue
// items (weight of each item is its size).  Or the weight's could all be
// zero for an unlimited queue.

typedef struct queue *QUEUE;

int queue_create (QUEUE *q, u_int64_t weight_limit);
// Effect: Create a queue with a given weight limit.  The queue is initially empty.

int queue_enq (QUEUE q, void *item, u_int64_t weight, u_int64_t *total_weight_after_enq);
// Effect: Insert ITEM of weight WEIGHT into queue.  If the resulting contents weight too much then block (don't return) until the total weight is low enough.
// If total_weight_after_enq!=NULL then return the current weight of the items in the queue (after finishing blocking on overweight, and after enqueueing the item).
// If successful return 0.
// If an error occurs, return the error number, and the state of the queue is undefined.  The item may have been enqueued or not, and in fact the queue may be badly corrupted if the condition variables go awry.  If it's just a matter of out-of-memory, then the queue is probably OK.
// Requires: There is only a single consumer. (We wake up the consumer using a pthread_cond_signal (which is suitable only for single consumers.)

int queue_eof (QUEUE q);
// Effect: Inform the queue that no more values will be inserted.  After all the values that have been inserted are dequeued, further dequeue operations will return EOF.
// Returns 0 on success.   On failure, things are pretty bad (likely to be some sort of mutex failure).

int queue_deq (QUEUE q, void **item, u_int64_t *weight, u_int64_t *total_weight_after_deq);
// Effect: Wait until the queue becomes nonempty.  Then dequeue and return the oldest item.  The item and its weight are returned in *ITEM.
// If weight!=NULL then return the item's weight in *weight.
// If total_weight_after_deq!=NULL then return the current weight of the items in the queue (after dequeuing the item).
// Return 0 if an item is returned.
// Return EOF is we no more items will be returned.
// Usage note: The queue should be destroyed only after any consumers will no longer look at it (for example, they saw EOF).

int queue_destroy (QUEUE q);
// Effect: Destroy the queue.
// Requires: The queue must be empty and no consumer should try to dequeue after this (one way to do this is to make sure the consumer saw EOF).
// Returns 0 on success.   If the queue is not empty, returns EINVAL.  Other errors are likely to be bad (some sort of mutex or condvar failure).

C_END
#endif
