# We currently only have named pipe support on windows, so
# in order  to optimize things we skip this test on all
# other platforms
--source include/windows.inc

# thread pool causes different results
-- source include/not_threadpool.inc

# Connect using named pipe for testing
connect(pipe_con,localhost,root,,,,,PIPE);

# Source select test case
-- source include/common-tests.inc

connection default;
disconnect pipe_con;
