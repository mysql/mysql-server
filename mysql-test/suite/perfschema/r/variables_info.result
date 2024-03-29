CALL mtr.add_suppression("Plugin mysqlx reported: 'All I/O interfaces are disabled");
drop table if exists test.marker;
create table test.marker(before_marker timestamp, after_marker timestamp);
# Test explicitly set options using defaults-file
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'EXPLICIT' AND
VARIABLE_NAME LIKE 'performance%'
  order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
performance_schema	EXPLICIT	NULL	NULL	NULL
performance_schema_accounts_size	EXPLICIT	NULL	NULL	NULL
performance_schema_digests_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_stages_history_long_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_stages_history_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_statements_history_long_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_statements_history_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_transactions_history_long_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_transactions_history_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_waits_history_long_size	EXPLICIT	NULL	NULL	NULL
performance_schema_events_waits_history_size	EXPLICIT	NULL	NULL	NULL
performance_schema_hosts_size	EXPLICIT	NULL	NULL	NULL
performance_schema_session_connect_attrs_size	EXPLICIT	NULL	NULL	NULL
performance_schema_setup_actors_size	EXPLICIT	NULL	NULL	NULL
performance_schema_setup_objects_size	EXPLICIT	NULL	NULL	NULL
performance_schema_users_size	EXPLICIT	NULL	NULL	NULL
# Test command line options
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'COMMAND_LINE' AND
(VARIABLE_NAME = 'max_connections' OR
VARIABLE_NAME = 'skip_networking')
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
# Restart server with command line option
select VARIABLE_NAME, VARIABLE_SOURCE, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'COMMAND_LINE' AND
(VARIABLE_NAME = 'max_connections' OR
VARIABLE_NAME = 'skip_networking')
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_USER	SET_HOST
max_connections	COMMAND_LINE	NULL	NULL
skip_networking	COMMAND_LINE	NULL	NULL
# Test persisted variables
SET PERSIST sort_buffer_size=256000;
SET PERSIST max_heap_table_size=999424, replica_net_timeout=124;
SET PERSIST innodb_default_row_format=COMPACT;
SET @@persist.max_execution_time=44000, @@persist.max_user_connections=30;
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'PERSISTED'
  order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
# Restart server with some persisted variables
select VARIABLE_NAME, VARIABLE_SOURCE, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'PERSISTED'
  order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_USER	SET_HOST
innodb_default_row_format	PERSISTED	root	localhost
max_execution_time	PERSISTED	root	localhost
max_heap_table_size	PERSISTED	root	localhost
max_user_connections	PERSISTED	root	localhost
replica_net_timeout	PERSISTED	root	localhost
slave_net_timeout	PERSISTED	root	localhost
sort_buffer_size	PERSISTED	root	localhost
# Change persisted variables to dynamic
SELECT now() into @before_timestamp;
Sleep 1 second
SET GLOBAL sort_buffer_size=256000;
SET GLOBAL max_heap_table_size=999424, replica_net_timeout=124;
SET @@global.max_execution_time=440000, @@global.max_user_connections=30;
SET GLOBAL innodb_default_row_format=DEFAULT;
Sleep 1 second
SELECT now() into @after_timestamp;
truncate table test.marker;
insert into test.marker values (@before_timestamp, @after_timestamp);
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_SOURCE = 'PERSISTED'
  order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME in
