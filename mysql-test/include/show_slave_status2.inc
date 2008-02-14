# Include file to show the slave status, masking out some information
# that varies depending on where the test is executed.

# masked out log positions

--replace_result $MASTER_MYPORT MASTER_PORT
--replace_column 1 # 7 # 8 # 9 # 16 # 22 # 23 # 33 # 35 # 36 #
query_vertical SHOW SLAVE STATUS;
