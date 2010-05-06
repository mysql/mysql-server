# Include file to show the slave status, masking out some information
# that varies depending on where the test is executed.

--replace_result $MASTER_MYPORT MASTER_PORT
--replace_column 1 # 8 # 9 # 16 # 23 # 33 # 35 # 36 #
--replace_regex /[0-9][0-9][0-9][0-9][0-9][0-9] [0-9]+:[0-9]+:[0-9]+/YYMMDD HH:MM:SS/
query_vertical SHOW SLAVE STATUS;
