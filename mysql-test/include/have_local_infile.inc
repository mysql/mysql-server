--require r/have_local_infile.require
disable_query_log;
show variables like 'local_infile';
enable_query_log;
