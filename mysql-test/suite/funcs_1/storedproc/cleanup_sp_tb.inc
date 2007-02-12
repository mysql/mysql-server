let $message= --source suite/funcs_1/storedproc/cleanup_sp_tb.inc;
--source include/show_msg80.inc

# called both to cleanup possibly existing data before and after the SP tests

--disable_warnings
DROP DATABASE IF EXISTS db_storedproc;
DROP DATABASE IF EXISTS db_storedproc_1;
--enable_warnings
