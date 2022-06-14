create schema xtest default charset 'utf8mb4';

command ok

command ok
RUN use xtest

0 rows affected
RUN insert into mycoll (doc) values ('{"_id": "1", "name": "Joe1"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "2", "name": "Joe2", "last_name": "Smith"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "3", "name": "Joe2", "last_name": "Shmo"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "4", "name": "小島", "last_name": "Bla"}')

1 rows affected
RUN insert into funkycoll (doc) values ('{"_id": "1", "I love \\\"\'quotes\'\\\"\\n": "I love \\\"\'quotes\'\\\"\\n"}')

1 rows affected
RUN insert into funkycoll (doc) values ('{"_id": "2", "I love \\\"\'quotes\'\\\"\\n": "\\\",$.array[2]\\\\\\\""}')

1 rows affected
RUN insert into funkycoll (doc) values ('{"_id": "3", "\\\",$.array[2]\\\\\\\"": "I love \\\"\'quotes\'\\\"\\n"}')

1 rows affected
RUN insert into funkycoll (doc) values ('{"_id": "4", "\\\",$.array[2]\\\\\\\"": "\\\",$.array[2]\\\\\\\""}')

1 rows affected
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
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
    alias: "Name"
  }
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: ""
  table: ""
  original_table: ""
  schema: ""
  catalog: "def"
  collation: 46
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"Name\": \"Joe1\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"Name\": \"Joe2\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"Name\": \"Joe2\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"Name\": \"\345\260\217\345\263\266\"}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}

send Mysqlx.Crud.Find {
  collection {
    name: "funkycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  projection {
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "I love \"\'quotes\'\"\n"
        }
      }
    }
    alias: "key"
  }
  projection {
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "_id"
        }
      }
    }
    alias: "\"\'"
  }
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: ""
  table: ""
  original_table: ""
  schema: ""
  catalog: "def"
  collation: 46
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"\\\"\'\": \"1\", \"key\": \"I love \\\"\'quotes\'\\\"\\n\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"\\\"\'\": \"2\", \"key\": \"\\\",$.array[2]\\\\\\\"\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"\\\"\'\": \"3\", \"key\": null}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"\\\"\'\": \"4\", \"key\": null}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}

send Mysqlx.Crud.Find {
  collection {
    name: "funkycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  projection {
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "I love \"\'quotes\'\"\n\\/\010\014\r\t"
        }
      }
    }
    alias: "I love \"\'quotes\'\"\n"
  }
  projection {
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "_id"
        }
      }
    }
    alias: "\"\'"
  }
  limit {
    row_count: 1
    offset: 2
  }
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: ""
  table: ""
  original_table: ""
  schema: ""
  catalog: "def"
  collation: 46
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"\\\"\'\": \"3\", \"I love \\\"\'quotes\'\\\"\\n\": null}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}

send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
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
  projection {
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "last_name"
        }
      }
    }
    alias: "last_name"
  }
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: ""
  table: ""
  original_table: ""
  schema: ""
  catalog: "def"
  collation: 46
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"name\": \"Joe1\", \"last_name\": null}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"name\": \"Joe2\", \"last_name\": \"Smith\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"name\": \"Joe2\", \"last_name\": \"Shmo\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"name\": \"\345\260\217\345\263\266\", \"last_name\": \"Bla\"}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}

send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  projection {
    source {
      type: OBJECT
      object {
        fld {
          key: "X"
          value {
            type: OPERATOR
            operator {
              name: "-"
              param {
                type: OPERATOR
                operator {
                  name: "cast"
                  param {
                    type: LITERAL
                    literal {
                      type: V_SINT
                      v_signed_int: 0
                    }
                  }
                  param {
                    type: LITERAL
                    literal {
                      type: V_OCTETS
                      v_octets {
                        value: "UNSIGNED"
                      }
                    }
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
}

Got expected error: BIGINT UNSIGNED value is out of range in '(cast(0 as unsigned) - 1)' (code 1690)
Mysqlx.Ok {
  msg: "bye!"
}
ok
