CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;

## A. Execute the test using zlib/group
#


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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


## Test setup
RUN DROP TABLE IF EXISTS xtest.coll

0 rows affected

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"1\", \"name\":\"first\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"2\", \"name\":\"second\", \"prio\":2}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"3\", \"name\":\"third\", \"prio\":1}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)
send Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"5\", \"name\":\"fifth\", \"prio\":4}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}


1 rows affected
Got expected error (code 0)


## I. Validate simple resultsets for CRUD messages
#
# 1. Assert resultset for CRUD-delete message
# 2. Assert resultset for CRUD-update message
# 3. Assert resultset for CRUD-create message (doc with id)
# 4. Assert resultset for CRUD-create message (doc without id)
# 5. Assert resultset for CRUD-read message

## II. Validate simple resultset with warnings
#
# 1. Assert one resultsets for select queries

#
# I.1
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: DELETE
    delete {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      criteria {
        type: OPERATOR
        operator {
          name: "=="
          param {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "name"
              }
            }
          }
          param {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "fifth"
              }
            }
          }
        }
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 1
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.2
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: UPDATE
    update {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      operation {
        source {
          document_path {
            type: MEMBER
            value: "prio"
          }
        }
        operation: ITEM_SET
        value {
          type: OPERATOR
          operator {
            name: "-"
            param {
              type: IDENT
              identifier {
                document_path {
                  type: MEMBER
                  value: "prio"
                }
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
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 2
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:3]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.3
send Mysqlx.Prepare.Prepare {
  stmt_id: 3
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 3
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.4
send Mysqlx.Prepare.Prepare {
  stmt_id: 4
  stmt {
    type: INSERT
    insert {
      collection {
        name: "coll"
        schema: "xtest"
      }
      data_model: DOCUMENT
      row {
        field {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "{\"name\":\"sixth\", \"prio\":3}"
              content_type: 2
            }
          }
        }
      }
      upsert: false
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 4
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT notice session state change - GENERATED_DOCUMENT_IDS, type: V_OCTETS]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# I.5
send Mysqlx.Prepare.Prepare {
  stmt_id: 5
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
      projection {
        source {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "name"
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 5
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
send Mysqlx.Prepare.Prepare {
  stmt_id: 6
  stmt {
    type: FIND
    find {
      collection {
        name: "coll"
      }
      data_model: DOCUMENT
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
                document_path {
                  type: MEMBER
                  value: "prio"
                }
              }
            }
          }
        }
        alias: "name"
      }
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 6
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
