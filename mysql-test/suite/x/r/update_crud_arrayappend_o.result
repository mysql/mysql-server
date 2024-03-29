=============================================
CRUD UPDATE ARRAY_APPEND SCENARIOS
=============================================

================================================================================
PREAMBLE
================================================================================
create user updateitemremove@localhost identified by 'updateitemremove';
grant all on *.* to updateitemremove@localhost;
CREATE SCHEMA mysqlxcoll;
================================================================================
TEST START
================================================================================

command ok
RUN SET SQL_SAFE_UPDATES = 0

0 rows affected
RUN USE mysqlxcoll

0 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "1", "name": "Victor", "last_name": "Otero","amount": 4.99}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "2", "name": "Gonzalo", "last_name": "Chrystens","amount": 120.57}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "3", "name": "Abraham", "last_name": "Vega","amount": 74.56}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "4", "name": "Jennifer", "last_name": "Leon","amount": 387.14}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "5", "name": "Jhonny", "last_name": "Test","amount": 125.45}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "6","name":null}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "7"}')

1 rows affected
Update a valid collection with == operator
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "1"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"VictorArrayAppend\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\""], "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": "Jennifer", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": "Jhonny", "amount": 125.45, "last_name": "Test"}
{"_id": "6", "name": null}
{"_id": "7"}
0 rows affected
Update a valid collection with string value and > operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">"
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "3"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"ExtraName_>\""
        }
      }
    }
  }
}


3 rows affected
Rows matched: 4  Changed: 3  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\""], "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": 125.45, "last_name": "Test"}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update a valid collection with string value and < operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "<"
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "3"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"ExtraName_<\""
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": 125.45, "last_name": "Test"}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update a valid collection with string value and <= operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "<="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "3"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "last_name"
      }
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"ExtraLastName_<=\""
        }
      }
    }
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": 120.57, "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": 125.45, "last_name": "Test"}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update a valid collection with string value and >= operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: ">="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "4"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "last_name"
      }
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"ExtraLastName_>=\""
        }
      }
    }
  }
}


2 rows affected
Rows matched: 4  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": 120.57, "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": 387.14, "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": 125.45, "last_name": ["Test", "\"ExtraLastName_>=\""]}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update a valid collection with string value and != operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "1"
          }
        }
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
        type: V_UINT
        v_unsigned_int: 10
      }
    }
  }
}


4 rows affected
Rows matched: 6  Changed: 4  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10], "last_name": ["Test", "\"ExtraLastName_>=\""]}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update append a Double
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "4"
          }
        }
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
        type: V_DOUBLE
        v_double: 156.59874665
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10], "last_name": ["Test", "\"ExtraLastName_>=\""]}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update append a Float
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "4"
          }
        }
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
        type: V_FLOAT
        v_float: 78445.5
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10], "last_name": ["Test", "\"ExtraLastName_>=\""]}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
Update append a NULL
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "4"
          }
        }
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
        type: V_NULL
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10], "last_name": ["Test", "\"ExtraLastName_>=\""]}
{"_id": "6", "name": [null, "\"ExtraName_>\""]}
{"_id": "7"}
0 rows affected
RUN delete from maincoll where _id in (6,7)

2 rows affected
Update with order clause update last _id in desc order (_id = 5)
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "0"
          }
        }
      }
    }
  }
  limit {
    row_count: 1
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
        type: V_DOUBLE
        v_double: 1234.56
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": 4.99, "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Update with order clause update first two _id in asc order (_id = 1,2)
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "0"
          }
        }
      }
    }
  }
  limit {
    row_count: 2
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
        type: V_DOUBLE
        v_double: 9876.543
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Update append a String into a only numbers member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "3"
          }
        }
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
          value: "\"notanumber\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Update append a number into a only String member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "5"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 536
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\"", 536], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
RUN START TRANSACTION

0 rows affected
Update transaction rollback
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "5"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"Iam_rollback\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\"", 536, "\"Iam_rollback\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
RUN ROLLBACK

0 rows affected
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\"", 536], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Update transaction commit
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "5"
          }
        }
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
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"Iam_rollback\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN COMMIT

0 rows affected
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\"", 536, "\"Iam_rollback\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Update with non existing doc member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "1"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "notadocmember"
      }
    }
    operation: ARRAY_APPEND
    value {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 10
      }
    }
  }
}


0 rows affected
Rows matched: 4  Changed: 0  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "\"VictorArrayAppend\"", "\"ExtraName_<\""], "amount": [4.99, 9876.543], "last_name": ["Otero", "\"ExtraLastName_<=\""]}
{"_id": "2", "name": ["Gonzalo", "\"ExtraName_<\""], "amount": [120.57, 10, 9876.543], "last_name": ["Chrystens", "\"ExtraLastName_<=\""]}
{"_id": "3", "name": "Abraham", "amount": [74.56, 10, "\"notanumber\""], "last_name": ["Vega", "\"ExtraLastName_<=\""]}
{"_id": "4", "name": ["Jennifer", "\"ExtraName_>\""], "amount": [387.14, 10, 156.59874665, 78445.5, null], "last_name": ["Leon", "\"ExtraLastName_>=\""]}
{"_id": "5", "name": ["Jhonny", "\"ExtraName_>\"", 536, "\"Iam_rollback\""], "amount": [125.45, 10, 1234.56], "last_name": ["Test", "\"ExtraLastName_>=\""]}
0 rows affected
Invalid type of update for Document
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "!="
      param {
        type: IDENT
        identifier {
          name: "_id"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "1"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "notadocmember"
      }
    }
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 10
      }
    }
  }
}

Got expected error: Invalid type of update operation for document (code 5051)
================================================================================
CLEAN UP
================================================================================
RUN DROP USER updateitemremove@localhost

0 rows affected
RUN SET SQL_SAFE_UPDATES = 1

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA if EXISTS mysqlxcoll;
