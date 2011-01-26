##############################################################################
#
# binlog_start_pos is the postion of the the first event in the binary log
# which follows the Format description event. Intended to reduce test suite
# dependance on the Format description event length changes (e.g. in case
# of adding new events). Evaluated as:
#  
#   binlog_start_pos = 4  /* binlog header */ +
#                      (Format_description_log_event length)
#
#   Format_description_log_event length = 
#                      19 /* event common header */ +
#                      57 /* misc stuff in the Format description header */ +
#                      number of events.
# 
# With current number of events = 160,
#
#   binlog_start_pos = 4 + 19 + 57 + 160 = 240.
#
##############################################################################

let $binlog_start_pos=240;
--disable_query_log
SET @binlog_start_pos=240;
--enable_query_log

