/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
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
    QUEUE MALLOC(result);
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
