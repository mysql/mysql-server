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
