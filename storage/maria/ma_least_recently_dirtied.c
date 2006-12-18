/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  This thread does background flush of pieces of the LRD, and serves
  requests for asynchronous checkpoints.
  Just launch it when engine starts.
  MikaelR questioned why the same thread does two different jobs, the risk
  could be that while a checkpoint happens no LRD flushing happens.
  For now, we only do checkpoints - no LRD flushing (to be done when the
  second version of the page cache is ready WL#3077).
  Reasons to delay:
  - Recovery will work (just slower)
  - new page cache may be different, why do then re-do
  - current pagecache probably has issues with flushing when somebody is
  writing to the table being flushed - better avoid that.
*/
pthread_handler_decl background_flush_and_checkpoint_thread()
{
  while (this_thread_not_killed)
  {
    /* note that we don't care of the checkpoint's success */
    (void)execute_asynchronous_checkpoint_if_any();
    sleep(5);
    /*
      in the final version, we will not sleep but call flush_pages_from_LRD()
      repeatedly. If there are no dirty pages, we'll make sure to not have a
      tight loop probing for checkpoint requests.
    */
  }
}

/* The rest of this file will not serve in first version */

/*
  flushes only the first pages of the LRD.
  max_this_number could be FLUSH_CACHE (of mf_pagecache.c) for example.
*/
flush_pages_from_LRD(uint max_this_number, LSN max_this_lsn)
{
  /*
    One rule to better observe is "page must be flushed to disk before it is
    removed from LRD" (otherwise checkpoint is incomplete info, corruption).
  */

  /*
    Build a list of pages to flush:
    changed_blocks[i] is roughly sorted by descending rec_lsn,
    so we could do a merge sort of changed_blocks[] lists, stopping after we
    have the max_this_number first elements or after we have found a page with
    rec_lsn > max_this_lsn.
    Then do like pagecache_flush_blocks_int() does (beware! this time we are
    not alone on the file! there may be dangers! TODO: sort this out).
  */

  /*
    MikaelR noted that he observed that Linux's file cache may never fsync to
    disk until this cache is full, at which point it decides to empty the
    cache, making the machine very slow. A solution was to fsync after writing
    2 MB.
  */
}

/*
  Note that when we flush all page from LRD up to rec_lsn>=max_lsn,
  this is approximate because the LRD list may
  not be exactly sorted by rec_lsn (because for a big row, all pages of the
  row are inserted into the LRD with rec_lsn being the LSN of the REDO for the
  first page, so if there are concurrent insertions, the last page of the big
  row may have a smaller rec_lsn than the previous pages inserted by
  concurrent inserters).
*/
