#
# Initialize a table for storing the DD PFS memory events. This is typically done
# at the beginning of an MTR test. Please source dd_pfs_cleanup.inc at the end
# of the test to remove the table and schema.
#
--disable_query_log
TRUNCATE TABLE performance_schema.memory_summary_global_by_event_name;

CREATE SCHEMA dd_pfs;

# Create a table for saving PFS state based on one of the PFS table, along with a
# sequence column and a query field.
CREATE TABLE dd_pfs.mem_events (Seq int auto_increment PRIMARY KEY, Query varchar(1000)) AS
  SELECT EVENT_NAME AS Name, COUNT_ALLOC AS N_alloc, COUNT_FREE AS N_free,
         CURRENT_COUNT_USED AS N_curr, SUM_NUMBER_OF_BYTES_ALLOC AS Sz_alloc,
         SUM_NUMBER_OF_BYTES_FREE AS Sz_free, CURRENT_NUMBER_OF_BYTES_USED AS Sz_curr,
         'savepoint' AS Query
  FROM performance_schema.memory_summary_global_by_event_name
  WHERE EVENT_NAME = 'memory/sql/dd::infrastructure' OR
        EVENT_NAME = 'memory/sql/dd::objects' OR
        EVENT_NAME = 'memory/sql/dd::String_type';

# Save the current state after the table and schema have been created, since
# the execution of the previous statement affects the counters.
--source include/dd_pfs_save_state.inc

# Turn off reporting by default.
let $report=;
--enable_query_log
