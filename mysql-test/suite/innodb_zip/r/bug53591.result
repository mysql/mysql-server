SET GLOBAL innodb_file_per_table=on;
set old_alter_table=0;
CREATE TABLE bug53591(a text charset utf8 not null)
ENGINE=InnoDB KEY_BLOCK_SIZE=1;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
ALTER TABLE bug53591 ADD PRIMARY KEY(a(220));
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is {checked_valid}. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SHOW WARNINGS;
Level	Code	Message
Error	1118	Row size too large. The maximum row size for the used table type, not counting BLOBs, is {checked_valid}. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
DROP TABLE bug53591;
SET GLOBAL innodb_file_per_table=1;
