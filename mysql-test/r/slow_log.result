SET @save_sqlf=@@global.slow_query_log_file;
SET GLOBAL slow_query_log_file= '.../my_slow.log';
SET timestamp=10;
SELECT unix_timestamp(), sleep(2);
unix_timestamp()	sleep(2)
10	0
[ PASS ] timestamp found
SET @@global.slow_query_log_file=@save_sqlf;
