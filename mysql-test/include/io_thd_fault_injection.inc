#
#  Takes the flag as an argument:
#  -- let $io_thd_injection_fault_flag=+d,fault_injection_new_file_rotate_event
#  -- source include/io_thd_fault_injection.inc
#

SET @old_debug=@@global.debug;
-- disable_warnings
-- source include/rpl/stop_replica.inc 
-- enable_warnings
-- eval SET GLOBAL debug="+d,$io_thd_injection_fault_flag"

START REPLICA io_thread;
-- source include/rpl/wait_for_receiver_error.inc

-- eval SET GLOBAL debug="-d,$io_thd_injection_fault_flag"
SET GLOBAL debug=@old_debug;

# restart because slave is in bad shape
--let $rpl_server_number= 2
--source include/rpl/restart_server.inc
