create schema xtest default charset='utf8mb4';
RUN use xtest

0 rows affected
RUN create table simpletable (id int auto_increment primary key, value int)

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
    name: "i"
  }
  projection {
    name: "ui"
  }
  projection {
    name: "vc"
  }
  projection {
    name: "f"
  }
  projection {
    name: "d"
  }
  projection {
    name: "dt"
  }
  projection {
    name: "ts"
  }
  projection {
    name: "j"
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 5013
  msg: "Missing row data for Insert"
  sql_state: "HY000"
}

RUN select count(*) from xtest.mytable
count(*)
0
0 rows affected
RUN truncate table xtest.mytable

0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "mytable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "i"
  }
  projection {
    name: "ui"
  }
  projection {
    name: "vc"
  }
  projection {
    name: "f"
  }
  projection {
    name: "d"
  }
  projection {
    name: "dt"
  }
  projection {
    name: "ts"
  }
  projection {
    name: "j"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: -100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: ""
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 0.333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 0.333333333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-05-05 1:20:30"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-06-06 03:10:20.12345"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"name\":\"Me\"}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 50
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "new\'string"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 0
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2000-1-1 1:1:1"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "1999-2-2 2:2:2.22222"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 1048
  msg: "Column \'i\' cannot be null"
  sql_state: "23000"
}

RUN select count(*) from xtest.mytable
count(*)
0
0 rows affected
RUN truncate table xtest.mytable

0 rows affected
Valid cases
send Mysqlx.Crud.Insert {
  collection {
    name: "mytable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "i"
  }
  projection {
    name: "ui"
  }
  projection {
    name: "vc"
  }
  projection {
    name: "f"
  }
  projection {
    name: "d"
  }
  projection {
    name: "dt"
  }
  projection {
    name: "ts"
  }
  projection {
    name: "j"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: -100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "string"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 0.333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 0.333333333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-05-05 1:20:30"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-06-06 03:10:20.12345"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"name\":\"Me\"}"
        }
      }
    }
  }
}


1 rows affected
RUN select count(*) from xtest.mytable
count(*)
1
0 rows affected
RUN truncate table xtest.mytable

0 rows affected
send Mysqlx.Crud.Insert {
  collection {
    name: "mytable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "i"
  }
  projection {
    name: "ui"
  }
  projection {
    name: "vc"
  }
  projection {
    name: "f"
  }
  projection {
    name: "d"
  }
  projection {
    name: "dt"
  }
  projection {
    name: "ts"
  }
  projection {
    name: "j"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: -100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 100
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: ""
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 0.333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 0.333333333333
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-05-05 1:20:30"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2015-06-06 03:10:20.12345"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"name\":\"Me\"}"
        }
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: -50
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 50
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "new\'string"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 0
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "2000-1-1 1:1:1"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "1999-2-2 2:2:2.22222"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}


2 rows affected
Records: 2  Duplicates: 0  Warnings: 0
send Mysqlx.Crud.Insert {
  collection {
    name: "simpletable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    name: "value"
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_double: 5
      }
    }
  }
  row {
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_double: 6
      }
    }
  }
}


2 rows affected
last insert id: 1
Records: 2  Duplicates: 0  Warnings: 0
RUN select count(*) from xtest.mytable
count(*)
2
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
