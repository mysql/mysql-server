=============================================
CRUD DOC CRITERIA ARGS SCENARIOS
=============================================

================================================================================
PREAMBLE
================================================================================
================================================================================
TEST START
================================================================================
create schema xtest;

command ok
RUN use xtest

0 rows affected
RUN insert into mycoll (doc) values ('{"_id": "1", "name": "Robb", "amount": 125.21}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "2", "name": "Bran", "amount": 542.75}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "3", "name": "Arya", "amount": 98.89}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "4", "name": "Tassadar", "amount": 57.52}')

1 rows affected
RUN insert into mycoll (doc) values ('{"_id": "5", "name": "Sansa", "amount": null}')

1 rows affected
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "3", "name": "Arya", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
0 rows affected
Find with > Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}

doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
command ok
Find with < Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "<"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}

doc
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
command ok
Find with >= Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}

doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "3", "name": "Arya", "amount": 98.89}
command ok
Find with <= Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "<="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}

doc
{"_id": "3", "name": "Arya", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
command ok
Find with <= Operator, placeholder and Order desc
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  order {
    expr {
      type: IDENT
      identifier {
        name: "_id"
      }
    }
    direction: DESC
  }
  args {
    type: V_DOUBLE
    v_double: 0
  }
}

doc
{"_id": "5", "name": "Sansa", "amount": null}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "3", "name": "Arya", "amount": 98.89}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "1", "name": "Robb", "amount": 125.21}
command ok
Find with <= Operator, placeholder and Order asc
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  order {
    expr {
      type: IDENT
      identifier {
        name: "_id"
      }
    }
    direction: ASC
  }
  args {
    type: V_DOUBLE
    v_double: 0
  }
}

doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "3", "name": "Arya", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
command ok
Find with == Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "Robb"
    }
  }
}

doc
{"_id": "1", "name": "Robb", "amount": 125.21}
command ok
Find with != Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "Robb"
    }
  }
}

doc
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "3", "name": "Arya", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
command ok
Find with in Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "in"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "_id"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "3"
    }
  }
}

doc
{"_id": "3", "name": "Arya", "amount": 98.89}
command ok
Find with not_in Operator and placeholder
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "not_in"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "_id"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "3"
    }
  }
}

doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
command ok
Update with == operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "Update_=="
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Robb", "amount": 125.21}
{"_id": "2", "name": "Bran", "amount": 542.75}
{"_id": "3", "name": "Update_==", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
0 rows affected
Update with > operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "Update_>"
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Update_>", "amount": 125.21}
{"_id": "2", "name": "Update_>", "amount": 542.75}
{"_id": "3", "name": "Update_==", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
0 rows affected
Update with >= operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "Update_>="
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Update_>=", "amount": 125.21}
{"_id": "2", "name": "Update_>=", "amount": 542.75}
{"_id": "3", "name": "Update_>=", "amount": 98.89}
{"_id": "4", "name": "Tassadar", "amount": 57.52}
{"_id": "5", "name": "Sansa", "amount": null}
0 rows affected
Update with <= operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "<="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "Update_<="
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 98.89
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Update_>=", "amount": 125.21}
{"_id": "2", "name": "Update_>=", "amount": 542.75}
{"_id": "3", "name": "Update_<=", "amount": 98.89}
{"_id": "4", "name": "Update_<=", "amount": 57.52}
{"_id": "5", "name": "Update_<=", "amount": null}
0 rows affected
Update with Float operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 256.51
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "Update_<="
    }
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Update_>=", "amount": 125.21}
{"_id": "2", "name": "Update_>=", "amount": 542.75}
{"_id": "3", "name": "Update_<=", "amount": 256.51}
{"_id": "4", "name": "Update_<=", "amount": 256.51}
{"_id": "5", "name": "Update_<=", "amount": 256.51}
0 rows affected
Update with Double operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 256.51
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "Update_>="
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "Update_>=", "amount": 256.51}
{"_id": "2", "name": "Update_>=", "amount": 256.51}
{"_id": "3", "name": "Update_<=", "amount": 256.51}
{"_id": "4", "name": "Update_<=", "amount": 256.51}
{"_id": "5", "name": "Update_<=", "amount": 256.51}
0 rows affected
Update with String operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "amount"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "watermelon"
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 256.51
  }
}


