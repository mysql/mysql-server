create schema xtest default charset 'utf8mb4';

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
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "doc"
  original_name: "doc"
  table: "mycoll"
  original_table: "mycoll"
  schema: "xtest"
  catalog: "def"
  collation: 63
  length: 4294967295
  content_type: 2
}

Mysqlx.Resultset.Row {
  field: "{\"_id\": \"1\", \"name\": \"Joe1\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"_id\": \"2\", \"name\": \"Joe2\", \"last_name\": \"Smith\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"_id\": \"3\", \"name\": \"Joe2\", \"last_name\": \"Shmo\"}\000"
}

Mysqlx.Resultset.Row {
  field: "{\"_id\": \"4\", \"name\": \"\345\260\217\345\263\266\", \"last_name\": \"Bla\"}\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Sql.StmtExecuteOk {
}


set empty document and array
............................
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "docfield"
      }
    }
    operation: ITEM_SET
    value {
      type: OBJECT
      object {
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "docarray"
      }
    }
    operation: ITEM_SET
    value {
      type: ARRAY
      array {
      }
    }
  }
}


4 rows affected
Rows matched: 4  Changed: 4  Warnings: 0
Mysqlx.Ok {
  msg: "bye!"
}
ok
