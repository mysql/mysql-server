DROP SCHEMA IF EXISTS xtest;
Warnings:
Note	1008	Can't drop database 'xtest'; database doesn't exist
CREATE SCHEMA xtest;
USE xtest;
CREATE TABLE xtable (phrase VARCHAR(30));
INSERT INTO xtable (phrase) VALUES ('Alice has a cat');
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: FUNC_CALL
      function_call {
        name {
          name: "count"
        }
        param {
          type: LITERAL
          literal {
            type: V_OCTETS
            v_octets {
              value: "*"
            }
          }
        }
      }
    }
  }
}

count('*')
1
command ok
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: FUNC_CALL
      function_call {
        name {
          name: "count"
        }
        param {
          type: OPERATOR
          operator {
            name: "*"
          }
        }
      }
    }
  }
}

count(*)
1
command ok
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "*"
        }
      }
    }
  }
}

*
*
command ok
send Mysqlx.Crud.Find {
  collection {
    name: "xtable"
    schema: "xtest"
  }
  data_model: TABLE
  projection {
    source {
      type: OPERATOR
      operator {
        name: "*"
      }
    }
  }
}

phrase
Alice has a cat
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP SCHEMA IF EXISTS xtest;
