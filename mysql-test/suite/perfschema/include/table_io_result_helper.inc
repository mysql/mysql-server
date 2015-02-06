# See related script table_io_setup_helper.inc

# Stop table io recording
update performance_schema.setup_consumers set enabled='NO';
eval select event_name,
  left(source, locate(":", source)) as short_source,
  object_type, object_schema,
  if (locate("#sql-", object_name), "#sql-XXXX", object_name)
    as pretty_name,
  operation, number_of_bytes
  from performance_schema.events_waits_history_long
  where event_name like 'wait/io/table/%'
  and object_schema in ($schema_to_dump)
  order by thread_id, event_id;

# In case of failures, this will tell if table io are lost.
show global status like 'performance_schema_%';

# Cleanup
truncate performance_schema.events_waits_history_long;
flush status;

