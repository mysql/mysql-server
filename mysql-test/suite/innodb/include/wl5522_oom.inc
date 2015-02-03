--eval SET SESSION debug="+d,$DBUG_CRASH_POINT"

--replace_regex /'.*t1.cfg'/'t1.cfg'/

--error ER_OUT_OF_RESOURCES
ALTER TABLE test_wl5522.t1 IMPORT TABLESPACE;

--eval SET SESSION debug="-d,$DBUG_CRASH_POINT"
