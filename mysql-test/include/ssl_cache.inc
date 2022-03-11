--echo #
--echo # WL#13075: Support TLS session reuse in the C API version independent part
--echo #

# ==== Purpose ====
#
# Version agnostic TLS cache test applied to all supported versions
#
# ==== Usage ====
#
# --let $tls_version= valid value for --tls-version, e.g. TLSv1.2 or TLSv1.3
# --source include/ssl_cache.inc
#
# Parameters:
#
#   $tls_version
#     the tls version to pass to the mysql client utilities while testing
#

if (!$tls_version)
{
  --die !!!ERROR IN TEST: you must set $tls_version
}

let $sql_file=$MYSQLTEST_VARDIR/tmp/ssl_cmds.sql;
let $out_file=$MYSQLTEST_VARDIR/tmp/ssl_cmds.out;
let $session_file=$MYSQLTEST_VARDIR/tmp/ssl_cmds.pem;

--echo # FR6: mysql store and retrieve sessions at connect

write_file $sql_file;
status;
connect;
status;
EOF

exec $MYSQL --tls-version=$tls_version --ssl-mode=required < $sql_file > $out_file 2>&1;

--let $assert_file=$out_file
--let $assert_text=Checking if session was reused post-connect
--let $assert_select=^SSL session reused:[\t\ ]*true
--let $assert_count=1
source include/assert_grep.inc;

remove_file $sql_file;
remove_file $out_file;

--echo # FR8: ssl cache limit
SET GLOBAL ssl_session_cache_mode=off;
ALTER INSTANCE RELOAD TLS;
--echo # try to see if a session ticket is sent
exec $MYSQL --tls-version=$tls_version --ssl-mode=required -e "ssl_session_data_print" 2>&1 > $out_file;
SELECT VARIABLE_NAME,VARIABLE_VALUE FROM performance_schema.global_status
  WHERE VARIABLE_NAME LIKE 'ssl_session%' ORDER BY 1;
--let $assert_file=$out_file
--let $assert_text=FR8: Checking if session data file was populated
--let $assert_select=-*BEGIN SSL SESSION PARAMETERS
--let $assert_count=0;
# openssl 1.0 (that doesn't support TLS 1.3) will still send
# SSL session resumption data even if disabled on the server
# Hence we expect 1 with it.
if (`SELECT @@GLOBAL.tls_version NOT LIKE '%TLSv1.3'`) {
  let $assert_count=1;
}
source include/assert_grep.inc;

--echo # store a ticket to a file
exec $MYSQL --tls-version=$tls_version --ssl-mode=required -e "ssl_session_data_print $session_file" 2>&1 > $out_file;
--echo # reuse: should fail
--error 1
exec $MYSQL --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file -e "SELECT 'dead' as the_server_is" 2>&1;

remove_file $out_file;
remove_file $session_file;
SET GLOBAL ssl_session_cache_mode=default;
ALTER INSTANCE RELOAD TLS;


--echo # FR10 and FR9:  check ssl session timeout
SET GLOBAL ssl_session_cache_timeout = 5; # seconds
ALTER INSTANCE RELOAD TLS;
--echo # store session data
exec $MYSQL --tls-version=$tls_version --ssl-mode=required -e "ssl_session_data_print $session_file" 2>&1;
--echo # try the stored data pre- timeout
exec $MYSQL --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file -e "SELECT 'alive' as the_server_is" 2>&1;
--echo # sleep 10 secs
--sleep 10
--echo # try the stored data post- timeout
--error 1
exec $MYSQL --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file -e "SELECT 'dead' as the_server_is" 2>&1;
SET GLOBAL ssl_session_cache_timeout=default;
ALTER INSTANCE RELOAD TLS;
remove_file $session_file;

--echo # FR11: store ssl session data into a file and check format
exec $MYSQL --tls-version=$tls_version --ssl-mode=required -e "ssl_session_data_print $session_file" 2>&1;
--let $assert_file=$session_file
--let $assert_text=FR11: Checking if session data file was populated
--let $assert_select=-*BEGIN SSL SESSION PARAMETERS
--let $assert_count=1
source include/assert_grep.inc;

--echo # openssl sess_id should be able to parse the file
exec $OPENSSL_EXECUTABLE sess_id -in $session_file -inform PEM -noout -text > $out_file 2>&1;
--let $assert_file=$out_file
--let $assert_text=Checking if session file is PEM format
--let $assert_select=Protocol[\ \t]*\:[\ \t]*TLSv1\.
--let $assert_count=1
source include/assert_grep.inc;
remove_file $out_file;

--echo # FR12: sesison reuse file should work
--echo # try mysqladmin
exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file ping 2>&1;
--echo # try mysql command line
exec $MYSQL --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file -e "status" > $out_file 2>&1;

--let $assert_file=$out_file
--let $assert_text=Checking if file session was reused
--let $assert_select=^SSL session reused:[\t\ ]*true
--let $assert_count=1
source include/assert_grep.inc;
remove_file $out_file;

--echo # FR7 and FR12: make sure resetting TLS invalidates the stored session data
ALTER INSTANCE RELOAD TLS;
--let $copts=-u root --protocol=tcp -S $MASTER_MYSOCK -P $MASTER_MYPORT --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file
--echo # try mysql
--error 1
exec $MYSQL $copts -e "status" 2>&1;
--echo # try mysqladmin
--error 1
exec $MYSQLADMIN $copts version 2>&1;
--echo # try mysqldump
--error 2
exec $MYSQL_DUMP $copts test 2>&1;
--echo # try mysqlimport
--error 1
exec $MYSQL_IMPORT $copts test foo.bar 2>&1;
--echo # try mysqlshow
--error 1
exec $MYSQL_SHOW $copts 2>&1;
--echo # try mysqlslap
--error 1
exec $MYSQL_SLAP $copts 2>&1;
--echo # try mysqltest
--error 1
exec $MYSQL_TEST $copts 2>&1;
--echo # try mysqlpump
exec $MYSQL_PUMP $copts test 2>&1;

--echo # FR12.1: make sure --ssl-session-data-continue-on-failed-reuse works
--let $copts=-u root --protocol=tcp -S $MASTER_MYSOCK -P $MASTER_MYPORT --tls-version=$tls_version --ssl-mode=required --ssl-session-data=$session_file --ssl-session-data-continue-on-failed-reuse
--echo # try mysql
exec $MYSQL $copts -e "status" > $out_file 2>&1;
--echo # try mysqladmin
exec $MYSQLADMIN $copts version > $out_file 2>&1;
--echo # try mysqldump
exec $MYSQL_DUMP $copts test > $out_file 2>&1;
--echo # try mysqlshow
exec $MYSQL_SHOW $copts test > $out_file 2>&1;
--echo # try mysqlslap
exec $MYSQL_SLAP $copts --concurrency=1 --iterations=1 -query="SELECT 1" > $out_file 2>&1;
--echo # try mysqlpump
exec $MYSQL_PUMP $copts test > $out_file 2>&1;


remove_file $session_file;
remove_file $out_file;

SELECT VARIABLE_NAME,VARIABLE_VALUE FROM performance_schema.global_status
  WHERE VARIABLE_NAME LIKE 'ssl_session%' ORDER BY 1;

--echo # FR13: print session data to stdout
--replace_regex /^[^-]*($)//
exec $MYSQL --tls-version=$tls_version --ssl-mode=required -e "ssl_session_data_print";

--echo # end of WL#13075: Support TLS session reuse in the C API version independent part
