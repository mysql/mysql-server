#
# Tests for the Performance Schema native functions ps_thread_id()
# and ps_current_thread_id() with disabled Performance Schema

# Verify the Performance Schema is disabled
SELECT @@global.performance_schema AS 'Expect 0';
Expect 0
0

# Get the connection ID of this connection
SELECT connection_id() INTO @cid;

# The Performance Schema is not available
SELECT ps_current_thread_id();
ERROR HY000: 'ps_current_thread_id': The Performance Schema is not enabled.

SELECT ps_thread_id(@cid);
ERROR HY000: 'ps_thread_id': The Performance Schema is not enabled.

