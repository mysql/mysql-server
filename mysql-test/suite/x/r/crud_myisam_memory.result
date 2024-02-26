SET @start_global_value = @@global.default_storage_engine;
SELECT @start_global_value;
@start_global_value
InnoDB
SET @@global.default_storage_engine = myisam;
RUN DROP DATABASE IF EXISTS xplugintest

0 rows affected
RUN CREATE DATABASE xplugintest

1 rows affected
RUN USE xplugintest

0 rows affected
RUN CREATE TABLE `table1` (
  `doc` VARCHAR(100) DEFAULT NULL,
  `_id` INT AUTO_INCREMENT PRIMARY KEY
) ENGINE=memory

0 rows affected

command ok
Insert multiple rows into a Document column : myisam table
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
          value: "\n{\n         \"_id\": \"1\",\n         \"name\": \"Omar Bras\", \"id\": \"1\"\n}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\n{\n         \"_id\": \"2\",\n         \"name\": \"Omar Mex\", \"id\": \"2\"\n}"
        }
      }
    }
  }
}


2 rows affected
Records: 2  Duplicates: 0  Warnings: 0
Insert into memory table
send Mysqlx.Crud.Insert {
  collection {
    name: "table1"
    schema: "xplugintest"
  }
  data_model: TABLE
  projection {
    name: "_id"
  }
  projection {
    name: "doc"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 10
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Omar Bras"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 11
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Omar Mex"
        }
      }
    }
  }
}


2 rows affected
last insert id: 11
Records: 2  Duplicates: 0  Warnings: 0
RUN SELECT * FROM xplugintest.table2
doc	_id	_json_schema
{"id": "1", "_id": "1", "name": "Omar Bras"}	1	{"type": "object"}
{"id": "2", "_id": "2", "name": "Omar Mex"}	2	{"type": "object"}
0 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id
Omar Bras	10
Omar Mex	11
0 rows affected
"Index with  unique(true), index column NOT NULL (true)":myisam table

command ok
RUN SHOW CREATE TABLE xplugintest.table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7` text GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.name'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `name_index` (`$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7`(50)),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` varchar(100) DEFAULT NULL,
  `_id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`_id`)
) ENGINE=MEMORY AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SELECT * FROM xplugintest.table2
doc	_id	_json_schema	$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7
{"id": "1", "_id": "1", "name": "Omar Bras"}	1	{"type": "object"}	Omar Bras
{"id": "2", "_id": "2", "name": "Omar Mex"}	2	{"type": "object"}	Omar Mex
0 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id
Omar Bras	10
Omar Mex	11
0 rows affected
Update - myisam table
send Mysqlx.Crud.Update {
  collection {
    name: "table2"
    schema: "xplugintest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 2
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"Omar Argentina\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
Update value - memory table
send Mysqlx.Crud.Update {
  collection {
    name: "table1"
    schema: "xplugintest"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 10
        }
      }
    }
  }
  operation {
    source {
      name: "doc"
    }
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Omar Argentina"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SHOW CREATE TABLE xplugintest.table2
Table	Create Table
table2	CREATE TABLE `table2` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7` text GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.name'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `name_index` (`$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7`(50)),
  CONSTRAINT `$val_strict_8A6DECD6D7FCE389DA6206C4247042D2D76E08AD` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SHOW CREATE TABLE xplugintest.table1
Table	Create Table
table1	CREATE TABLE `table1` (
  `doc` varchar(100) DEFAULT NULL,
  `_id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`_id`)
) ENGINE=MEMORY AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
RUN SELECT JSON_EXTRACT(doc,'$.name') FROM xplugintest.table2
JSON_EXTRACT(doc,'$.name')
"Omar Bras"
"\"Omar Argentina\""
0 rows affected
RUN SELECT * FROM xplugintest.table2
doc	_id	_json_schema	$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7
{"id": "1", "_id": "1", "name": "Omar Bras"}	1	{"type": "object"}	Omar Bras
{"id": "2", "_id": "2", "name": "\"Omar Argentina\""}	2	{"type": "object"}	"Omar Argentina"
0 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id
Omar Argentina	10
Omar Mex	11
0 rows affected
Delete with == operator : myisam table
send Mysqlx.Crud.Delete {
  collection {
    name: "table2"
    schema: "xplugintest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 2
        }
      }
    }
  }
}


1 rows affected
Delete with == operator : memory table
send Mysqlx.Crud.Delete {
  collection {
    name: "table1"
    schema: "xplugintest"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 11
        }
      }
    }
  }
}


1 rows affected
RUN SELECT JSON_EXTRACT(doc,'$.name') FROM xplugintest.table2
JSON_EXTRACT(doc,'$.name')
"Omar Bras"
0 rows affected
RUN SELECT * FROM xplugintest.table2
doc	_id	_json_schema	$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7
{"id": "1", "_id": "1", "name": "Omar Bras"}	1	{"type": "object"}	Omar Bras
0 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id
Omar Argentina	10
0 rows affected
insert : myisam table
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
          value: "\n{\n         \"_id\": \"3\",\n         \"name\": \"Omar Peru\", \"id\": \"3\"\n}"
        }
      }
    }
  }
}


1 rows affected
insert : memory table
send Mysqlx.Crud.Insert {
  collection {
    name: "table1"
    schema: "xplugintest"
  }
  data_model: TABLE
  projection {
    name: "_id"
  }
  projection {
    name: "doc"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 12
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Omar Peru"
        }
      }
    }
  }
}


1 rows affected
last insert id: 12
# Insert fail due to non unique _id doc member: myisam table
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
          value: "\n{\n         \"_id\": \"1\",\n         \"name\": \"Omar Colombia\", \"id\": \"3\"\n}"
        }
      }
    }
  }
}

Got expected error: Document contains a field value that is not unique but required to be (code 5116)
# Insert fail due to non unique _id doc member: memory table
send Mysqlx.Crud.Insert {
  collection {
    name: "table1"
    schema: "xplugintest"
  }
  data_model: TABLE
  projection {
    name: "_id"
  }
  projection {
    name: "doc"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 10
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Omar Colombia"
        }
      }
    }
  }
}

Got expected error: Duplicate entry '10' for key 'table1.PRIMARY' (code 1062)
RUN SELECT * FROM xplugintest.table2
doc	_id	_json_schema	$ix_t50_r_2BAA686C5604372BA51E965E6346198F5072C3D7
{"id": "1", "_id": "1", "name": "Omar Bras"}	1	{"type": "object"}	Omar Bras
{"id": "3", "_id": "3", "name": "Omar Peru"}	3	{"type": "object"}	Omar Peru
0 rows affected
RUN SELECT * FROM xplugintest.table1
doc	_id
Omar Argentina	10
Omar Peru	12
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP DATABASE IF EXISTS xplugintest;
SET @@global.default_storage_engine = @start_global_value;
SELECT @@global.default_storage_engine;
@@global.default_storage_engine
InnoDB
