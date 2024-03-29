#
# These tests verify that MySQL supports hostname with maximum of 255
# char length in various performance schema tables.
#
# Setup
CREATE USER some_user_name@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890;
GRANT ALL ON *.* TO some_user_name@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890;
SET GLOBAL DEBUG='+d,vio_peer_addr_fake_hostname1';
# Execute test with long hostname in GRANTOR.
--------------
SELECT CURRENT_USER()
--------------

CURRENT_USER()
some_user_name@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890
--------------
SELECT Host FROM performance_schema.hosts WHERE host like 'host_%'
--------------

Host
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890
--------------
SELECT User, Host FROM performance_schema.accounts
  WHERE User = 'some_user_name'
--------------

User	Host
some_user_name	host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890
--------------
SELECT host, count(variable_name)>1
  FROM performance_schema.status_by_host
  WHERE host like 'host_%' GROUP BY host
--------------

host	count(variable_name)>1
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT host, count(variable_name)>1
  FROM performance_schema.status_by_account
  WHERE host like 'host_%' GROUP BY host
--------------

host	count(variable_name)>1
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT name, type, processlist_user, processlist_host, processlist_db
  FROM performance_schema.threads WHERE processlist_host like 'host_%'
--------------

name	type	processlist_user	processlist_host	processlist_db
thread/sql/one_connection	FOREGROUND	some_user_name	host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	test
--------------
SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name')
--------------

sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij123
YES
--------------
DELETE FROM performance_schema.setup_actors
--------------

--------------
SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name')
--------------

sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij123
NO
--------------
INSERT INTO performance_schema.setup_actors VALUES ('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name', '%', 'YES', 'NO')
--------------

--------------
SELECT * FROM performance_schema.setup_actors
--------------

HOST	USER	ROLE	ENABLED	HISTORY
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	some_user_name	%	YES	NO
--------------
SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name')
--------------

sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij123
YES
--------------
SET GLOBAL sort_buffer_size=256000
--------------

--------------
SELECT VARIABLE_NAME, VARIABLE_SOURCE, SET_USER, SET_HOST 
  FROM performance_schema.variables_info
  WHERE VARIABLE_NAME in ('sort_buffer_size') ORDER BY VARIABLE_NAME
--------------

VARIABLE_NAME	VARIABLE_SOURCE	SET_USER	SET_HOST
sort_buffer_size	DYNAMIC	some_user_name	host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890
--------------
SET GLOBAL sort_buffer_size=default
--------------

--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_waits_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_errors_summary_by_host_by_error
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_errors_summary_by_account_by_error
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_stages_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_stages_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_statements_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_statements_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_transactions_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_transactions_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_waits_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.memory_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
--------------
SELECT Host, COUNT(*)>0
  FROM performance_schema.memory_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host
--------------

Host	COUNT(*)>0
host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890	1
Pattern "host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij123456789" found
Pattern "host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij123456789" found
SET GLOBAL DEBUG='-d,vio_peer_addr_fake_hostname1';
# Cleanup
TRUNCATE TABLE performance_schema.setup_actors;
INSERT INTO performance_schema.setup_actors VALUES ('%', '%', '%', 'YES', 'YES');
DROP USER some_user_name@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890;
