create schema xtest;

command ok
RUN use xtest

0 rows affected
RUN insert into mycoll (doc) values ('{"_id": "1", "name": "Robb"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "2", "name": "Bran"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "3", "name": "Arya"}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "4", "name": "Sansa"}')

1 rows affected
RUN create table mytable (id int primary key, name varchar(40), price decimal(5,2), info json)

0 rows affected
RUN insert into mytable values (1, 'banana', 1.20, '{"color": "yellow"}')

1 rows affected
RUN insert into mytable values (2, 'apple', 0.25, '{"color":"red"}')

1 rows affected
RUN insert into mytable values (3, 'tomato', 1.80, '{"color":"red"}')

1 rows affected
RUN insert into mytable values (4, 'mango', 3.14, '{"color":"green"}')

1 rows affected
RUN insert into mytable values (5, 'orange', 0.90, '{"color":"orange"}')

1 rows affected
RUN SELECT * FROM xtest.mytable ORDER BY price
id	name	price	info
2	apple	0.25	{"color": "red"}
5	orange	0.90	{"color": "orange"}
1	banana	1.20	{"color": "yellow"}
3	tomato	1.80	{"color": "red"}
4	mango	3.14	{"color": "green"}
0 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: "mytable"
    schema: "xtest"
  }
  data_model: TABLE
  limit {
    row_count: 2
  }
  order {
    expr {
      type: IDENT
      identifier {
        name: "price"
      }
    }
  }
}


2 rows affected
RUN SELECT * FROM xtest.mytable
id	name	price	info
1	banana	1.20	{"color": "yellow"}
3	tomato	1.80	{"color": "red"}
4	mango	3.14	{"color": "green"}
0 rows affected
RUN SELECT doc FROM xtest.mycoll ORDER BY JSON_UNQUOTE(JSON_EXTRACT(doc,'$.name'))
doc
{"_id": "3", "name": "Arya"}
{"_id": "2", "name": "Bran"}
{"_id": "1", "name": "Robb"}
{"_id": "4", "name": "Sansa"}
0 rows affected
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  limit {
    row_count: 2
  }
  order {
    expr {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "name"
        }
      }
    }
  }
}


2 rows affected
RUN SELECT doc FROM xtest.mycoll ORDER BY JSON_UNQUOTE(JSON_EXTRACT(doc,'$.name'))
doc
{"_id": "1", "name": "Robb"}
{"_id": "4", "name": "Sansa"}
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
