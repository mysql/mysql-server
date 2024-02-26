# Tests for PERFORMANCE_SCHEMA

let $MYSQLD_DATADIR= `select @@datadir`;
let $MYSQLD_TMPDIR= `select @@tmpdir`;

--disable_query_log
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_consumers set enabled='YES';
--enable_query_log

connect (con1, localhost, root, , );

let $con1_THREAD_ID=`select thread_id from performance_schema.threads
  where PROCESSLIST_ID = connection_id()`;

connect (con2, localhost, root, , );

let $con2_THREAD_ID=`select thread_id from performance_schema.threads
  where PROCESSLIST_ID = connection_id()`;

connect (con3, localhost, root, , );

let $con3_THREAD_ID=`select thread_id from performance_schema.threads
  where PROCESSLIST_ID = connection_id()`;

connection default;

--disable_query_log
prepare stmt_dump_events from
  "select event_name,
     left(source, locate(\":\", source)) as short_source,
     operation, number_of_bytes
  from performance_schema.events_waits_history_long
  where thread_id=? order by event_id;";

prepare stmt_dump_thread from
  "select name from performance_schema.threads where thread_id=? ;";
--enable_query_log

