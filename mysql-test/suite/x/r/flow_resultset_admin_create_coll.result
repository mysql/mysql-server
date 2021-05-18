CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;


## Check command flow when using a create_collection or
## ensure_collection admin commands.
#
# 1. Create valid collection.
# 2. Create collection and insert valid value.
# 3. Create collection and insert invalid value.
# 4. Create collection, insert valid value and update to valid value.
# 5. Create collection, insert valid value and update to invalid value.
# 6. Create collection which is already created in the system.
#    a) use create_collection command
#    b) use ensure_collection command

#
# -.1
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\":\"object\"}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.2
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.3
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 97}"
        }
      }
    }
  }
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.4
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Update {
  collection {
    name: "test_coll"
    schema: "xtest"
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
            value: "latitude"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 82
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "latitude"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 80
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.5
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Update {
  collection {
    name: "test_coll"
    schema: "xtest"
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
            value: "latitude"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 82
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "latitude"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 123
      }
    }
  }
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.6.a
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.6.b
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok


## Check command flow when using a create_collection or
## ensure_collection admin commands.
#
# 1. Create valid collection.
# 2. Create collection and insert valid value.
# 3. Create collection and insert invalid value.
# 4. Create collection, insert valid value and update to valid value.
# 5. Create collection, insert valid value and update to invalid value.
# 6. Create collection which is already created in the system.
#    a) use create_collection command
#    b) use ensure_collection command

#
# -.1
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\":\"object\"}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.2
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.3
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 97}"
        }
      }
    }
  }
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.4
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Update {
  collection {
    name: "test_coll"
    schema: "xtest"
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
            value: "latitude"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 82
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "latitude"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 80
      }
    }
  }
}

[ASSERT notice session state change - PRODUCED_MESSAGE, type: V_STRING]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.5
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Insert {
  collection {
    name: "test_coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"_id\": \"one\", \"latitude\": 82}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Crud.Update {
  collection {
    name: "test_coll"
    schema: "xtest"
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
            value: "latitude"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 82
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "latitude"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 123
      }
    }
  }
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.6.a
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Error]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected

#
# -.6.b
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Sql.StmtExecute {
  stmt: "ensure_collection"
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
              value: "xtest"
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
              value: "test_coll"
            }
          }
        }
      }
      fld {
        key: "options"
        value {
          type: OBJECT
          obj {
            fld {
              key: "validation"
              value {
                type: OBJECT
                obj {
                  fld {
                    key: "schema"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 90}},  \"required\": [\"latitude\"]}"
                        }
                      }
                    }
                  }
                  fld {
                    key: "level"
                    value {
                      type: SCALAR
                      scalar {
                        type: V_STRING
                        v_string {
                          value: "strict"
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  namespace: "mysqlx"
}

[ASSERT Mysqlx.Sql.StmtExecuteOk]
RUN DROP TABLE IF EXISTS xtest.test_coll

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
