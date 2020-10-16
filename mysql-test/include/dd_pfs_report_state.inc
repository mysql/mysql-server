###############################################################################
# Report the change in the DD PFS memory events into the MTR test log, if the
# $report variable is set to 'yes' (default is 'no'). Assert specific changes
# in the counter events, if requested. Finally, store a new savepoint in order
# to be ready for testing a new query.
#

echo ###############################################################################;

--disable_query_log
--disable_result_log
# Execute the submitted operation.
if ($operation != '')
{
  eval $operation;
}

--enable_result_log

# Report the change in the memory events if requested.
if ($report == 'yes')
{
  SELECT
         after_stats.EVENT_NAME                                           AS Name,
         after_stats.COUNT_ALLOC                  - before_stats.N_alloc  AS N_alloc,
         after_stats.COUNT_FREE                   - before_stats.N_free   AS N_free,
         after_stats.CURRENT_COUNT_USED           - before_stats.N_curr   AS N_curr,
         after_stats.SUM_NUMBER_OF_BYTES_ALLOC    - before_stats.Sz_alloc AS Sz_alloc,
         after_STATS.SUM_NUMBER_OF_BYTES_FREE     - before_stats.Sz_free  AS Sz_free,
         after_stats.CURRENT_NUMBER_OF_BYTES_USED - before_stats.Sz_curr  AS Sz_curr
    FROM performance_schema.memory_summary_global_by_event_name AS after_stats,
         dd_pfs.mem_events                                      AS before_stats
    WHERE (after_stats.EVENT_NAME = 'memory/sql/dd::infrastructure' OR
           after_stats.EVENT_NAME = 'memory/sql/dd::objects' OR
           after_stats.EVENT_NAME = 'memory/sql/dd::String_type') AND
          (after_stats.EVENT_NAME = before_stats.Name) AND
          (before_stats.Query = 'savepoint')
    ORDER BY Seq;
}

if ($report == 'leftovers')
{
  SELECT
         SUM(after_stats.CURRENT_COUNT_USED) - SUM(before_stats.N_curr)   AS N_left,
         SUM(after_stats.CURRENT_NUMBER_OF_BYTES_USED) - SUM(before_stats.Sz_curr)  AS Sz_left
    FROM performance_schema.memory_summary_global_by_event_name AS after_stats,
         dd_pfs.mem_events                                      AS before_stats
    WHERE (after_stats.EVENT_NAME = 'memory/sql/dd::infrastructure' OR
           after_stats.EVENT_NAME = 'memory/sql/dd::objects' OR
           after_stats.EVENT_NAME = 'memory/sql/dd::String_type') AND
          (after_stats.EVENT_NAME = before_stats.Name) AND
          (before_stats.Query = 'savepoint')
    ORDER BY Seq;
}

# Assert specific changes in the events if requested.
if ($object_alloc != '')
{
  SET @diff = (SELECT COUNT_ALLOC - N_alloc
                 FROM performance_schema.memory_summary_global_by_event_name,
                      dd_pfs.mem_events
                 WHERE EVENT_NAME = 'memory/sql/dd::objects' AND
                       EVENT_NAME = Name AND
                       Query = 'savepoint');
  let $assert_cond= @diff = $object_alloc;
  let $assert_text = '$operation' will allocate $object_alloc objects.;
  source include/assert.inc;

  let $object_alloc=;
}

if ($object_free != '')
{
  SET @diff = (SELECT COUNT_FREE - N_free
                 FROM performance_schema.memory_summary_global_by_event_name,
                      dd_pfs.mem_events
                 WHERE EVENT_NAME = 'memory/sql/dd::objects' AND
                       EVENT_NAME = Name AND
                       Query = 'savepoint');
  let $assert_cond= @diff = $object_free;
  let $assert_text = '$operation' will free $object_free objects.;
  source include/assert.inc;

  let $object_free=;
}

if ($object_count != '')
{
  SET @diff = (SELECT CURRENT_COUNT_USED - N_curr
                 FROM performance_schema.memory_summary_global_by_event_name,
                      dd_pfs.mem_events
                 WHERE EVENT_NAME = 'memory/sql/dd::objects' AND
                       EVENT_NAME = Name AND
                       Query = 'savepoint');
  let $assert_cond= @diff = $object_count;
  let $assert_text = '$operation' will leave $object_count objects.;
  source include/assert.inc;

  let $object_count=;
}

let $operation=;

# Implicitly store a new savepoint, and delete the old one. If the test needs to do
# additional tasks that will obsolete the savepoint, the test itself may call
# dd_pfs_save_state when it is ready to execute the next query to be tested.
--source include/dd_pfs_save_state.inc
--enable_query_log
