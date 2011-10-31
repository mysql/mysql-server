# See related script table_io_setup_helper.inc

# Cleanup
update performance_schema.setup_consumers set enabled='NO';
truncate performance_schema.events_waits_history_long;
drop table test.marker;
flush status;
update performance_schema.setup_instruments set enabled='YES';
update performance_schema.setup_consumers set enabled='YES';

