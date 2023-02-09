--echo # Uninstall test component
UNINSTALL COMPONENT 'file://component_test_event_tracking_consumer';

--echo # Drop user
DROP USER event_tracking_user;
DROP USER other_event_tracking_user;

--echo # Drop objects
DROP PROCEDURE event_tracking_db.event_tracking_procedure;
DROP TABLE event_tracking_db.event_tracking_table;
DROP DATABASE event_tracking_db;

--echo # Restore value of default_password_lifetime
SET GLOBAL default_password_lifetime=@original_default_password_lifetime;
