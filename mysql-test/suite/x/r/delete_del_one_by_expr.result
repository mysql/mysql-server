create schema xtest;

command ok
RUN use xtest

0 rows affected
RUN insert into mycoll (doc) values ('{"_id": "1", "name": "Joe1"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "2", "name": "Joe2", "last_name": "Smith"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "3", "name": "Joe2", "last_name": "Shmo"}')

1 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
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
            value: "_id"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "1"
          }
        }
      }
    }
  }
}


1 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
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
            value: "last_name"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "Smith"
          }
        }
      }
    }
  }
}


1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
