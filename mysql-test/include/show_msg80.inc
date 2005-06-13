#### include/show_msg80.inc
#
# This file writes the value set in @message into the
# a protocol file as part of executing a test sequence
# with a dash line that is fixed on 80 characters.
# This can be used in the case of long messages, 
# multi line messages that exceed 80 or if an 80 char
# line is desired for short messages.
#
# Usage:
#    Add the following to any *.test file:
#      :
#    set @message="This is a message example";
#    --source include/show_msg80.inc
#      :
#

--disable_query_log
SET @utf8_message = CONVERT(@message using utf8);
select @utf8_message as ""
union
select repeat(CONVERT('-' using utf8),80);
--enable_query_log
