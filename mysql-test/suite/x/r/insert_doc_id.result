create schema xtest default charset='utf8mb4';

command ok
send Mysqlx.Crud.Insert {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\n{\n         \"_id\": \"X999_Y999\",\n         \"from\": {\n            \"name\": \"Tom Brady\", \"id\": \"X12\"\n         },\n         \"message\": \"Looking forward to 2010!\",\n         \"actions\": [\n            {\n               \"name\": \"Comment\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y999\"\n            },\n            {\n               \"name\": \"Like\",\n               \"link\": \"http://www.facebook.com/X999/posts/Y999\"\n            }\n         ],\n         \"type\": \"status\",\n         \"created_time\": \"2010-08-02T21:27:44+0000\",\n         \"updated_time\": \"2010-08-02T21:27:44+0000\"\n      }\n"
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
