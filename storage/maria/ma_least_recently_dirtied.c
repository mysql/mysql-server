/*
  WL#3261 Maria - background flushing of the least-recently-dirtied pages
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/*
  To be part of the page cache.
  The pseudocode below is dependent on the page cache
  which is being designed WL#3134. It is not clear if I need to do page
  copies, as the page cache already keeps page copies.
  So, this code will move to the page cache and take inspiration from its
  methods. Below is just to give the idea of what could be done.
  And I should compare my imaginations to WL#3134.
*/

/* Here is the implementation of this module */

#include "page_cache.h"
#include "least_recently_dirtied.h"

/*
  MikaelR suggested removing this global_LRD_mutex (I have a paper note of
  comments), however at least for the first version we'll start with this
  mutex (which will be a LOCK-based atomic_rwlock).
*/
pthread_mutex_t global_LRD_mutex; 

/*
  When we flush a page, we should pin page.
  This "pin" is to protect against that:
  I make copy,
  you modify in memory and flush to disk and remove from LRD and from cache,
  I write copy to disk,
  checkpoint happens.
  result: old page is on disk, page is absent from LRD, your REDO will be
  wrongly ignored.

  Pin: there can be multiple pins, flushing imposes that there are zero pins.
  For example, pin could be a uint counter protected by the page's latch.

  Maybe it's ok if when there is a page replacement, the replacer does not
  remove page from the LRD (it would save global mutex); for that, background
  flusher should be prepared to see pages in the LRD which are not in the page
  cache (then just ignore them). However checkpoint will contain superfluous
  entries and so do more work.
*/

#define PAGE_SIZE (16*1024) /* just as an example */
/*
  Optimization:
  LRD flusher should not flush pages one by one: to be fast, it flushes a
  group of pages in sequential disk order if possible; a group of pages is just
  FLUSH_GROUP_SIZE pages.
  Key cache has groupping already somehow Monty said (investigate that).
*/
#define FLUSH_GROUP_SIZE 512 /* 8 MB */
/*
  We don't want to probe for checkpoint requests all the time (it takes
  the log mutex).
  If FLUSH_GROUP_SIZE is 8MB, assuming a local disk which can write 30MB/s
  (1.8GB/min), probing every 16th call to flush_one_group_from_LRD() is every
  16*8=128MB which is every 128/30=4.2second.
  Using a power of 2 gives a fast modulo operation.
*/
#define CHECKPOINT_PROBING_PERIOD_LOG2 4

/*
  This thread does background flush of pieces of the LRD, and all checkpoints.
  Just launch it when engine starts.
  MikaelR questioned why the same thread does two different jobs, the risk
  could be that while a checkpoint happens no LRD flushing happens.
*/
pthread_handler_decl background_flush_and_checkpoint_thread()
{
  char *flush_group_buffer= my_malloc(PAGE_SIZE*FLUSH_GROUP_SIZE);
  uint flush_calls= 0;
  while (this_thread_not_killed)
  {
    if ((flush_calls++) & ((2<<CHECKPOINT_PROBING_PERIOD_LOG2)-1) == 0)
      execute_asynchronous_checkpoint_if_any();
    lock(global_LRD_mutex);
    flush_one_group_from_LRD();
    safemutex_assert_not_owner(global_LRD_mutex);
    /*
      We are a background thread, leave time for client threads or we would
      monopolize the disk:
    */
    pthread_yield();
  }
  my_free(flush_group_buffer);
}

/*
  flushes only the first FLUSH_GROUP_SIZE pages of the LRD.
*/
flush_one_group_from_LRD()
{
  char *ptr;
  safe_mutex_assert_owner(global_LRD_mutex);

  for (page= 0; page<FLUSH_GROUP_SIZE; page++)
  {
    copy_element_to_array;
  }
  /*
    One rule to better observe is "page must be flushed to disk before it is
    removed from LRD" (otherwise checkpoint is incomplete info, corruption).
  */
  unlock(global_LRD_mutex);
  /* page id is concatenation of "file id" and "number of page in file" */
  qsort(array, sizeof(*element), FLUSH_GROUP_SIZE, by_page_id);
  for (scan_array)
  {
    if (page_cache_latch(page_id, READ) == PAGE_ABSENT)
    {
      /*
        page disappeared since we made the copy (it was flushed to be
        replaced), remove from array (memcpy tail of array over it)...
      */
      continue;
    }
    memcpy(flush_group_buffer+..., page->data, PAGE_SIZE);
    pin_page;
    page_cache_unlatch(page_id, KEEP_PINNED); /* but keep pinned */
  }
  for (scan_the_array)
  {
    /*
      As an optimization, we try to identify contiguous-in-the-file segments (to
      issue one big write()).
      In non-optimized version, contiguous segment is always only one page.
    */
    if ((next_page.page_id - this_page.page_id) == 1)
    {
      /*
        this page and next page are in same file and are contiguous in the
        file: add page to contiguous segment...
      */
      continue; /* defer write() to next pages */
    }
    /* contiguous segment ends */
    my_pwrite(file, contiguous_segment_start_offset, contiguous_segment_size);

    /*
      note that if we had doublewrite, doublewrite buffer may prevent us from
      doing this write() grouping (if doublewrite space is shorter).
    */
  }
  /*
    Now remove pages from LRD. As we have pinned them, all pages that we
    managed to pin are still in the LRD, in the same order, we can just cut
    the LRD at the last element of "array". This is more efficient that
    removing element by element (which would take LRD mutex many times) in the
    loop above.
  */
  lock(global_LRD_mutex);
  /* cut LRD by bending LRD->first, free cut portion... */
  unlock(global_LRD_mutex);
  for (scan_array)
  {
    /*
      if the page has a property "modified since last flush" (i.e. which is
      redundant with the presence of the page in the LRD, this property can
      just be a pointer to the LRD element) we should reset it
      (note that then the property would live slightly longer than
      the presence in LRD).
    */
    page_cache_unpin(page_id);
    /*
      order between unpin and removal from LRD is not clear, depends on what
      pin actually is.
    */
  }
  free(array);
  /*
    MikaelR noted that he observed that Linux's file cache may never fsync to
    disk until this cache is full, at which point it decides to empty the
    cache, making the machine very slow. A solution was to fsync after writing
    2 MB.
  */
}

/*
  Flushes all page from LRD up to approximately rec_lsn>=max_lsn.
  This is approximate because we flush groups, and because the LRD list may
  not be exactly sorted by rec_lsn (because for a big row, all pages of the
  row are inserted into the LRD with rec_lsn being the LSN of the REDO for the
  first page, so if there are concurrent insertions, the last page of the big
  row may have a smaller rec_lsn than the previous pages inserted by
  concurrent inserters).
*/
int flush_all_LRD_to_lsn(LSN max_lsn)
{
  lock(global_LRD_mutex);
  if (max_lsn == MAX_LSN) /* don't want to flush forever, so make it fixed: */
    max_lsn= LRD->first->prev->rec_lsn;
  while (LRD->first->rec_lsn < max_lsn)
  {
    if (flush_one_group_from_LRD()) /* will unlock LRD mutex */
      return 1;
    /*
      The scheduler may preempt us here as we released the mutex; this is good.
    */
    lock(global_LRD_mutex);
  }
  unlock(global_LRD_mutex);
  return 0;
}
