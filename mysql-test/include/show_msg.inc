#### include/show_msg.inc
#
# This file writes the value set in @message into the
# a protocol file as part of executing a test sequence
#
# Usage:
#    Add the following to any *.test file:
#      :
#    set @message="This is a message example";
#    --source include/show_msg.inc
#      :
#

--disable_query_log
SET @utf8_message = CONVERT(@message using utf8);
select @utf8_message as ""
union
select repeat(CONVERT('-' using utf8),char_length(@utf8_message));
--enable_query_log
