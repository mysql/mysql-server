# Auxiliary routine for debugging
#
# The content of all "pool of threads" related tables within the information_schema
# is printed in case of $DEBUG= 1.

if($DEBUG)
{
   SELECT * FROM information_schema.processlist;
   SELECT * FROM information_schema.TP_THREAD_GROUP_STATS;
   SELECT * FROM information_schema.TP_THREAD_GROUP_STATE;
   SELECT TP_GROUP_ID,TP_THREAD_NUMBER,PROCESS_COUNT,WAIT_TYPE
   FROM information_schema.TP_THREAD_STATE;
   SELECT * FROM performance_schema.tp_thread_group_stats;
   SELECT * FROM performance_schema.tp_thread_group_state;
   SELECT TP_GROUP_ID,TP_THREAD_NUMBER,PROCESS_COUNT,WAIT_TYPE
   FROM performance_schema.tp_thread_state;
}
