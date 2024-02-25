create schema xtest default charset 'utf8mb4';

command ok
RUN use xtest

0 rows affected
RUN insert into fancycoll (doc) values ('\n{\n         \"_id\": \"X999_Y999\",\n         \"from\": {\n            \"name\": \"Tom Brady\", \"id\": \"X12\"\n         },\n         \"message\": \"Looking forward to 2010!\",\n         \"actions\": [\n            {\n               \"name\": \"Comment\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y999\"\n            },\n            {\n               \"name\": \"Like\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y999\"\n            }\n         ],\n         \"type\": \"status\",\n         \"created_time\": \"2010-08-02T21:27:44+0000\",\n         \"updated_time\": \"2010-08-02T21:27:44+0000\"\n      }\n')

1 rows affected
RUN insert into fancycoll (doc) values ('\n{\n         \"_id\": \"X999_Y998\",\n         \"from\": {\n            \"name\": \"Dom Crady\", \"id\": \"A12\"\n         },\n         \"message\": \"Looking forward to 2034!\",\n         \"actions\": [\n            {\n               \"name\": \"Comment\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y222\"\n            },\n            {\n               \"name\": \"Like\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y222\"\n            }\n         ],\n         \"type\": \"status\",\n         \"created_time\": \"2011-08-02T21:27:44+0000\",\n         \"updated_time\": \"2011-08-02T21:27:44+0000\"\n      }\n')

1 rows affected
send Mysqlx.Crud.Find {
  collection {
    name: "fancycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "from"
          }
          document_path {
            type: MEMBER
            value: "id"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "X12"
          }
        }
      }
    }
  }
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: "doc"
  table: "fancycoll"
  original_table: "fancycoll"
  schema: "xtest"
  catalog: "def"
  collation: 63
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"_id\": \"X999_Y998\", \"from\": {\"id\": \"A12\", \"name\": \"Dom Crady\"}, \"type\": \"status\", \"actions\": [{\"link\": \"http://www.facebook.com/X999/posts/Y222\", \"name\": \"Comment\"}, {\"link\": \"http://www.facebook.com/X999/posts/Y222\", \"name\": \"Like\"}], \"message\": \"Looking forward to 2034!\", \"created_time\": \"2011-08-02T21:27:44+0000\", \"updated_time\": \"2011-08-02T21:27:44+0000\"}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}

Mysqlx.Ok {
  msg: "bye!"
}
ok
