-- connection mysqld_a
-- eval SET SESSION binlog_row_image= $row_image_param
-- eval SET GLOBAL binlog_row_image= $row_image_param
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';

-- connection mysqld_b
-- eval SET SESSION binlog_row_image= $row_image_param
-- eval SET GLOBAL binlog_row_image= $row_image_param
-- source include/stop_slave.inc
-- source include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';

-- connection mysqld_c
-- eval SET SESSION binlog_row_image= $row_image_param
-- eval SET GLOBAL binlog_row_image= $row_image_param
-- source include/stop_slave.inc
-- source include/start_slave.inc
FLUSH TABLES;
SHOW VARIABLES LIKE 'binlog_row_image';

-- connection mysqld_a

