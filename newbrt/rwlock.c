/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Readers/writers locks implementation
 *
 *****************************************
 *     Overview
 *****************************************
 *
 * TokuDB employs readers/writers locks for the ephemeral locks (e.g.,
 * on BRT nodes) Why not just use the pthread_rwlock API?
 *
 *   1) we need multiprocess rwlocks (not just multithreaded)
 *
 *   2) pthread rwlocks are very slow since they entail a system call
 *   (about 2000ns on a 2GHz T2500.)
 *
 *     Related: We expect the common case to be that the lock is
 *     granted
 *
 *   3) We are willing to employ machine-specific instructions (such
 *   as atomic exchange, and mfence, each of which runs in about
 *   10ns.)
 *
 *   4) We want to guarantee nonstarvation (many rwlock
 *   implementations can starve the writers because another reader
 *   comes * along before all the other readers have unlocked.)
 *    
 *****************************************
 *      How it works
 *****************************************
 *
 * We arrange that the rwlock object is in the address space of both
 * threads or processes.  For processes we use mmap().
 *
 * The rwlock struct comprises the following fields
 *
 *    a long mutex field (which is accessed using xchgl() or other
 *    machine-specific instructions.  This is a spin lock.
 *
 *    a read counter (how many readers currently have the lock?)
 *
 *    a write boolean (does a writer have the lock?)
 *
 *    a singly linked list of semaphores for waiting requesters.  This
 *    list is sorted oldest requester first.  Each list element
 *    contains a semaphore (which is provided by the requestor) and a
 *    boolean indicating whether it is a reader or a writer.
 *
 * To lock a read rwlock:
 *
 *    1) Acquire the mutex.
 *
 *    2) If the linked list is not empty or the writer boolean is true
 *    then
 *
 *       a) initialize your semaphore (to 0),
 *       b) add your list element to the end of the list (with  rw="read")
 *       c) release the mutex
 *       d) wait on the semaphore
 *       e) when the semaphore release, return success.
 *
 *    3) Otherwise increment the reader count, release the mutex, and
 *    return success.
 *
 * To lock the write rwlock is almost the same.
 *     1) Acquire the mutex
 *     2) If the list is not empty or the reader count is nonzero
 *        a) initialize semaphore
 *        b) add to end of list (with rw="write")
 *        c) release mutex
 *        d) wait on the semaphore
 *        e) return success when the semaphore releases
 *     3) Otherwise set writer=TRUE, release mutex and return success.
 *
 * To unlock a read rwlock:
 *     1) Acquire mutex
 *     2) Decrement reader count
 *     3) If the count is still positive or the list is empty then
 *        return success
 *     4) Otherwise (count==zero and the list is nonempty):
 *        a) If the first element of the list is a reader:
 *            i) while the first element is a reader:
 *                 x) pop the list
 *                 y) increment the reader count
 *                 z) increment the semaphore (releasing it for some waiter)
 *            ii) return success
 *        b) Else if the first element is a writer
 *            i) pop the list
 *            ii) set writer to TRUE
 *            iii) increment the semaphore
 *            iv) return success
 */

