All row lock test with InnoDB have to be executed with the options

--innodb_lock_wait_timeout=1
--innodb_locks_unsafe_for_binlog 

for example

perl mysql-test-run.pl --mysqld=--innodb_lock_wait_timeout=2 --mysqld=--innodb_locks_unsafe_for_binlog --suite=row_lock innodb_row_lock_2

