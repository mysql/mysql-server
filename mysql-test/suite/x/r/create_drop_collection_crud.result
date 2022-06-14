call mtr.add_suppression("Error parsing message of type 12: args");
call mtr.add_suppression("Error handling message: args");
RUN DROP DATABASE IF EXISTS xplugintest

0 rows affected
RUN DROP DATABASE IF EXISTS xplugintest_1

0 rows affected
RUN CREATE DATABASE xplugintest

1 rows affected
RUN CREATE DATABASE xplugintest_1

1 rows affected
RUN USE xplugintest

0 rows affected
RUN CREATE TABLE table1 ( _id INT , doc JSON ) 

0 rows affected

command ok

command ok
RUN SHOW TABLES
Tables_in_xplugintest
table1
table2
0 rows affected
RUN SHOW CREATE TABLE table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `_id` int DEFAULT NULL,
  `doc` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xplugintest_1.table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO table2 (doc) VALUES ('{"a":1,"_id":[100,200]}')

1 rows affected
RUN SELECT * FROM table2
doc	_id	_json_schema
{"a": 1, "_id": [100, 200]}	[100, 200]	{"type": "object"}
0 rows affected
RUN ALTER TABLE table2 ADD COLUMN col1 INT 

0 rows affected
Records: 0  Duplicates: 0  Warnings: 0
RUN SELECT * FROM table2
doc	_id	_json_schema	col1
{"a": 1, "_id": [100, 200]}	[100, 200]	{"type": "object"}	null
0 rows affected
RUN SHOW CREATE TABLE table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `_id` int DEFAULT NULL,
  `doc` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `col1` int DEFAULT NULL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO table2 (doc,col1) VALUES ('{"a":2,"_id":[300,400]}',300)

1 rows affected
RUN SELECT * FROM table2
doc	_id	_json_schema	col1
{"a": 1, "_id": [100, 200]}	[100, 200]	{"type": "object"}	null
{"a": 2, "_id": [300, 400]}	[300, 400]	{"type": "object"}	300
0 rows affected
RUN INSERT INTO table2 (doc,col1) VALUES ('{"a":3,"b":[400,400]}',400)
While executing INSERT INTO table2 (doc,col1) VALUES ('{"a":3,"b":[400,400]}',400):
Got expected error: Column '_id' cannot be null (code 1048)
RUN SELECT * FROM table2
doc	_id	_json_schema	col1
{"a": 1, "_id": [100, 200]}	[100, 200]	{"type": "object"}	null
{"a": 2, "_id": [300, 400]}	[300, 400]	{"type": "object"}	300
0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "table2"
    schema: "xplugintest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"a\":3,\"b\":[500,500]}"
        }
      }
    }
  }
}


1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000001
RUN SELECT * FROM table2
doc	_id	_json_schema	col1
{"a": 3, "b": [500, 500], "_id": "0000XXXXXXXX0000000000000001"}	0000XXXXXXXX0000000000000001	{"type": "object"}	null
{"a": 1, "_id": [100, 200]}	[100, 200]	{"type": "object"}	null
{"a": 2, "_id": [300, 400]}	[300, 400]	{"type": "object"}	300
0 rows affected
RUN ALTER TABLE table2 DROP COLUMN _id

3 rows affected
Records: 3  Duplicates: 0  Warnings: 0
RUN INSERT INTO table2 (doc,col1) VALUES ('{"a":4,"b":[400,400]}',400)

1 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "table2"
    schema: "xplugintest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"a\":5,\"b\":[500,500]}"
        }
      }
    }
  }
}


