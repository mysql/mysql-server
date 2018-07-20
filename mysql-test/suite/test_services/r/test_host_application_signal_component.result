INSTALL COMPONENT "file://component_test_host_application_signal";
# Test for bad service id
SELECT test_shutdown_signal(2);
test_shutdown_signal(2)
0
# shutdown the server from mtr.
# restart the server.
# start the sql client and test the shutdown UDF with 0
test_shutdown_signal(0)
0
# Search for the message in the log
Pattern "Received SHUTDOWN from user .internal signal component service" found
# restart the server
# start the sql client and test the shutdown UDF with 1
test_shutdown_signal(1)
0
# Search for the message in the log
Pattern "Received SHUTDOWN from user .internal signal component service" found
# restart server.
UNINSTALL COMPONENT "file://component_test_host_application_signal";
