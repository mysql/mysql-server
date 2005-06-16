#### include/show_msg80.inc
#
# This file writes the value set in @message into the a protocol file as part
# of executing a test sequence with a dash line that is fixed on 80 characters.
#
# This can be used in the case of long messages, multi line messages that 
# exceed 80 or if an 80 char line is desired for short messages.
#
# Usage:
#    Add the following to any *.test file:
#      :
#    let $message= <value>;
#    --source include/show_msg80.inc
#      :
#
# Attention:
#   - Please do not write any spaces between $message and the "=", because the
#     assignment will not work.
#   - Be careful with single quotes. They must be escaped like "''" or "\'".
#
#
# Content of "$message" and protocol output depending on the assignement:
# -----------------------------------------------------------------------
#   <x>  first char after "$message=", 
#        where the content is not (space or tab)
#   <y*> first chat after beginning of the line,
#        where the content is not (space or tab)
#   <z>  last char before ";"
#    |   beginning or end of line
#           
#   script: let $message=   <x><whatever0>|
#           | <y1><whatever1>| 
#           |................| 
#           |  <yn><whatevern><z>;
#   content: "<x><whatever0><new line><y1><whatever1><new line>
#             ....<new line><yn><whatevern><z>"
#   protocol output: |<x><whatever0>|
#                    |<y1><whatever1>|
#                    |.....|
#                    |<yn><whatevern><z>|
#                    |--- 80 dashes ---|
#
#
# Examples of messages:
# ---------------------
#
#   Several lines with indentation variant 1:
#      script:   |let $message= . Testcase 3.1 :  Ensure that Pi is not an|
#                |              .                 integer number|
#                |              .                 Third line;
#      protocol: |. Testcase 3.1 :  Ensure that Pi is not an|
#                |.                 integer number|
#                |.                 Third line|
#                |------ 80 dashes ----|
#      Please mention that
#      - the auxiliary "." preserves the indentation.
#      - it is easy to write the script lines to get a fine indentation
#      - the "." is printed
#
#   Several lines with indentation variant 2:
#      script:   |let $message= 
#                |" Testcase 3.1 :  Ensure that Pi is not an|
#                |                  integer number.|
#                |                  Third line";
#      protocol: |" Testcase 3.1 :  Ensure that Pi is not an|
#                |                  integer number.|
#                |                  Third line"|
#                |------ 80 dashes ----|
#
#      Please mention that
#      - the '"' preserves the indentation.
#      - it is not so easy to write the script lines to get a fine indentation
#      - the '"' is printed
#
#   Several lines with lost indentation (negative example)
#      script:   |let $message=     Here is message line 1
#                |    message line 2;
#      protocol: |Here is message line 1|
#                |message line 2|
#                |------ 80 dashes ----|
#      Please mention, that the leading spaces of the message lines disappeared.
#

--disable_query_log
eval SET @utf8_message = CONVERT('$message' using utf8);
select @utf8_message as ""
union
select repeat(CONVERT('-' using utf8),80);
--enable_query_log
