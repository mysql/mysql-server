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
          value: "{"
        }
      }
    }
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 3140
  msg: "Invalid JSON text: \"Missing a name for object member.\" at position 1 in value for column \'mytable.j\'."
  sql_state: "22032"
}

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
          value: "badvalue"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{}"
        }
      }
    }
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 1292
  msg: "Incorrect datetime value: \'badvalue\' for column \'ts\' at row 1"
  sql_state: "22007"
}

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
        type: V_STRING
        v_string {
          value: "hello"
        }
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
          value: "{"
        }
      }
    }
  }
}

Mysqlx.Error {
  severity: ERROR
  code: 1366
  msg: "Incorrect integer value: \'hello\' for column \'ui\' at row 1"
  sql_state: "HY000"
}

Mysqlx.Ok {
  msg: "bye!"
}
ok
