create schema xtest default charset='utf8mb4';
RUN use xtest

0 rows affected
RUN create table mytable (i int NOT NULL, ui int unsigned, vc varchar(100), f float, d double, dt datetime, ts timestamp(5), j json)

0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "mytable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "vc"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "x"
        }
      }
    }
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 1364
  msg: "Field \'i\' doesn\'t have a default value"
  sql_state: "HY000"
}

Mysqlx.Ok {
  msg: "bye!"
}
ok
