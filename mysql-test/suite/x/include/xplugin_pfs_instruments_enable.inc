# Enabled XPlugin Instruments
-->sql
TRUNCATE TABLE performance_schema.events_waits_history_long;
update performance_schema.setup_instruments 
set ENABLED='YES' where NAME like '%mysqlx%';

update performance_schema.setup_consumers 
set ENABLED='YES' where NAME like 'events_waits%';
-->endsql
