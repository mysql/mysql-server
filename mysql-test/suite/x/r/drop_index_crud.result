call mtr.add_suppression("Error parsing message of type 12: args");
call mtr.add_suppression("Error handling message: args");
RUN CREATE DATABASE xplugintest

1 rows affected
RUN USE xplugintest

0 rows affected

command ok
Create and drop indexes of different types on single fields
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_it_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_it_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` tinyint GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_it_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_it_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_it_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` tinyint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_it_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_it2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_it2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` tinyint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_it2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_is_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_is_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` smallint GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_is_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_is_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_is_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` smallint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_is_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_is4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_is4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` smallint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_is4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"12345","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_is4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "12345"}	1	{"type": "object"}	12345
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_im_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_im_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` mediumint GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_im_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_im_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_im_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` mediumint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_im_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_im7_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_im7_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` mediumint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_im7_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"12345678","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_im7_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "12345678"}	1	{"type": "object"}	12345678
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_i_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_i_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` int GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_i_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_i_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_i_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` int unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_i_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_i6_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_i6_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` int unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_i6_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"1234567","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_i6_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "1234567"}	1	{"type": "object"}	1234567
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ib_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ib_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` bigint GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ib_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ib_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ib_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` bigint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ib_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ib13_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ib13_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` bigint GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ib13_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"123456789012345","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_ib13_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "123456789012345"}	1	{"type": "object"}	123456789012345
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ib22_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ib22_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` bigint unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ib22_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_fr_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_fr_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` double GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_fr_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_fr_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_fr_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` double unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_fr_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_f_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_f_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` float GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_f_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_f_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_f_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` float unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_f_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_fd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_fd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` double GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_fd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_fd_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_fd_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` double unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_fd_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_fd7_4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_fd7_4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` double(7,4) unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_fd7_4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"123.1234","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_fd7_4_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "123.1234"}	1	{"type": "object"}	123.1234
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(10,0) GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xd10_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xd10_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(10,0) unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xd10_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"123456.123","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_xd10_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "123456.123"}	1	{"type": "object"}	123456
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xd8_2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xd8_2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(8,2) unsigned GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xd8_2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"123456.123","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_xd8_2_ur_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "123456.123"}	1	{"type": "object"}	123456.12
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xn_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xn_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(10,0) GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xn_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xn10_2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xn10_2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(10,2) GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xn10_2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"12345.123456","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_xn10_2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "12345.123456"}	1	{"type": "object"}	12345.12
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_xn5_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_xn5_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` decimal(5,0) GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.col')) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_xn5_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"12345.123456","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_xn5_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "12345.123456"}	1	{"type": "object"}	12345
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_d_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_d_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` date GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_d_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_dt_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_dt_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` time GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_dt_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_dt2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_dt2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` time(2) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_dt2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"12:13:44","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_dt2_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "12:13:44"}	1	{"type": "object"}	12:13:44
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ds_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ds_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` timestamp GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ds_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_ds6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_ds6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` timestamp(6) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_ds6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"2038-01-19 03:14:07.123456","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_ds6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "2038-01-19 03:14:07.123456"}	1	{"type": "object"}	2038-01-19 03:14:07.123456
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_dd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_dd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` datetime GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_dd_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_dd6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_dd6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` datetime(6) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_dd6_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_dy_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_dy_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` year GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_dy_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid or unsupported type specification \'VARCHAR(15)\'"
  sql_state: "HY000"
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"QUITE LOOOONG STRING","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema
{"_id": 1, "col": "QUITE LOOOONG STRING"}	1	{"type": "object"}
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Error {
  severity: ERROR
  code: 1091
  msg: "Can\'t DROP \'ix\'; check that column/key exists"
  sql_state: "42000"
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_c25_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_c25_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` char(25) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_c25_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"LONG LONG STRING, LONGER THAN INDEX SIZE","_id":1}')
While executing INSERT INTO xplugintest.table1(doc) VALUES('{"col":"LONG LONG STRING, LONGER THAN INDEX SIZE","_id":1}'):
Got expected error: Data too long for column '$ix_c25_r_193639E0029FB71DA5916BDF64B75D481FD4CF70' at row 1 (code 1406)
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_c25_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
0 rows affected
RUN DELETE FROM xplugintest.table1

0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_t_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	null	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_t_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` bit(1) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_t_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_bt50_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	50	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_bt50_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` tinyblob GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_bt50_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`(50)),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"BLOB BLOB BLOB BLOB BLOB VALUE LONG VALUE 0123456789 BLOB01234567890123456789","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_bt50_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "BLOB BLOB BLOB BLOB BLOB VALUE LONG VALUE 0123456789 BLOB01234567890123456789"}	1	{"type": "object"}	BLOB BLOB BLOB BLOB BLOB VALUE LONG VALUE 0123456789 BLOB01234567890123456789
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	ix	1	$ix_t30_r_193639E0029FB71DA5916BDF64B75D481FD4CF70	A	0	30	null		BTREE			YES	null
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_t30_r_193639E0029FB71DA5916BDF64B75D481FD4CF70` text GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.col'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `ix` (`$ix_t30_r_193639E0029FB71DA5916BDF64B75D481FD4CF70`(30)),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table1(doc) VALUES('{"col":"TEXT LONGER THAN INDEX SIZE WHICH EQUALS 30 1234567890","_id":1}')

1 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id	_json_schema	$ix_t30_r_193639E0029FB71DA5916BDF64B75D481FD4CF70
{"_id": 1, "col": "TEXT LONGER THAN INDEX SIZE WHICH EQUALS 30 1234567890"}	1	{"type": "object"}	TEXT LONGER THAN INDEX SIZE WHICH EQUALS 30 1234567890
0 rows affected
RUN DELETE FROM xplugintest.table1

1 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Create index on two fields
Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	two_field_index	1	$ix_t10_r_0AD3DA5237EEE3DFC9BAFF8E7071211F65E9713C	A	0	10	null		BTREE			YES	null
table1	0	two_field_index	2	$ix_i_r_B8701A5C0B839D9E3336ED0BD89272C2666E1C74	A	0	null	null		BTREE			YES	null
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Create two indexes using the same field
Mysqlx.Sql.StmtExecuteOk {
}

Mysqlx.Sql.StmtExecuteOk {
}

RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	index1	1	$ix_i_r_391E85E134E06EE3BB933293E26546D764D922BD	A	0	null	null		BTREE			YES	null
table1	0	index1	2	$ix_i_r_EF67772F9C09859F0E6CC3208EDA759461EA79E2	A	0	null	null		BTREE			YES	null
table1	0	index2	1	$ix_i_r_EF67772F9C09859F0E6CC3208EDA759461EA79E2	A	0	null	null		BTREE			YES	null
0 rows affected
Drop the first of the indexes, it should keep the shared column
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	0	index2	1	$ix_i_r_EF67772F9C09859F0E6CC3208EDA759461EA79E2	A	0	null	null		BTREE			YES	null
0 rows affected
Drop the second of the indexes, the column should be gone
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
Error scenarios when dropping the index
Mysqlx.Error {
  severity: ERROR
  code: 5113
  msg: "Invalid collection name: nonexisting.table1"
  sql_state: "HY000"
}

Mysqlx.Error {
  severity: ERROR
  code: 5113
  msg: "Invalid collection name: xplugintest.nonexisting"
  sql_state: "HY000"
}

Mysqlx.Error {
  severity: ERROR
  code: 1091
  msg: "Can\'t DROP \'nonexisting\'; check that column/key exists"
  sql_state: "42000"
}

Mysqlx.Error {
  severity: ERROR
  code: 5017
  msg: "Invalid or unsupported type specification \'TINYBLOB(10)\'"
  sql_state: "HY000"
}

RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
0 rows affected
RUN ALTER TABLE xplugintest.table1 ADD SUBJECT text GENERATED ALWAYS AS (json_extract( doc, '$.subject' )) STORED

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ALTER TABLE xplugintest.table1 ADD book text 

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ALTER TABLE xplugintest.table1 ADD INDEX `fulltext_idx_1` (subject(100))

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ALTER TABLE xplugintest.table1 ADD INDEX `fulltext_idx_2` (subject(100))

0 rows affected
Records: 0  Duplicates: 0  Warnings: 1
RUN ALTER TABLE xplugintest.table1 ADD INDEX `fulltext_idx_3` (book(100))

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `SUBJECT` text GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.subject')) STORED,
  `book` text,
  PRIMARY KEY (`_id`),
  KEY `fulltext_idx_1` (`SUBJECT`(100)),
  KEY `fulltext_idx_2` (`SUBJECT`(100)),
  KEY `fulltext_idx_3` (`book`(100)),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table1	0	PRIMARY	1	_id	A	0	null	null		BTREE			YES	null
table1	1	fulltext_idx_1	1	SUBJECT	A	0	100	null	YES	BTREE			YES	null
table1	1	fulltext_idx_2	1	SUBJECT	A	0	100	null	YES	BTREE			YES	null
table1	1	fulltext_idx_3	1	book	A	0	100	null	YES	BTREE			YES	null
0 rows affected
drop primary index
Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `SUBJECT` text GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.subject')) STORED,
  `book` text,
  KEY `fulltext_idx_1` (`SUBJECT`(100)),
  KEY `fulltext_idx_2` (`SUBJECT`(100)),
  KEY `fulltext_idx_3` (`book`(100)),
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Sql.StmtExecuteOk {
}

Mysqlx.Sql.StmtExecuteOk {
}

Mysqlx.Sql.StmtExecuteOk {
}

RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `SUBJECT` text GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.subject')) STORED,
  `book` text,
  CONSTRAINT `$val_strict_F722D77E7114627C511CE41D56D327CB09C4F9E5` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN ALTER TABLE xplugintest.table1 DROP COLUMN subject

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ANALYZE TABLE xplugintest.table1
Table	Op	Msg_type	Msg_text
xplugintest.table1	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table1
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
0 rows affected
RUN CREATE TABLE xplugintest.table2 (
ID          TINYINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
Name        VARCHAR(255) NOT NULL,
LastName        VARCHAR(255) NOT NULL,
UNIQUE `idx1` (Name),
UNIQUE `idx2` (LastName)
) 

0 rows affected
 drop index from data_model=TABLE
Mysqlx.Sql.StmtExecuteOk {
}

Error can not drop primary index when column defined as auto_increment
Got expected error: Incorrect table definition; there can be only one auto column and it must be defined as a key (code 1075)
RUN SHOW CREATE TABLE xplugintest.table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `ID` tinyint NOT NULL AUTO_INCREMENT,
  `Name` varchar(255) NOT NULL,
  `LastName` varchar(255) NOT NULL,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `idx2` (`LastName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
drop index using namespace=sql
send Mysqlx.Sql.StmtExecute {
  stmt: "ALTER TABLE xplugintest.table2 DROP INDEX `idx2`"
  namespace: "sql"
}


0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN ANALYZE TABLE xplugintest.table2
Table	Op	Msg_type	Msg_text
xplugintest.table2	analyze	status	OK
0 rows affected
RUN SHOW INDEX FROM xplugintest.table2
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
table2	0	PRIMARY	1	ID	A	0	null	null		BTREE			YES	null
0 rows affected
RUN DROP DATABASE IF EXISTS xplugintest

2 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
