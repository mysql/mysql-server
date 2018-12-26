let $dirty_pages_added_up_to_lsn = `SELECT COUNT FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE NAME = 'log_lsn_buf_dirty_pages_added'`;