1 rows affected
auto-generated id(s): 0000XXXXXXXX0000000000000002
RUN SELECT * FROM table2
doc	_json_schema	col1
{"a": 3, "b": [500, 500], "_id": "0000XXXXXXXX0000000000000001"}	{"type": "object"}	null
{"a": 1, "_id": [100, 200]}	{"type": "object"}	null
{"a": 2, "_id": [300, 400]}	{"type": "object"}	300
{"a": 4, "b": [400, 400]}	{"type": "object"}	400
{"a": 5, "b": [500, 500], "_id": "0000XXXXXXXX0000000000000002"}	{"type": "object"}	null
0 rows affected
RUN SHOW CREATE TABLE table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `col1` int DEFAULT NULL,
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN DROP TABLE table2

0 rows affected

command ok
RUN SHOW CREATE TABLE table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  PRIMARY KEY (`_id`),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN CREATE TABLE table2_tmp ( col1 INT)

0 rows affected

command ok

command ok
RUN SHOW CREATE TABLE table2
While executing SHOW CREATE TABLE table2:
Got expected error: Table 'xplugintest.table2' doesn't exist (code 1146)
Got expected error: Unknown table 'xplugintest.table2' (code 1051)
Got expected error: Unknown table 'xplugintest_non_existing.table2' (code 1051)
Got expected error: Invalid number of arguments, expected value for 'name' (code 5015)
Got expected error: Invalid value for argument 'name' (code 5017)
Mysqlx.Ok {
  msg: "bye!"
}
ok
send Mysqlx.Sql.StmtExecute {
  stmt: "create table xplugintest.table_sql ( doc JSON ) "
  namespace: "sql"
}


0 rows affected
RUN SHOW CREATE TABLE xplugintest.table_sql
Table	Create Table
table_sql	CREATE TABLE `table_sql` (
  `doc` json DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN INSERT INTO xplugintest.table_sql (doc) VALUES ('{"_id":"200"}')

1 rows affected
RUN SELECT * FROM xplugintest.table_sql
doc
{"_id": "200"}
0 rows affected
RUN DROP TABLE xplugintest.table_sql

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN CREATE USER 'user1'@'localhost' identified by ''

0 rows affected
RUN GRANT USAGE ON *.* TO 'user1'@'localhost'

0 rows affected
RUN CREATE DATABASE xplugintest_user_access

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: CREATE command denied to user 'user1'@'localhost' for table 'table1' (code 1142)
RUN SHOW CREATE TABLE xplugintest_user_access.table1
While executing SHOW CREATE TABLE xplugintest_user_access.table1:
Got expected error: SHOW command denied to user 'user1'@'localhost' for table 'table1' (code 1142)
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN SHOW CREATE TABLE xplugintest_user_access.table1
While executing SHOW CREATE TABLE xplugintest_user_access.table1:
Got expected error: Table 'xplugintest_user_access.table1' doesn't exist (code 1146)
RUN DROP USER 'user1'@'localhost'

0 rows affected
RUN DROP DATABASE xplugintest_user_access

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: Invalid number of arguments, expected value for 'schema' (code 5015)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: 'alias' is not a valid field for create_collection command (code 5021)
Mysqlx.Ok {
  msg: "bye!"
}
ok
send Mysqlx.Sql.StmtExecute {
  stmt: "create_collection"
  args {
    type: OBJECT
    obj {
      fld {
        key: "schema"
        value {
          type: SCALAR
          scalar {
            type: V_STRING
            v_string {
              value: "xplugintest"
            }
          }
        }
      }
      fld {
        key: "name"
        value {
          type: SCALAR
          scalar {
            type: V_STRING
            v_string {
              value: "table2"
            }
          }
        }
      }
    }
  }
  namespace: "Invalid"
}

Got expected error: Unknown namespace Invalid (code 5162)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: Unknown database 'non_existing' (code 1049)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: Invalid value for argument 'name' (code 5017)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: Invalid value for argument 'schema' (code 5017)
Mysqlx.Ok {
  msg: "bye!"
}
ok
Got expected error: Table 'table1' already exists (code 1050)
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN DROP DATABASE IF EXISTS xplugintest

1 rows affected
RUN DROP DATABASE IF EXISTS xplugintest_1

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP DATABASE xplugintest;
DROP DATABASE xplugintest;
