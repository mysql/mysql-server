create schema xtest default charset='utf8mb4';
RUN use xtest

0 rows affected
RUN create table ` .'"````\\` (` .'"````\\` int);


0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: " .\'\"``\\\\"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: " .\'\"``\\\\"
    alias: " .\'\"``\\\\"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 1
      }
    }
  }
}


1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
