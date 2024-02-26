--let $wait_condition = SELECT COUNT >= $wait_lsn FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE NAME = 'log_lsn_current';

--source include/wait_condition.inc
