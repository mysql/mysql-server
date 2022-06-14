#
# Store the current state of the DD PFS memory events into the table that has been
# created for this purpose. In the 'Query' column, we put the label 'savepoint'.
#
# Note that dd_pfs_init must have been called prior to this in order to create the
# required table.
#
--disable_query_log
# If there is a previous savepoint, we will delete it.
DELETE FROM dd_pfs.mem_events WHERE Query = 'savepoint';

# Store the new state.
INSERT INTO dd_pfs.mem_events (Name, N_alloc, N_free, N_curr, Sz_alloc, Sz_free, Sz_curr, Query)
  SELECT EVENT_NAME AS Name, COUNT_ALLOC AS N_alloc, COUNT_FREE AS N_free,
         CURRENT_COUNT_USED AS N_curr, SUM_NUMBER_OF_BYTES_ALLOC AS Sz_alloc,
         SUM_NUMBER_OF_BYTES_FREE AS Sz_free, CURRENT_NUMBER_OF_BYTES_USED AS Sz_curr,
         'savepoint' AS Query
  FROM performance_schema.memory_summary_global_by_event_name
  WHERE EVENT_NAME = 'memory/sql/dd::infrastructure' OR
        EVENT_NAME = 'memory/sql/dd::objects' OR
        EVENT_NAME = 'memory/sql/dd::String_type';
--enable_query_log