5 rows affected
Rows matched: 5  Changed: 5  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon", "amount": 256.51}
{"_id": "2", "name": "watermelon", "amount": 256.51}
{"_id": "3", "name": "watermelon", "amount": 256.51}
{"_id": "4", "name": "watermelon", "amount": 256.51}
{"_id": "5", "name": "watermelon", "amount": 256.51}
0 rows affected
Update with Null operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_SET
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "watermelon"
    }
  }
}


5 rows affected
Rows matched: 5  Changed: 5  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon", "amount": null}
{"_id": "2", "name": "watermelon", "amount": null}
{"_id": "3", "name": "watermelon", "amount": null}
{"_id": "4", "name": "watermelon", "amount": null}
{"_id": "5", "name": "watermelon", "amount": null}
0 rows affected
Update with ITEM_REPLACE operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "watermelon_replace"
        }
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "watermelon"
    }
  }
}


5 rows affected
Rows matched: 5  Changed: 5  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "2", "name": "watermelon_replace", "amount": null}
{"_id": "3", "name": "watermelon_replace", "amount": null}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
0 rows affected
Update with ITEM_REMOVE operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "name"
      }
    }
    operation: ITEM_REMOVE
  }
  args {
    type: V_DOUBLE
    v_double: 3
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "2", "name": "watermelon_replace", "amount": null}
{"_id": "3", "amount": null}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
0 rows affected
Update with ARRAY_APPEND operator and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "Update_ArrayAppend"
        }
      }
    }
  }
  args {
    type: V_DOUBLE
    v_double: 3
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "2", "name": "watermelon_replace", "amount": null}
{"_id": "3", "amount": [null, "Update_ArrayAppend"]}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
0 rows affected
Update with ITEM_MERGE value and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      name: "doc"
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
  args {
    type: V_UINT
    v_unsigned_int: 2
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "2", "name": "watermelon_replace", "third": 3.0, "amount": null, "fourth": "four"}
{"_id": "3", "amount": [null, "Update_ArrayAppend"]}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
0 rows affected
RUN insert into mycoll (doc) values ('{"_id": "6", "third": ["two"]}')

1 rows affected
Update with ARRAY_INSERT value and placeholder
send Mysqlx.Crud.Update {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "third"
      }
      document_path {
        type: ARRAY_INDEX
        index: 0
      }
      name: "doc"
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
  args {
    type: V_UINT
    v_unsigned_int: 6
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "2", "name": "watermelon_replace", "third": 3.0, "amount": null, "fourth": "four"}
{"_id": "3", "amount": [null, "Update_ArrayAppend"]}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
{"_id": "6", "third": ["two.1", "two"]}
0 rows affected
Delete with == operator and placeholder
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_SINT
    v_signed_int: 2
  }
}


1 rows affected
RUN select doc FROM xtest.mycoll
doc
{"_id": "1", "name": "watermelon_replace", "amount": null}
{"_id": "3", "amount": [null, "Update_ArrayAppend"]}
{"_id": "4", "name": "watermelon_replace", "amount": null}
{"_id": "5", "name": "watermelon_replace", "amount": null}
{"_id": "6", "third": ["two.1", "two"]}
0 rows affected
Delete with != operator and placeholder
send Mysqlx.Crud.Delete {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 0
      }
    }
  }
  args {
    type: V_STRING
    v_string {
      value: "Update_<="
    }
  }
}


3 rows affected
RUN select doc FROM xtest.mycoll
doc
{"_id": "3", "amount": [null, "Update_ArrayAppend"]}
{"_id": "6", "third": ["two.1", "two"]}
0 rows affected
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 1
      }
    }
  }
  args {
    type: V_OCTETS
    v_octets {
      value: "Jon"
    }
  }
}

Got expected error: Invalid value of placeholder (code 5154)
send Mysqlx.Crud.Find {
  collection {
    name: "mycoll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">"
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: PLACEHOLDER
        position: 1
      }
    }
  }
}

Got expected error: Invalid value of placeholder (code 5154)
================================================================================
CLEAN UP
================================================================================
RUN drop schema if exists xtest

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
