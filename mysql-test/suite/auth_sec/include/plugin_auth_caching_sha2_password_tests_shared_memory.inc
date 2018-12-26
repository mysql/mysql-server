# We currently only have shm support on windows, so in order
# to optimize things we skip this test on all other platforms
--source include/windows.inc
# thread pool causes different results
--source include/not_threadpool.inc

let $shm_name= query_get_value("SHOW GLOBAL VARIABLES LIKE 'shared_memory_base_name'", Value, 1);

CREATE USER fordprefect@localhost IDENTIFIED WITH caching_sha2_password BY 'abcd';

--echo # Try to establish a TCP connection with TLS/RSA keys - Should fail
--error 1
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -ufordprefect -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

--echo # Connect using shared memory - Should succeed

--connect(shm_con,localhost,fordprefect,abcd,,,$shm_name,SHM)

--connection default
--disconnect shm_con

--echo # Try to establish a TCP connection with TLS/RSA keys - Should succeed
--exec $MYSQL --host=127.0.0.1 -P $MASTER_MYPORT --ssl-mode=DISABLED -ufordprefect -pabcd -e "SELECT USER(), CURRENT_USER()" 2>&1

DROP USER fordprefect@localhost;
