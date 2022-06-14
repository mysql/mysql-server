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
}


3 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
