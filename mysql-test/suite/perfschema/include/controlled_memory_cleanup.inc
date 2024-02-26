
--connection default
--disconnect con1
--disconnect con2

let $count_sessions= 1;
--source include/wait_until_count_sessions.inc

# Reset setup_instruments to defaults

UPDATE performance_schema.setup_instruments
  SET FLAGS = '', ENABLED='YES';

UPDATE performance_schema.setup_instruments
  SET FLAGS = 'controlled'
  where (find_in_set('controlled_by_default', PROPERTIES));

# Restore instrumentation for all threads

UPDATE performance_schema.setup_threads SET ENABLED = 'YES';
UPDATE performance_schema.threads SET INSTRUMENTED = 'YES';

DROP PROCEDURE test.verify_thread;
DROP TABLE test.enforced;

