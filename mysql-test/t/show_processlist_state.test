#
# Output should be the same with or without the performance schema.
#

--echo "Test SHOW PROCESSLIST, column INFO"
--replace_column 1 ID 3 HOST 6 TIME
--replace_regex /Waiting on empty queue/WAITING/ /Waiting for next activation/WAITING/
show processlist;

