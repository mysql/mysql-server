# Experiments with thread_pool_func4 and this include file revealed that these
# three selects have a good impact on the code coverage in thread_pool.cc
# (10 extra lines covered).
#
# The main property of thread_pool_func4 is:
#    Have a lot activity of concurrent sessions which have a strong impact
#    on the content of thread_pool related information_schema tables.
#
# Therefore this include file gets sourced in all thread_pool tests which
# run with concurrent sessions.
# The printing of the result sets must be suppressed because the content
# depends on preceding tests, current load on testing box etc.
#
--echo # Printing of result sets is disabled.
--disable_result_log
SELECT * FROM information_schema.TP_THREAD_GROUP_STATS;
SELECT * FROM information_schema.TP_THREAD_GROUP_STATE;
SELECT * FROM information_schema.TP_THREAD_STATE;
SELECT * FROM performance_schema.tp_thread_group_stats;
SELECT * FROM performance_schema.tp_thread_group_state;
SELECT * FROM performance_schema.tp_thread_state;
--enable_result_log

