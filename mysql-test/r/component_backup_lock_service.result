INSTALL COMPONENT "file://component_test_backup_lock_service";
# The component component_test_backup_lock_service acquires BACKUP LOCK
# during its initialization. Check that BACKUP LOCK has been really acquired.
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_type = 'BACKUP LOCK';
object_type	object_schema	object_name	lock_type	lock_duration	lock_status
BACKUP LOCK	NULL	NULL	SHARED	EXPLICIT	GRANTED
# Stop component.
UNINSTALL COMPONENT "file://component_test_backup_lock_service";
# The component component_test_backup_lock_service releases BACKUP LOCK
# during its unloading. Check that BACKUP LOCK has been really released.
SELECT object_schema, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_type = 'BACKUP LOCK';
object_schema	object_name	lock_type	lock_duration	lock_status
#
# Bug#33087212: COMPONENT COMPONENT_TEST_BACKUP_LOCK_SERVICE CAUSES
# CRASH AT SHUTDOWN.
#
# Verify that it is possible to shut down the server while the
# component is installed.
INSTALL COMPONENT 'file://component_test_backup_lock_service';
# shutdown the server from mtr.
# restart the server.
INSTALL COMPONENT 'file://component_test_backup_lock_service';
ERROR HY000: Cannot load component from specified URN: 'file://component_test_backup_lock_service'.
# Check for the presence of BACKUP LOCK. There should not be one
# since the lock is not re-acquired when the component is reloaded
# as part of boot.
SELECT object_schema, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_type = 'BACKUP LOCK';
object_schema	object_name	lock_type	lock_duration	lock_status
# But the component is registered and can be unloaded.
UNINSTALL COMPONENT 'file://component_test_backup_lock_service';
