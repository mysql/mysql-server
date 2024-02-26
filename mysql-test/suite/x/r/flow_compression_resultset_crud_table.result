CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE PROCEDURE recreate_tables()
BEGIN
CREATE TABLE IF NOT EXISTS xtable (phrase VARCHAR(30), prio INTEGER);
TRUNCATE TABLE xtable;
INSERT INTO xtable (phrase, prio) VALUES ('Alice has a cat', 1);
INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 1', 2);
INSERT INTO xtable (phrase, prio) VALUES ('Peppa has a teddy', 1);
INSERT INTO xtable (phrase, prio) VALUES ('Bob has a dog 2', 3);
INSERT INTO xtable (phrase, prio) VALUES ('Row to delete', 4);
CREATE TABLE IF NOT EXISTS xtable_with_pk (p_id INTEGER AUTO_INCREMENT PRIMARY KEY, description varchar(40));
TRUNCATE TABLE xtable_with_pk;
INSERT INTO xtable_with_pk (description) VALUES ('MyRow');
END //
## A. Execute the test using zlib/group
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## B. Execute the test using zlib/multiple
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## C. Execute the test using zlib/single
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## D. Execute the test using lz4/group
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## E. Execute the test using lz4/multiple
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## F. Execute the test using lz4/single
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## G. Execute the test using zstd/group
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## H. Execute the test using zstd/multiple
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
## I. Execute the test using zstd/single
#
CALL recreate_tables();


## Test setup


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (table with auto_inc column)
# 4. Assert resultset for CRUD-create message (table without auto_inc column)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:4]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  data_model: TABLE
  projection {
    name: "description"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_INSERT_ID, v_unsigned_int: 2]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
    alias: "prio"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# II.1
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
    alias: "name"
  }
}

[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice warning - WARNING, 1365]
[ASSERT notice warning - WARNING, 1365]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
Mysqlx.Ok {
  msg: "bye!"
}
ok
