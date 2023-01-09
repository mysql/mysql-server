
--echo #
--echo # Bug #29009331: SSL CLIENT CONNECTION ALLOWED ON
--echo #  SERVER WHICH FAILED TO SETUP SSL CONFIGURATION
--echo #

let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_DATADIR= `SELECT @@datadir`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;

SET PERSIST_ONLY ssl_ca = 'mohit';
CALL mtr.add_suppression('Failed to set up SSL because of the following SSL library error');
CALL mtr.add_suppression('Failed to initialize TLS for channel: mysql_main');
call mtr.add_suppression("Failed to set up TLS. Check logs for details");
call mtr.add_suppression("Internal TLS error.*");
call mtr.add_suppression("Server certificate .* verification has failed. Check logs for more details");

--source include/restart_mysqld.inc

--echo # Must be mohit
SELECT @@global.ssl_ca;
--echo # Must be mohit
SHOW STATUS LIKE '%tls_ca';
--echo # Must be empty
SHOW STATUS LIKE 'ssl_cipher';
RESET PERSIST;

--remove_file $MYSQL_DATADIR/mysqld-auto.cnf
--source include/restart_mysqld.inc
