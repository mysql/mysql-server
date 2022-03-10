SELECT @@innodb_temp_data_file_path;
@@innodb_temp_data_file_path
ibtmp1:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_temporary';
FILE_NAME
./ibtmp1
# restart: --innodb-temp-data-file-path=mytemp:12M:autoextend
SELECT @@innodb_temp_data_file_path;
@@innodb_temp_data_file_path
mytemp:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_temporary';
FILE_NAME
./mytemp
# restart: --innodb-temp-data-file-path=./mytemp:12M:autoextend
SELECT @@innodb_temp_data_file_path;
@@innodb_temp_data_file_path
./mytemp:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_temporary';
FILE_NAME
./mytemp
SET GLOBAL innodb_fast_shutdown = 0;
# restart: --innodb-read-only
SELECT @@innodb_temp_data_file_path;
@@innodb_temp_data_file_path
ibtmp1:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_temporary';
FILE_NAME
./mytemp
# restart: --innodb-temp-data-file-path=MYSQL_TMP_DIR/mytemp:12M:autoextend
SELECT @@innodb_temp_data_file_path;
@@innodb_temp_data_file_path
MYSQL_TMP_DIR/mytemp:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_temporary';
FILE_NAME
MYSQL_TMP_DIR/mytemp
# restart:
