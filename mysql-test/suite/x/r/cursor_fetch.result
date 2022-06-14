CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
CREATE TABLE t1 (phrase VARCHAR(30));
INSERT INTO t1 (phrase) VALUES ('Alice has a cat');
INSERT INTO t1 (phrase) VALUES ('Bob has a dog');
CREATE TABLE t2 (x INT, y FLOAT, z TINYINT);
INSERT INTO t2 VALUES (1, 1.1, 11);
INSERT INTO t2 VALUES (2, 2.22, 22);
INSERT INTO t2 VALUES (3, 3.0, 33);
send Mysqlx.Prepare.Prepare {
  stmt_id: 1
  stmt {
    type: STMT
    stmt_execute {
      stmt: "SELECT * FROM xtest.t1"
    }
  }
}

RUN recvok
send Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt {
    type: STMT
    stmt_execute {
      stmt: "SELECT * FROM xtest.t2"
    }
  }
}

RUN recvok

 Fetch all rows in Cursor.Open
==============================
send Mysqlx.Cursor.Open {
  cursor_id: 1
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 3
}

phrase
Alice has a cat
Bob has a dog
0 rows affected
send Mysqlx.Cursor.Fetch {
  cursor_id: 1
  fetch_rows: 2
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5123
  msg: "No more data in cursor (cursor id:1)"
  sql_state: "HY000"
}


 Fetch no rows in Cursor.Open
=============================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 0
}

phrase
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 3
}

phrase
Alice has a cat
Bob has a dog
0 rows affected

 Fetch part of rows in Cursor.Open
==================================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 1
}

phrase
Alice has a cat
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 2
}

phrase
Bob has a dog
0 rows affected

 Multiple cursor fetch messages
===============================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 0
}

phrase
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
Alice has a cat
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
Bob has a dog
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
0 rows affected

 Fetch more results than available in one fetch
===============================================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 0
}

phrase
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 8
}




0 rows affected

 Fetch more results than available in multiple fetches
======================================================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 0
}

phrase
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
Alice has a cat
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
Bob has a dog
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

phrase
0 rows affected
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 1
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5123
  msg: "No more data in cursor (cursor id:2)"
  sql_state: "HY000"
}


 Fetch result with multiple column metadata
===========================================
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
  fetch_rows: 0
}

x	y	z
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 3
}

x	y	z
1	1.1	11
2	2.22	22
3	3	33
command ok

 Alternating cursor fetch
=========================
send Mysqlx.Cursor.Open {
  cursor_id: 1
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 1
    }
  }
  fetch_rows: 0
}

phrase
command ok
send Mysqlx.Cursor.Open {
  cursor_id: 2
  stmt {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
  fetch_rows: 0
}

x	y	z
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 1
  fetch_rows: 1
}

phrase
Alice has a cat
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 2
}

x	y	z
1	1.1	11
2	2.22	22
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 1
  fetch_rows: 1
}

phrase
Bob has a dog
command ok
send Mysqlx.Cursor.Fetch {
  cursor_id: 2
  fetch_rows: 2
}

x	y	z
3	3	33
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
