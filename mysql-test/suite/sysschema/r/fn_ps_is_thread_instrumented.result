SELECT sys.ps_is_thread_instrumented(NULL);
sys.ps_is_thread_instrumented(NULL)
NULL
SELECT sys.ps_is_thread_instrumented(CONNECTION_ID());
sys.ps_is_thread_instrumented(CONNECTION_ID())
YES
UPDATE performance_schema.threads SET instrumented = 'NO' WHERE processlist_id = CONNECTION_ID();
SELECT sys.ps_is_thread_instrumented(CONNECTION_ID());
sys.ps_is_thread_instrumented(CONNECTION_ID())
NO
SELECT sys.ps_is_thread_instrumented(234623462376);
sys.ps_is_thread_instrumented(234623462376)
UNKNOWN
UPDATE performance_schema.threads SET instrumented = 'YES';
