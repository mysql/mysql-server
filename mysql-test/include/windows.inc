--require r/windows.require
disable_query_log;
select convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows") as "TRUE";
enable_query_log;
