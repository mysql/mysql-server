# Include file to show the slave status, masking out some information
# that varies depending on where the test is executed.

--replace_result $MASTER_MYPORT MASTER_PORT
--replace_column 1 # 8 # 9 # 16 # 23 # 33 # 34 # 35 #
query_vertical SHOW SLAVE STATUS;
