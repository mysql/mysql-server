let $checkpoint_lsn = `SELECT COUNT FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE NAME = 'log_lsn_last_checkpoint';`;
