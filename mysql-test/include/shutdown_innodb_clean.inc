# Perform clean shutdown of MySQL

# For InnoDB, slow shutdown has to be requested to flush all dirty pages,
# and make the redo log empty.

SET GLOBAL innodb_fast_shutdown = 0;

--source include/shutdown_mysqld.inc
