=============================================
CRUD UPDATE ITEM_MERGE SCENARIOS
=============================================

================================================================================
PREAMBLE
================================================================================
create user updateitemmerge@localhost identified by 'updateitemmerge';
grant all on *.* to updateitemmerge@localhost;
CREATE SCHEMA mysqlxcoll;
================================================================================
TEST START
================================================================================

command ok
RUN SET SQL_SAFE_UPDATES = 0

0 rows affected
RUN USE mysqlxcoll

0 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "1", "name": ["Victor"], "last_name": ["Otero"],"amount": [4.99]}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "2", "name": ["Gonzalo"], "last_name": ["Chrystens"],"amount": [120.57]}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "3", "name": ["Abraham"], "last_name": ["Vega"],"amount": [74.56]}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "4", "name": ["Jennifer"], "last_name": ["Leon"],"amount": [387.14]}')

1 rows affected
RUN INSERT INTO maincoll (doc) values ('{"_id": "5", "name": ["Jhonny"], "last_name": ["Test"],"amount": [125.45]}')

1 rows affected
Update merge String value into Array using == Operator for criteria
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"name\":\"Vic\", \"last_name\":\"Ote\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic"], "amount": [4.99], "last_name": ["Otero", "Ote"]}
{"_id": "2", "name": ["Gonzalo"], "amount": [120.57], "last_name": ["Chrystens"]}
{"_id": "3", "name": ["Abraham"], "amount": [74.56], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer"], "amount": [387.14], "last_name": ["Leon"]}
{"_id": "5", "name": ["Jhonny"], "amount": [125.45], "last_name": ["Test"]}
0 rows affected
Update with == operator and Quotes
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"reference\":\"from Mexico\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic"], "amount": [4.99], "last_name": ["Otero", "Ote"], "reference": "from Mexico"}
{"_id": "2", "name": ["Gonzalo"], "amount": [120.57], "last_name": ["Chrystens"]}
{"_id": "3", "name": ["Abraham"], "amount": [74.56], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer"], "amount": [387.14], "last_name": ["Leon"]}
{"_id": "5", "name": ["Jhonny"], "amount": [125.45], "last_name": ["Test"]}
0 rows affected
Update with > operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"name\":\"> with id 3\",\"applied_operator\":\"> with id 3\"}"
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic"], "amount": [4.99], "last_name": ["Otero", "Ote"], "reference": "from Mexico"}
{"_id": "2", "name": ["Gonzalo"], "amount": [120.57], "last_name": ["Chrystens"]}
{"_id": "3", "name": ["Abraham"], "amount": [74.56], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer", "> with id 3"], "amount": [387.14], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3"], "amount": [125.45], "last_name": ["Test"], "applied_operator": "> with id 3"}
0 rows affected
Update with < operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
            value: "2"
          }
        }
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
        type: V_STRING
        v_string {
          value: "{\"name\":\"< with id 2\",\"applied_operator\":\"< with id 2\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2"], "amount": [4.99], "last_name": ["Otero", "Ote"], "reference": "from Mexico", "applied_operator": "< with id 2"}
{"_id": "2", "name": ["Gonzalo"], "amount": [120.57], "last_name": ["Chrystens"]}
{"_id": "3", "name": ["Abraham"], "amount": [74.56], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer", "> with id 3"], "amount": [387.14], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3"], "amount": [125.45], "last_name": ["Test"], "applied_operator": "> with id 3"}
0 rows affected
Update with <= operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
            value: "2"
          }
        }
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
        type: V_STRING
        v_string {
          value: "{\"name\":\"<= with id 2\",\"applied_operator\":\"<= with id 2\"}"
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99], "last_name": ["Otero", "Ote"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2"], "amount": [120.57], "last_name": ["Chrystens"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham"], "amount": [74.56], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer", "> with id 3"], "amount": [387.14], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3"], "amount": [125.45], "last_name": ["Test"], "applied_operator": "> with id 3"}
0 rows affected
Update with >= operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
            value: "3"
          }
        }
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
          value: "{\"amount\":256.57, \"name\": \">= with id 3\"}"
        }
      }
    }
  }
}


