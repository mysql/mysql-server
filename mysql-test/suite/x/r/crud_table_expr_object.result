RUN DROP SCHEMA IF EXISTS xtest

0 rows affected
RUN CREATE SCHEMA xtest

1 rows affected
RUN CREATE TABLE xtest.xtable (xfield JSON)

0 rows affected
Inserting object into table
send Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "xfield"
  }
  row {
    field {
      type: OBJECT
      object {
        fld {
          key: "_id"
          value {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "1"
              }
            }
          }
        }
        fld {
          key: "first_name"
          value {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "John"
              }
            }
          }
        }
        fld {
          key: "last_name"
          value {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "Snow"
              }
            }
          }
        }
      }
    }
  }
}


1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"_id": "1", "last_name": "Snow", "first_name": "John"}
0 rows affected
Selecting object from table with alias
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: OBJECT
      object {
        fld {
          key: "name"
          value {
            type: FUNC_CALL
            function_call {
              name {
                name: "concat"
              }
              param {
                type: IDENT
                identifier {
                  document_path {
                    type: MEMBER
                    value: "first_name"
                  }
                  name: "xfield"
                }
              }
              param {
                type: LITERAL
                literal {
                  type: V_OCTETS
                  v_octets {
                    value: " "
                  }
                }
              }
              param {
                type: IDENT
                identifier {
                  document_path {
                    type: MEMBER
                    value: "last_name"
                  }
                  name: "xfield"
                }
              }
            }
          }
        }
        fld {
          key: "number"
          value {
            type: OPERATOR
            operator {
              name: "+"
              param {
                type: LITERAL
                literal {
                  type: V_SINT
                  v_signed_int: 1
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
    alias: "result"
  }
}

result
{"name": "John Snow", "number": 2}
command ok
Selecting object with sub-object from table
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: OBJECT
      object {
        fld {
          key: "name"
          value {
            type: FUNC_CALL
            function_call {
              name {
                name: "concat"
              }
              param {
                type: IDENT
                identifier {
                  document_path {
                    type: MEMBER
                    value: "first_name"
                  }
                  name: "xfield"
                }
              }
              param {
                type: LITERAL
                literal {
                  type: V_OCTETS
                  v_octets {
                    value: " "
                  }
                }
              }
              param {
                type: IDENT
                identifier {
                  document_path {
                    type: MEMBER
                    value: "last_name"
                  }
                  name: "xfield"
                }
              }
            }
          }
        }
        fld {
          key: "detail"
          value {
            type: OBJECT
            object {
              fld {
                key: "first"
                value {
                  type: IDENT
                  identifier {
                    document_path {
                      type: MEMBER
                      value: "first_name"
                    }
                    name: "xfield"
                  }
                }
              }
              fld {
                key: "last"
                value {
                  type: IDENT
                  identifier {
                    document_path {
                      type: MEMBER
                      value: "last_name"
                    }
                    name: "xfield"
                  }
                }
              }
            }
          }
        }
      }
    }
    alias: "result"
  }
}

result
{"name": "John Snow", "detail": {"last": "Snow", "first": "John"}}
command ok
Updating table by object
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "occupation"
      }
      name: "xfield"
    }
    operation: ITEM_SET
    value {
      type: OBJECT
      object {
        fld {
          key: "location"
          value {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "the wall"
              }
            }
          }
        }
        fld {
          key: "title"
          value {
            type: LITERAL
            literal {
              type: V_OCTETS
              v_octets {
                value: "lord commander"
              }
            }
          }
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"_id": "1", "last_name": "Snow", "first_name": "John", "occupation": {"title": "lord commander", "location": "the wall"}}
0 rows affected
NOT Error: empty object
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: OBJECT
      object {
      }
    }
    alias: "result"
  }
}

result
{}
command ok
Error: empty object key
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: OBJECT
      object {
        fld {
          key: ""
          value {
            type: IDENT
            identifier {
              document_path {
                type: MEMBER
                value: "first_name"
              }
              name: "xfield"
            }
          }
        }
      }
    }
    alias: "result"
  }
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5154
  msg: "Invalid key for Mysqlx::Expr::Object"
  sql_state: "HY000"
}

RUN drop schema if exists xtest

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
