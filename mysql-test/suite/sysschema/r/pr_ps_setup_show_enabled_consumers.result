UPDATE performance_schema.setup_consumers
SET ENABLED = 'NO'
 WHERE NAME LIKE '%\_history\_long';
CALL sys.ps_setup_show_enabled_consumers();
enabled_consumers
events_stages_current
events_stages_history
events_statements_cpu
events_statements_current
events_statements_history
events_transactions_current
events_transactions_history
events_waits_current
events_waits_history
global_instrumentation
statements_digest
thread_instrumentation
UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
TRUNCATE TABLE performance_schema.setup_actors;
INSERT INTO performance_schema.setup_actors VALUES ('%', '%', '%', 'YES', 'YES');
UPDATE performance_schema.setup_consumers SET enabled = 'YES';
UPDATE performance_schema.threads SET instrumented = 'YES';
