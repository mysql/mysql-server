#include <errno.h>
#include <toku_assert.h>
#include "queue.h"
#include "memory.h"
#include "toku_pthread.h"

struct qitem;

struct qitem {
    void *item;
    struct qitem *next;
    u_int64_t weight;
};

struct queue {
    u_int64_t contents_weight; // how much stuff is in there?
    u_int64_t weight_limit;    // Block enqueueing when the contents gets to be bigger than the weight.
    struct qitem *head, *tail;

    BOOL eof;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;
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


int queue_create (QUEUE *q, u_int64_t weight_limit)
{
    QUEUE MALLOC(result);
    if (result==NULL) return errno;
    result->contents_weight = 0;
    result->weight_limit    = weight_limit;
    result->head            = NULL;
    result->tail            = NULL;
    result->eof             = FALSE;
    int r;
    r = toku_pthread_mutex_init(&result->mutex, NULL);
    if (r!=0) {
	toku_free(result);
	return r;
    }
    r = toku_pthread_cond_init(&result->cond, NULL);
    if (r!=0) {
	toku_pthread_mutex_destroy(&result->mutex);
	toku_free(result);
	return r;
    }
    *q = result;
    return 0;
}

int queue_destroy (QUEUE q)
{
    if (q->head) return EINVAL;
    assert(q->contents_weight==0);
    {
	int r = toku_pthread_mutex_destroy(&q->mutex);
	if (r) return r;
    }
    {
	int r = toku_pthread_cond_destroy(&q->cond);
	if (r) return r;
    }
    toku_free(q);
    return 0;
}

int queue_enq (QUEUE q, void *item, u_int64_t weight, u_int64_t *total_weight_after_enq)
{
    {
	int r = toku_pthread_mutex_lock(&q->mutex);
	if (r) return r;
    }
    assert(!q->eof);
    // Go ahead and put it in, even if it's too much.
    q->contents_weight += weight;
    struct qitem *MALLOC(qi);
    if (qi==NULL) return errno;
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
    {
	int r = toku_pthread_cond_signal(&q->cond);
	if (r) return r;
    }
    // Now block if there's too much stuff in there.
    while (q->weight_limit < q->contents_weight) {
	int r = toku_pthread_cond_wait(&q->cond, &q->mutex);
	if (r) return r;
    }
    // we are allowed to return.
    if (total_weight_after_enq) {
	*total_weight_after_enq = q->contents_weight;
    }
    {
	int r = toku_pthread_mutex_unlock(&q->mutex);
	if (r) return r;
    }
    return 0;
}

int queue_eof (QUEUE q)
{
    {
	int r = toku_pthread_mutex_lock(&q->mutex);
	if (r) return r;
    }
    assert(!q->eof);
    q->eof = TRUE;
    {
	int r = toku_pthread_cond_signal(&q->cond);
	if (r) return r;
    }
    {
	int r = toku_pthread_mutex_unlock(&q->mutex);
	if (r) return r;
    }
    return 0;
}

int queue_deq (QUEUE q, void **item, u_int64_t *weight, u_int64_t *total_weight_after_deq)
{
    {
	int r = toku_pthread_mutex_lock(&q->mutex);
	if (r) return r;
    }
    int result;
    while (q->head==NULL && !q->eof) {
	int r = toku_pthread_cond_wait(&q->cond, &q->mutex);
	if (r) return r;
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
	int r = toku_pthread_cond_signal(&q->cond);
	if (r!=0) return r;
	// Successful result.
	result = 0;
    }
    {
	int r = toku_pthread_mutex_unlock(&q->mutex);
	if (r) return r;
    }
    return result;
}
