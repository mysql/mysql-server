include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/stop_slave.inc
#
# Explicitly simulate a NULL master_uuid return value i.e., fake as a pre-5.6 master.
#
SET @save_debug= @@GLOBAL.debug;
SET GLOBAL debug= '+d,dbug.return_null_SOURCE_UUID';
#
# We should still have the master_uuid value.
#
include/assert.inc [source_uuid field should be non-empty.]
#
# START SLAVE will return a NULL for master_uuid now as per our debug behaviour.
#
include/start_slave.inc
#
# We dont have a master_uuid now, so should see an empty output as the master_uuid value.
#
#
# de-activate the debug behaviour.
#
SET GLOBAL debug= '-d,dbug.return_null_SOURCE_UUID';
SET GLOBAL debug= @save_debug;
#
# After de-activating, the master_uuid value should be there.
#
include/stop_slave.inc
include/start_slave.inc
#
# Verify that we have the master_uuid now.
#
include/rpl_end.inc
