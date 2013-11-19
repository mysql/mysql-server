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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_os.h"
#include <errno.h>
#include <toku_assert.h>
#include "queue.h"
#include "memory.h"
#include <toku_pthread.h>

struct qitem;

struct qitem {
    void *item;
    struct qitem *next;
    uint64_t weight;
};

struct queue {
    uint64_t contents_weight; // how much stuff is in there?
    uint64_t weight_limit;    // Block enqueueing when the contents gets to be bigger than the weight.
    struct qitem *head, *tail;

    bool eof;

    toku_mutex_t mutex;
    toku_cond_t  cond;
};

// Representation invariant:
//   q->contents_weight is the sum of the weights of everything in the queue.
//   q->weight_limit    is the limit on the weight before we block.
//   q->head is the oldest thing in the queue.  q->tail is the newest.  (If nothing is in the queue then both are NULL)
//   If q->head is not null:
//    q->head->item is the oldest item.
//    q->head->weight is the weight of that item.
//    q->head->next is the next youngest thing.
//   q->eof indicates that the producer has said "that's all".
//   q->mutex and q->cond are used as condition variables.


int queue_create (QUEUE *q, uint64_t weight_limit)
{
    QUEUE CALLOC(result);
    if (result==NULL) return get_error_errno();
    result->contents_weight = 0;
    result->weight_limit    = weight_limit;
    result->head            = NULL;
    result->tail            = NULL;
    result->eof             = false;
    toku_mutex_init(&result->mutex, NULL);
    toku_cond_init(&result->cond, NULL);
    *q = result;
    return 0;
}

int queue_destroy (QUEUE q)
{
    if (q->head) return EINVAL;
    assert(q->contents_weight==0);
    toku_mutex_destroy(&q->mutex);
    toku_cond_destroy(&q->cond);
    toku_free(q);
    return 0;
}

int queue_enq (QUEUE q, void *item, uint64_t weight, uint64_t *total_weight_after_enq)
{
    toku_mutex_lock(&q->mutex);
    assert(!q->eof);
    // Go ahead and put it in, even if it's too much.
    struct qitem *MALLOC(qi);
    if (qi==NULL) {
	int r = get_error_errno();
	toku_mutex_unlock(&q->mutex);
	return r;
    }
    q->contents_weight += weight;
    qi->item = item;
    qi->weight = weight;
    qi->next   = NULL;
    if (q->tail) {
	q->tail->next = qi;
    } else {
	assert(q->head==NULL);
	q->head = qi;
    }
    q->tail = qi;
    // Wake up the consumer.
    toku_cond_signal(&q->cond);
    // Now block if there's too much stuff in there.
    while (q->weight_limit < q->contents_weight) {
	toku_cond_wait(&q->cond, &q->mutex);
    }
    // we are allowed to return.
    if (total_weight_after_enq) {
	*total_weight_after_enq = q->contents_weight;
    }
    toku_mutex_unlock(&q->mutex);
    return 0;
}

int queue_eof (QUEUE q)
{
    toku_mutex_lock(&q->mutex);
    assert(!q->eof);
    q->eof = true;
    toku_cond_signal(&q->cond);
    toku_mutex_unlock(&q->mutex);
    return 0;
}

int queue_deq (QUEUE q, void **item, uint64_t *weight, uint64_t *total_weight_after_deq)
{
    toku_mutex_lock(&q->mutex);
    int result;
    while (q->head==NULL && !q->eof) {
	toku_cond_wait(&q->cond, &q->mutex);
    }
    if (q->head==NULL) {
	assert(q->eof);
	result = EOF;
    } else {
	struct qitem *head = q->head;
	q->contents_weight -= head->weight;
	*item   = head->item;
	if (weight)
	    *weight = head->weight;
	if (total_weight_after_deq)
	    *total_weight_after_deq = q->contents_weight;
	q->head = head->next;
	toku_free(head);
	if (q->head==NULL) {
	    q->tail = NULL;
	}
	// wake up the producer, since we decreased the contents_weight.
	toku_cond_signal(&q->cond);
	// Successful result.
	result = 0;
    }
    toku_mutex_unlock(&q->mutex);
    return result;
}
