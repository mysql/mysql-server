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
#   - Be careful with single quotes within the value. They must be escaped like
#     "''" or "\'".
#   - Do not keep the value between single quotes.
#
#
# Content of "$message" and protocol output depending on the assignment:
# ----------------------------------------------------------------------
#
#   I is assumed, that the value is not kept between double quotes.
#
#   <x>  first character after "$message=", 
#        where the content is not (space or tab)
#   <y*> first character after beginning of the line,
#        where the content is not (space or tab)
#   <z>  last char before ";"
#    |   beginning or end of line
#           
#   script: let $message=   <x><whatever0>|
#           | <y1><whatever1>| 
#           |................| 
#           |      <yn><whatevern><z>;
#   content: "<x><whatever0><new line><y1><whatever1><new line>
#             ....<new line><yn><whatevern><z>"
#   protocol output: |<x><whatever0>|
#                    |<y1><whatever1>|
#                    |.....|
#                    |<yn><whatevern><z>|
#                    |--- 80 dashes ---|
#
# Attention:
#   <x> and <y*> set to characters like "-$#" which are also used 
#   to start comments, options and the names of mysqltest variables
#   lead to syntax errors or mangled messages.
#
#
# Examples of messages:
# ---------------------
#
#   Variant1 (ease of use): 
#      Several lines with indentation kept between double quotes
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
#      - it is easy to write the script lines to get a fine indentation,
#        if the value starts at the beginning of a new line
#      - the '"' is printed
#      - there are the least or no problems with characters like "-#$"
#
#
#   Variant 2 (grep the messages from the protocol is easy):
#      Several lines with indentation + auxiliary character (".")
#              at the (non tab or space) beginning of every message line
#      script:   |let $message= . Testcase 3.1 :  Ensure that Pi is not an|
#                |              .                 integer number.|
#                |              .                 Third line;
#      protocol: |. Testcase 3.1 :  Ensure that Pi is not an|
#                |.                 integer number.|
#                |.                 Third line|
#                |------ 80 dashes ----|
#      Please mention that
#      - the auxiliary character preserves the indentation.
#      - it is easy to write the script lines to get a fine indentation
#      - the auxiliary character is printed
#      - it is recommended to use "." as auxiliary character
#      - auxiliary characters like "-'$#" cause problems
#      
#
#
#   Bad variant1: Several lines with lost indentation
#      script:   |let $message=     Here is message line 1
#                |    message line 2;
#      protocol: |Here is message line 1|
#                |message line 2|
#                |------ 80 dashes ----|
#      Please mention, that the leading spaces of the message lines disappeared.
#
#   Bad variant2: Several lines leading to a syntax error, because of "-"
#      script:   |let $message= - This is a message
#                |   - with a second and
#                |   - third line;
#      protocol: | - third line;;
#                |ERROR 42000: You have an error ... near '- third line'
#                + several following errors
#
#

--disable_query_log
eval SET @utf8_message = CONVERT('$message' using utf8);
select @utf8_message as ""
union
select repeat(CONVERT('-' using utf8),80);
--enable_query_log
