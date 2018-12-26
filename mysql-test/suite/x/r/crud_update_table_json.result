RUN DROP SCHEMA IF EXISTS xtest

0 rows affected
RUN CREATE SCHEMA xtest

1 rows affected
RUN CREATE TABLE xtest.xtable (xfield JSON)

0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"first\":1.1, \"second\": \"two.1\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.1, "second": "two.1"}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "first"
      }
      name: "xfield"
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "second"
      }
      name: "xfield"
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "two.1"
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "third"
      }
      name: "xfield"
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 3.1
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.1, "third": 3.1, "second": "two.1"}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "first"
      }
      name: "xfield"
    }
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"second": "two"}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "first"
      }
      name: "xfield"
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "third"
      }
      name: "xfield"
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "three.1"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.1, "second": "two"}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"third\":3.0, \"fourth\": \"four\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "third": 3.0, "fourth": "four", "second": "two"}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": ["two"]}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": ["two"]}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "second"
      }
      document_path {
        type: ARRAY_INDEX
        index: 0
      }
      name: "xfield"
    }
    operation: ARRAY_INSERT
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "two.1"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": ["two.1", "two"]}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "second"
      }
      name: "xfield"
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "two.1"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": ["two", "two.1"]}
0 rows affected
RUN TRUNCATE TABLE xtest.xtable

0 rows affected
RUN INSERT INTO xtest.xtable VALUES('{"first":1.0, "second": "two"}')

1 rows affected
RUN SELECT * FROM xtest.xtable
xfield
{"first": 1.0, "second": "two"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      document_path {
        type: MEMBER
        value: "second"
      }
      name: "xfield"
    }
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"first\":1.1}"
        }
      }
    }
  }
}

Got expected error: Invalid column name to update (code 5052)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: ""
    }
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"first\":1.1}"
        }
      }
    }
  }
}

Got expected error: Invalid column name to update (code 5052)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
}

Got expected error: Invalid member location (code 5053)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ITEM_REMOVE
  }
}

Got expected error: Invalid member location (code 5053)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
}

Got expected error: Invalid member location (code 5053)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ARRAY_INSERT
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
}

Got expected error: Invalid member location (code 5053)
send Mysqlx.Crud.Update {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  operation {
    source {
      name: "xfield"
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1.1
      }
    }
  }
}

Got expected error: Invalid member location (code 5053)
RUN DROP SCHEMA IF EXISTS xtest

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
