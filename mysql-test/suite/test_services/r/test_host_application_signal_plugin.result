INSTALL PLUGIN test_services_host_application_signal SONAME "TEST_SERVICES_HOST_APPLICATION_SIGNAL_PLUGIN";
# Test for bad service id
SELECT test_shutdown_signal_udf(2);
test_shutdown_signal_udf(2)
0
# shutdown the server from mtr.
# restart the server.
# start the sql client and test the shutdown UDF with 0
test_shutdown_signal_udf(0)
0
# Search for the message in the log
Pattern "Received SHUTDOWN from user .internal signal component service" found
# restart the server
# start the sql client and test the shutdown UDF with 1
test_shutdown_signal_udf(1)
0
# Search for the message in the log
Pattern "Received SHUTDOWN from user .internal signal component service" found
# restart server.
# Stop DB server which was created by MTR default
# Trying the shutdown at init option
# Search for the message in the log
Pattern "Received SHUTDOWN from user .internal signal component service" found
# restart server.
UNINSTALL PLUGIN test_services_host_application_signal;
