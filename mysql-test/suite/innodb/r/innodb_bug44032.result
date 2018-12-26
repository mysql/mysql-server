CREATE TABLE bug44032(c CHAR(3) CHARACTER SET UTF8) ROW_FORMAT=REDUNDANT
ENGINE=InnoDB;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
INSERT INTO bug44032 VALUES('abc'),(0xEFBCA4EFBCA4EFBCA4);
UPDATE bug44032 SET c='DDD' WHERE c=0xEFBCA4EFBCA4EFBCA4;
UPDATE bug44032 SET c=NULL WHERE c='DDD';
UPDATE bug44032 SET c='DDD' WHERE c IS NULL;
DROP TABLE bug44032;
