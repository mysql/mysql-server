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
    schema: ""
  }
  data_model: DOCUMENT
}


3 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
    schema: ""
  }
  data_model: TABLE
}


0 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: ""
    schema: "xtest"
  }
  data_model: DOCUMENT
}

Got expected error: Invalid name of table/collection (code 5113)
send Mysqlx.Crud.Delete {
  collection {
    name: ""
    schema: "xtest"
  }
  data_model: TABLE
}

Got expected error: Invalid name of table/collection (code 5113)
Mysqlx.Ok {
  msg: "bye!"
}
ok
