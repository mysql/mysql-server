CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;


## I. Check command flow when using a modify_collection_options admin
##    command.
#
# 1. Modify existing schema with other valid schema.
# 2. Modify existing schema with schema that does not match to existing
#    collection content.

#
# I.1
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
          value: "{\"_id\": \"two\", \"latitude\": 49}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Sql.StmtExecute {
  stmt: "modify_collection_options"
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
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 100}},  \"required\": [\"latitude\"]}"
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
# I.2
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
          value: "{\"_id\": \"two\", \"latitude\": 49}"
        }
      }
    }
  }
}

[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:1]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
send Mysqlx.Sql.StmtExecute {
  stmt: "modify_collection_options"
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
                          value: "{\"type\": \"object\", \"properties\":  {\"latitude\": {\"type\": \"number\", \"minimum\": -90, \"maximum\": 50}},  \"required\": [\"latitude\"]}"
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
Mysqlx.Ok {
  msg: "bye!"
}
ok
