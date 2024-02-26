=============================================
CRUD UPDATE ITEM_REPLACE SCENARIOS
=============================================

================================================================================
PREAMBLE
================================================================================
create user upditemreplace@localhost identified by 'upditemreplace';
grant all on *.* to upditemreplace@localhost;
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
RUN INSERT INTO maincoll (doc) values ('{"_id": "0"}')

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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"VictorReplaced\""
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"VictorReplaced\"", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": "Jennifer", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": "Jhonny", "amount": 125.45, "last_name": "Test"}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_>\""
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"VictorReplaced\"", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": "\"xtest_>\"", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": "\"xtest_>\"", "amount": 125.45, "last_name": "Test"}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_<\""
        }
      }
    }
  }
}


2 rows affected
Rows matched: 3  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": "\"xtest_>\"", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": "\"xtest_>\"", "amount": 125.45, "last_name": "Test"}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_<=\""
        }
      }
    }
  }
}


3 rows affected
Rows matched: 4  Changed: 3  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 4.99, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "name": "\"xtest_>\"", "amount": 125.45, "last_name": "Test"}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_>=\""
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 4.99, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 387.14, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 125.45, "last_name": "\"xtest_>=\""}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "2"
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_!=\""
        }
      }
    }
  }
}


4 rows affected
Rows matched: 5  Changed: 4  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_>=\""}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "2"
          }
        }
      }
    }
  }
  operation {
    source {
      document_path {
        type: MEMBER
        value: "notamember"
      }
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"xtest_notmemberdoc\""
        }
      }
    }
  }
}


0 rows affected
Rows matched: 5  Changed: 0  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_>=\""}
0 rows affected
unsigned int data type update for Document
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_UINT
        v_unsigned_int: 23
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 23, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 23, "last_name": "\"xtest_>=\""}
0 rows affected
signed int data type update for Document
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 25
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 25, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 25, "last_name": "\"xtest_>=\""}
0 rows affected
double data type update for Document
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 26.53
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 26.53, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 26.53, "last_name": "\"xtest_>=\""}
0 rows affected
float data type update for Document
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_FLOAT
        v_float: 27.75
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 27.75, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 27.75, "last_name": "\"xtest_>=\""}
0 rows affected
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_DOUBLE
        v_double: 1234.567
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 120.57, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 27.75, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 1234.567, "last_name": "\"xtest_>=\""}
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
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
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": 27.75, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": 1234.567, "last_name": "\"xtest_>=\""}
0 rows affected
Null value for Document
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_NULL
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
0 rows affected
RUN START TRANSACTION

0 rows affected
Update Transaction Rollback
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
        value: "last_name"
      }
    }
    operation: ITEM_REPLACE
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
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": null, "last_name": "\"Iam_rollback\""}
0 rows affected
RUN ROLLBACK

0 rows affected
RUN select doc from maincoll
doc
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
0 rows affected
RUN START TRANSACTION

0 rows affected
Update Transaction and Commit
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
          document_path {
            type: MEMBER
            value: "_id"
          }
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
        value: "last_name"
      }
    }
    operation: ITEM_REPLACE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"Iam_commit\""
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
{"_id": "0"}
{"_id": "1", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "2", "name": "\"xtest_<\"", "amount": 9876.543, "last_name": "\"xtest_<=\""}
{"_id": "3", "name": "Abraham", "amount": "\"xtest_!=\"", "last_name": "\"xtest_<=\""}
{"_id": "4", "name": "\"xtest_>\"", "amount": null, "last_name": "\"xtest_>=\""}
{"_id": "5", "name": "\"xtest_>\"", "amount": null, "last_name": "\"Iam_commit\""}
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
      name: ">"
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
    operation: SET
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "xtest"
        }
      }
    }
  }
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5051
  msg: "Invalid type of update operation for document"
  sql_state: "HY000"
}

================================================================================
CLEAN UP
================================================================================
RUN DROP SCHEMA if EXISTS mysqlxcoll

1 rows affected
RUN DROP USER upditemreplace@localhost

0 rows affected
RUN SET SQL_SAFE_UPDATES = 1

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
