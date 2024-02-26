#
# Tests for the Performance Schema native functions ps_thread_id()
# and ps_current_thread_id()

# Get the connection ID of this connection
SELECT connection_id() INTO @cid;

# The thread is not instrumented, therefore it must be NULL
SELECT ps_current_thread_id();
ps_current_thread_id()
NULL

SELECT ps_thread_id(@cid);
ps_thread_id(@cid)
NULL

