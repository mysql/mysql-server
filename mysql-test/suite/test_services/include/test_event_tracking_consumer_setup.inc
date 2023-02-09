--echo # Save original value of default_password_lifetime
SELECT @@global.default_password_lifetime INTO @original_default_password_lifetime;

--echo # Set value of default_password_lifetime for tests
SET GLOBAL default_password_lifetime = 100;

--echo # Create objects
CREATE DATABASE event_tracking_db;
CREATE TABLE event_tracking_db.event_tracking_table(c1 INT);
INSERT INTO event_tracking_db.event_tracking_table VALUES (1), (2), (3);
CREATE PROCEDURE event_tracking_db.event_tracking_procedure()
  SELECT 1;

--echo # Create user
CREATE USER event_tracking_user IDENTIFIED BY 'abcd';
CREATE USER other_event_tracking_user IDENTIFIED BY 'efgh';

--echo # Install test component
INSTALL COMPONENT 'file://component_test_event_tracking_consumer';

--echo # Disable everything
SELECT (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) INTO @event_tracking_authentication_all;
SELECT (1 << 0) | (1 << 1) INTO @event_tracking_command_all;
SELECT (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) INTO @event_tracking_connection_all;
SELECT (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) INTO @event_tracking_general_all;
SELECT (1 << 0) | (1 << 1) INTO @event_tracking_global_variable_all;
SELECT (1 << 0) | (1 << 1) INTO @event_tracking_message_all;
SELECT (1 << 0) | (1 << 1) INTO @event_tracking_parse_all;
SELECT (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) INTO @event_tracking_query_all;
SELECT (1 << 0) INTO @event_tracking_shutdown_all;
SELECT (1 << 0) INTO @event_tracking_stored_program_all;
SELECT (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) INTO @event_tracking_table_access_all;

SELECT configure_event_tracking_filter("event_tracking_parse", @event_tracking_parse_all);
SELECT configure_event_tracking_filter("event_tracking_authentication", @event_tracking_authentication_all);
SELECT configure_event_tracking_filter("event_tracking_connection", @event_tracking_connection_all);
SELECT configure_event_tracking_filter("event_tracking_global_variable", @event_tracking_global_variable_all);
SELECT configure_event_tracking_filter("event_tracking_lifecycle", @event_tracking_shutdown_all);
SELECT configure_event_tracking_filter("event_tracking_table_access", @event_tracking_table_access_all);
SELECT configure_event_tracking_filter("event_tracking_message", @event_tracking_message_all);
SELECT configure_event_tracking_filter("event_tracking_stored_program", @event_tracking_stored_program_all);
SELECT configure_event_tracking_filter("event_tracking_query", @event_tracking_query_all);
SELECT configure_event_tracking_filter("event_tracking_general", @event_tracking_general_all);

SELECT reset_event_tracking_counter("all");
