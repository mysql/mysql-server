/*
  WL#3261 Maria - background flushing of the least-recently-dirtied pages
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* This is the interface of this module. */

/* flushes all page from LRD up to approximately rec_lsn>=max_lsn */
int flush_all_LRD_to_lsn(LSN max_lsn);