('sort_buffer_size',
'max_heap_table_size',
'replica_net_timeout',
'max_execution_time',
'max_user_connections',
'innodb_default_row_format')
order by VARIABLE_NAME;
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
innodb_default_row_format	DYNAMIC	pass	pass	root	localhost
max_execution_time	DYNAMIC	pass	pass	root	localhost
max_heap_table_size	DYNAMIC	pass	pass	root	localhost
max_user_connections	DYNAMIC	pass	pass	root	localhost
replica_net_timeout	DYNAMIC	pass	pass	root	localhost
sort_buffer_size	DYNAMIC	pass	pass	root	localhost
# Restart server
select VARIABLE_NAME, VARIABLE_SOURCE
from performance_schema.variables_info
where VARIABLE_NAME = 'innodb_fast_shutdown' OR
VARIABLE_NAME = 'innodb_file_per_table';
VARIABLE_NAME	VARIABLE_SOURCE
innodb_fast_shutdown	COMPILED
innodb_file_per_table	COMPILED
# Restart server
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'innodb_fast_shutdown' OR
VARIABLE_NAME = 'innodb_file_per_table';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
innodb_fast_shutdown	COMMAND_LINE	NULL	NULL	NULL
innodb_file_per_table	COMMAND_LINE	NULL	NULL	NULL
# Restart server
#
# WL#9720: SET PERSIST capture user + timestamp
#
CREATE USER WL9720@localhost;
GRANT ALL ON *.* to WL9720@localhost;
SELECT now() into @before_timestamp;
Sleep 1 second
SET SESSION max_heap_table_size=999424;
Sleep 1 second
SELECT now() into @after_timestamp;
truncate table test.marker;
insert into test.marker values (@before_timestamp, @after_timestamp);
# user must be WL9720
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'max_heap_table_size';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
max_heap_table_size	DYNAMIC	pass	pass	WL9720	localhost
select before_marker, after_marker into @before_timestamp, @after_timestamp
from test.marker;
# user must be root
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'max_heap_table_size';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
max_heap_table_size	DYNAMIC	pass	pass	WL9720	localhost
# user must be root
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'session_track_system_variables';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
session_track_system_variables	COMPILED	NULL	NULL	NULL
SELECT now() into @before_timestamp;
Sleep 1 second
SET GLOBAL session_track_system_variables=DEFAULT;
Sleep 1 second
SELECT now() into @after_timestamp;
truncate table test.marker;
insert into test.marker values (@before_timestamp, @after_timestamp);
# user must be WL9720
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'session_track_system_variables';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
session_track_system_variables	DYNAMIC	pass	pass	WL9720	localhost
select before_marker, after_marker into @before_timestamp, @after_timestamp
from test.marker;
# user must be WL9720 since variable was changed globally by wl9720 user
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'session_track_system_variables';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
session_track_system_variables	DYNAMIC	pass	pass	WL9720	localhost
# Restart server
# user must be root
select VARIABLE_NAME, VARIABLE_SOURCE, SET_TIME, SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'session_track_system_variables';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME	SET_USER	SET_HOST
session_track_system_variables	COMPILED	NULL	NULL	NULL
DROP USER WL9720@localhost;
# check timestamp column values
select SET_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sql_auto_is_null';
SET_TIME
NULL
Sleep 1 second
SET SESSION sql_auto_is_null= 1;
select SET_TIME is not null as CHECK_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sql_auto_is_null';
CHECK_TIME
1
select SET_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sort_buffer_size';
SET_TIME
NULL
Sleep 1 second
SET GLOBAL sort_buffer_size= 256000;
select SET_TIME is not null as CHECK_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sort_buffer_size';
CHECK_TIME
1
select SET_TIME is not null as CHECK_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'foreign_key_checks';
CHECK_TIME
1
SELECT now() into @before_timestamp;
Sleep 1 second
SET PERSIST foreign_key_checks = 0;
Sleep 1 second
SELECT now() into @after_timestamp;
select VARIABLE_NAME, VARIABLE_SOURCE,
if (SET_TIME >= @before_timestamp, "pass", "fail") as SET_TIME_BEFORE_CHECK,
if (SET_TIME <= @after_timestamp, "pass", "fail") as SET_TIME_AFTER_CHECK,
SET_USER, SET_HOST
from performance_schema.variables_info
where VARIABLE_NAME = 'foreign_key_checks';
VARIABLE_NAME	VARIABLE_SOURCE	SET_TIME_BEFORE_CHECK	SET_TIME_AFTER_CHECK	SET_USER	SET_HOST
foreign_key_checks	DYNAMIC	pass	pass	root	localhost
select SET_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sql_log_bin';
SET_TIME
NULL
Sleep 1 second
# since SET statement fails there should be no change in timestamp
SET @@persist.sql_log_bin=0;
ERROR HY000: Variable 'sql_log_bin' is a SESSION variable and can't be used with SET GLOBAL
select SET_TIME
from performance_schema.variables_info
where VARIABLE_NAME = 'sql_log_bin';
SET_TIME
NULL
SET GLOBAL sort_buffer_size= DEFAULT;
SET PERSIST foreign_key_checks = DEFAULT;
RESET PERSIST;
drop table test.marker;
SELECT 'END OF TEST';
END OF TEST
END OF TEST
