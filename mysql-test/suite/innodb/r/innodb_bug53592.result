set old_alter_table=0;
create table bug53592(a int) engine=innodb row_format=compact;
alter table bug53592 add column b text charset utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
alter table bug53592 add column c blob not null;
create index bug53592_b on bug53592(b(81));
create unique index bug53592_c on bug53592(c(1));
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
replace into bug53592 values (),();
Warnings:
Warning	1364	Field 'c' doesn't have a default value
SET sql_mode = default;
check table bug53592;
Table	Op	Msg_type	Msg_text
test.bug53592	check	status	OK
drop table bug53592;
set old_alter_table=1;
create table bug53592(a int) engine=innodb row_format=compact;
alter table bug53592 add column b text charset utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
alter table bug53592 add column c blob not null;
create index bug53592_b on bug53592(b(81));
create unique index bug53592_c on bug53592(c(1));
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
replace into bug53592 values (),();
Warnings:
Warning	1364	Field 'c' doesn't have a default value
SET sql_mode = default;
check table bug53592;
Table	Op	Msg_type	Msg_text
test.bug53592	check	status	OK
drop table bug53592;
CREATE TABLE bug53592_1(
col1 int, col2 int,
PRIMARY KEY  (col1, col2)
) ENGINE=InnoDB;
CREATE TABLE bug53592_2 (
col int PRIMARY KEY,
FOREIGN KEY (col) REFERENCES bug53592_1 (col1)
ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB;
INSERT INTO bug53592_1 VALUES (1, 2);
INSERT INTO bug53592_1 VALUES (3, 4);
INSERT INTO bug53592_2 VALUES (1);
INSERT INTO bug53592_2 VALUES (3);
UPDATE bug53592_1 SET col1 = 3 WHERE col2 = 2;
ERROR 23000: Foreign key constraint for table 'bug53592_1', record '3-2' would lead to a duplicate entry in table 'bug53592_2', key 'PRIMARY'
drop table bug53592_2;
drop table bug53592_1;
