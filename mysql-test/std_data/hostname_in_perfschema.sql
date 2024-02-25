SELECT CURRENT_USER();

#
# performance_schema.hosts.host
#

SELECT Host FROM performance_schema.hosts WHERE host like 'host_%';

#
# Check performance_schema.accounts
#

SELECT User, Host FROM performance_schema.accounts
  WHERE User = 'some_user_name';

#
# Check performance_schema.status_by_host
#

SELECT host, count(variable_name)>1
  FROM performance_schema.status_by_host
  WHERE host like 'host_%' GROUP BY host;

#
# Check performance_schema.status_by_account
#

SELECT host, count(variable_name)>1
  FROM performance_schema.status_by_account
  WHERE host like 'host_%' GROUP BY host;

#
# Check performance_schema.threads
#

SELECT name, type, processlist_user, processlist_host, processlist_db
  FROM performance_schema.threads WHERE processlist_host like 'host_%';

#
# Check performance_schema.setup_actors and sys.ps_is_account_enabled
#

# First test with the default "all enabled" for any random user
SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name');

# Now remove the wild card entry, and add some specific users to testx
DELETE FROM performance_schema.setup_actors;

SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name');

INSERT INTO performance_schema.setup_actors VALUES ('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name', '%', 'YES', 'NO');

SELECT * FROM performance_schema.setup_actors;

# Now the random account should not be enabled
SELECT sys.ps_is_account_enabled('host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890', 'some_user_name');

#
# Check host name performance_schema.variables
#

SET GLOBAL sort_buffer_size=256000;                                             
SELECT VARIABLE_NAME, VARIABLE_SOURCE, SET_USER, SET_HOST 
  FROM performance_schema.variables_info
  WHERE VARIABLE_NAME in ('sort_buffer_size') ORDER BY VARIABLE_NAME;
SET GLOBAL sort_buffer_size=default;                                             

#
# Check host name in performance_schema.events_* and memory_* tables
#

SELECT Host, COUNT(*)>0
  FROM performance_schema.events_waits_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_errors_summary_by_host_by_error
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_errors_summary_by_account_by_error
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_stages_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_stages_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_statements_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_statements_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_transactions_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_transactions_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.events_waits_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.memory_summary_by_account_by_event_name
  WHERE host like 'host_%' GROUP BY host;
SELECT Host, COUNT(*)>0
  FROM performance_schema.memory_summary_by_host_by_event_name
  WHERE host like 'host_%' GROUP BY host;