3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99], "last_name": ["Otero", "Ote"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2"], "amount": [120.57], "last_name": ["Chrystens"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57], "last_name": ["Vega"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57], "last_name": ["Test"], "applied_operator": "> with id 3"}
0 rows affected
Update with != operator for multiple docs
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
            value: "4"
          }
        }
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
          value: "{\"amount\":24.8752643, \"last_name\": \"!= 4\"}"
        }
      }
    }
  }
}


4 rows affected
Rows matched: 4  Changed: 4  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99, 24.8752643], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2"], "amount": [120.57, 24.8752643], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update item_merge a NULL
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"name\": null}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99, 24.8752643], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update item_merge a String into a only numbers member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": \"String\"}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99, 24.8752643], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String"], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update with order clause update last _id in desc order (_id = 5)
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 1234.567}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99, 24.8752643], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String"], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update with order clause update first two _id in asc order (_id = 1,2)
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
  }
  data_model: TABLE
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 9876.543}"
        }
      }
    }
  }
}


2 rows affected
Rows matched: 2  Changed: 2  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2"], "amount": [4.99, 24.8752643, 9876.543], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update item_merge a number into a only String member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"name\": 199896}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2", 199896], "amount": [4.99, 24.8752643, 9876.543], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
RUN START TRANSACTION

0 rows affected
Update transaction rollback
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 452.253}"
        }
      }
    }
  }
}


1 rows affected
Rows matched: 1  Changed: 1  Warnings: 0
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2", 199896], "amount": [4.99, 24.8752643, 9876.543, 452.253], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
RUN ROLLBACK

0 rows affected
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2", 199896], "amount": [4.99, 24.8752643, 9876.543], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update transaction commit
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 452.253}"
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
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2", 199896], "amount": [4.99, 24.8752643, 9876.543, 452.253], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update with non existing doc member
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "iamnotadoc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 987.24}"
        }
      }
    }
  }
}

Got expected error: Unknown column 'iamnotadoc' in 'field list' (code 1054)
RUN select doc from maincoll
doc
{"_id": "1", "name": ["Victor", "Vic", "< with id 2", "<= with id 2", 199896], "amount": [4.99, 24.8752643, 9876.543, 452.253], "last_name": ["Otero", "Ote", "!= 4"], "reference": "from Mexico", "applied_operator": ["< with id 2", "<= with id 2"]}
{"_id": "2", "name": ["Gonzalo", "<= with id 2", null], "amount": [120.57, 24.8752643, "String", 9876.543], "last_name": ["Chrystens", "!= 4"], "applied_operator": "<= with id 2"}
{"_id": "3", "name": ["Abraham", ">= with id 3"], "amount": [74.56, 256.57, 24.8752643], "last_name": ["Vega", "!= 4"]}
{"_id": "4", "name": ["Jennifer", "> with id 3", ">= with id 3"], "amount": [387.14, 256.57], "last_name": ["Leon"], "applied_operator": "> with id 3"}
{"_id": "5", "name": ["Jhonny", "> with id 3", ">= with id 3"], "amount": [125.45, 256.57, 24.8752643, 1234.567], "last_name": ["Test", "!= 4"], "applied_operator": "> with id 3"}
0 rows affected
Update with item_merge with } missing at the end
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "{\"reference\":\"from Mexico\""
        }
      }
    }
  }
}

Got expected error: Invalid JSON text in argument 2 to function json_merge_preserve: "Missing a comma or '}' after an object member." at position 26. (code 3141)
Update with item_merge with } missing at the start
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "\"reference\":\"from Mexico\"}"
        }
      }
    }
  }
}

Got expected error: Invalid JSON text in argument 2 to function json_merge_preserve: "The document root must not be followed by other values." at position 11. (code 3141)
Update with missing comma
send Mysqlx.Crud.Update {
  collection {
    name: "maincoll"
    schema: "mysqlxcoll"
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
      name: "doc"
    }
    operation: ITEM_MERGE
    value {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"amount\": 987.24 \"name\": \"Name\"}"
        }
      }
    }
  }
}

Got expected error: Invalid JSON text in argument 2 to function json_merge_preserve: "Missing a comma or '}' after an object member." at position 18. (code 3141)
================================================================================
CLEAN UP
================================================================================
RUN DROP USER updateitemmerge@localhost

0 rows affected
RUN SET SQL_SAFE_UPDATES = 1

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA if EXISTS mysqlxcoll;
