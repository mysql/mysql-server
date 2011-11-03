--require r/have_mutex_deadlock_detector.require
disable_query_log;
select 1 from information_schema.global_variables where variable_name = "mutex_deadlock_detector";
enable_query_log;
