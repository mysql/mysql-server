#### include/show_msg.inc
#
# This file writes the value set in @message into the
# a protocol file as part of executing a test sequence
#
# Usage:
#    Add the following to any *.test file:
#      :
#    let $message= <value>;
#    --source include/show_msg.inc
#      :
#
# Attention:
#   - Please do not write any spaces between $message and the "=", because the
#     assignment will not work.
#   - Be careful with single quotes. They must be escaped like "''" or "\'".
#
# "include/show_msg80.inc" contains a detailed description and examples.

--disable_query_log
eval SET @utf8_message = CONVERT('$message' using utf8);
select @utf8_message as ""
union
select repeat(CONVERT('-' using utf8),char_length(@utf8_message));
--enable_query_log
