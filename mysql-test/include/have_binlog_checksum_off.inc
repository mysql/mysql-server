--disable_warnings
if (`select variable_value not like 'NONE' from information_schema.GLOBAL_VARIABLES
       where variable_name='binlog_checksum'`){
   skip Can not run the test when server activated checksumming;
}
--enable_warnings
