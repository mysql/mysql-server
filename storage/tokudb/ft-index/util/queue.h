/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

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

int toku_queue_create (QUEUE *q, uint64_t weight_limit);
// Effect: Create a queue with a given weight limit.  The queue is initially empty.

int toku_queue_enq (QUEUE q, void *item, uint64_t weight, uint64_t *total_weight_after_enq);
// Effect: Insert ITEM of weight WEIGHT into queue.  If the resulting contents weight too much then block (don't return) until the total weight is low enough.
// If total_weight_after_enq!=NULL then return the current weight of the items in the queue (after finishing blocking on overweight, and after enqueueing the item).
// If successful return 0.
// If an error occurs, return the error number, and the state of the queue is undefined.  The item may have been enqueued or not, and in fact the queue may be badly corrupted if the condition variables go awry.  If it's just a matter of out-of-memory, then the queue is probably OK.
// Requires: There is only a single consumer. (We wake up the consumer using a pthread_cond_signal (which is suitable only for single consumers.)

int toku_queue_eof (QUEUE q);
// Effect: Inform the queue that no more values will be inserted.  After all the values that have been inserted are dequeued, further dequeue operations will return EOF.
// Returns 0 on success.   On failure, things are pretty bad (likely to be some sort of mutex failure).

int toku_queue_deq (QUEUE q, void **item, uint64_t *weight, uint64_t *total_weight_after_deq);
// Effect: Wait until the queue becomes nonempty.  Then dequeue and return the oldest item.  The item and its weight are returned in *ITEM.
// If weight!=NULL then return the item's weight in *weight.
// If total_weight_after_deq!=NULL then return the current weight of the items in the queue (after dequeuing the item).
// Return 0 if an item is returned.
// Return EOF is we no more items will be returned.
// Usage note: The queue should be destroyed only after any consumers will no longer look at it (for example, they saw EOF).

int toku_queue_destroy (QUEUE q);
// Effect: Destroy the queue.
// Requires: The queue must be empty and no consumer should try to dequeue after this (one way to do this is to make sure the consumer saw EOF).
// Returns 0 on success.   If the queue is not empty, returns EINVAL.  Other errors are likely to be bad (some sort of mutex or condvar failure).

