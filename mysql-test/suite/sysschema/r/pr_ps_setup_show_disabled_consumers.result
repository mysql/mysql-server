UPDATE performance_schema.setup_consumers
SET ENABLED = 'NO'
 WHERE NAME LIKE '%\_history\_long';
CALL sys.ps_setup_show_disabled_consumers();
disabled_consumers
events_stages_history_long
events_statements_history_long
events_transactions_history_long
events_waits_history_long
UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
TRUNCATE TABLE performance_schema.setup_actors;
INSERT INTO performance_schema.setup_actors VALUES ('%', '%', '%', 'YES', 'YES');
UPDATE performance_schema.setup_consumers SET enabled = 'YES';
UPDATE performance_schema.threads SET instrumented = 'YES';
