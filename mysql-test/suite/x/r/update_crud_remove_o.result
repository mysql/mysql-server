=============================================
CRUD UPDATE ITEM_REMOVE SCENARIOS
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
        value: "name"
      }
    }
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": "Victor", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "name": "Jennifer", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "amount": 125.45, "last_name": "Test"}
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
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 2  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": "Victor", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "name": "Gonzalo", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "amount": 125.45, "last_name": "Test"}
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
    operation: ITEM_REMOVE
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "amount": 4.99, "last_name": "Otero"}
{"_id": "2", "amount": 120.57, "last_name": "Chrystens"}
{"_id": "3", "name": "Abraham", "amount": 74.56, "last_name": "Vega"}
{"_id": "4", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "amount": 125.45, "last_name": "Test"}
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
    operation: ITEM_REMOVE
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "amount": 4.99}
{"_id": "2", "amount": 120.57}
{"_id": "3", "name": "Abraham", "amount": 74.56}
{"_id": "4", "amount": 387.14, "last_name": "Leon"}
{"_id": "5", "amount": 125.45, "last_name": "Test"}
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
    operation: ITEM_REMOVE
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "amount": 4.99}
{"_id": "2", "amount": 120.57}
{"_id": "3", "name": "Abraham", "amount": 74.56}
{"_id": "4", "amount": 387.14}
{"_id": "5", "amount": 125.45}
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
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "amount": 4.99}
{"_id": "2", "amount": 120.57}
{"_id": "3", "name": "Abraham", "amount": 74.56}
{"_id": "4", "amount": 387.14}
{"_id": "5"}
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
    operation: ITEM_REMOVE
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3", "name": "Abraham", "amount": 74.56}
{"_id": "4", "amount": 387.14}
{"_id": "5"}
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
    operation: ITEM_REMOVE
  }
}


2 rows affected
Rows matched: 4  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3", "name": "Abraham"}
{"_id": "4"}
{"_id": "5"}
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
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
RUN ROLLBACK

0 rows affected
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3", "name": "Abraham"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
RUN START TRANSACTION

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
    operation: ITEM_REMOVE
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN COMMIT

0 rows affected
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
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
    operation: ITEM_REMOVE
  }
}


0 rows affected
Rows matched: 4  Changed: 0  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
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

Got expected error: Invalid type of update operation for document (code 5051)
Invalid operator for Criteria
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "not an operator"
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
    operation: ITEM_REMOVE
  }
}

Got expected error: Invalid operator not an operator (code 5150)
# Remove already removed item
RUN select doc from mysqlxcoll.maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_REMOVE
  }
}


0 rows affected
Rows matched: 5  Changed: 0  Warnings: 0
RUN select doc from mysqlxcoll.maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "amount"
      }
    }
    operation: ITEM_REMOVE
  }
}


0 rows affected
Rows matched: 5  Changed: 0  Warnings: 0
RUN select doc from mysqlxcoll.maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
# Can not remove immutable document member _id
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "_id"
      }
    }
    operation: ITEM_REMOVE
  }
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5053
  msg: "Forbidden update operation on \'$._id\' member"
  sql_state: "HY000"
}

RUN select doc from mysqlxcoll.maincoll
doc
{"_id": "1"}
{"_id": "2"}
{"_id": "3"}
{"_id": "4"}
{"_id": "5"}
0 rows affected
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
