##------------------------------------------------------------------------------
## Disk Forecaster - Execute unit test
##
## Prerequisites: disk_forecaster_run_mode = STANDBY
##                disk_forecaster_test_filename = <filename>
##
## After test completes, run mode will be automatically set to STANDBY.
##------------------------------------------------------------------------------

--echo
--echo # Clear the forecast tables
use performance_schema;
truncate disk_metric;
truncate disk_forecast;
truncate disk_forecast_summary;

--echo
--echo # Set the test filename
--disable_query_log
eval set @@global.health_monitor.disk_forecast_test_filename=`$FILE_NAME`;
--enable_query_log

--echo
--echo # Start the test
set @@global.health_monitor.disk_forecast_run_mode=TEST;

--echo
--echo # Wait for the test to complete, disk_forecast_run_mode returned to STANDBY
let $wait_condition = select @@global.health_monitor.disk_forecast_run_mode='STANDBY';
let $wait_timeout = 30;
--source include/wait_condition.inc

--echo
--echo # Check the results
let $dfx_debug = 0;

if ($dfx_debug) {
  --echo
  select * from disk_forecast;
  select * from disk_forecast_summary;
}
# TODO: Verify predicted usage is within tolerances
select status from disk_forecast_summary;

select count(*) from disk_metric;

--echo
--echo # Test complete
