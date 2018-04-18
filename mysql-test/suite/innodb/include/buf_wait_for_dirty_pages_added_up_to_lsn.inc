--let $wait_condition = SELECT COUNT >= $wait_lsn FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE NAME = 'log_lsn_buf_dirty_pages_added';

--source include/wait_condition.inc
