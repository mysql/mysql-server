#
## Expectation block
## open([+docid_generated]) / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

#
## Expectation block
## open([+docid_generated]) / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

#
## Expectation block
## open([+docid_generated, +docid_generated]) / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

#
## Expectation block
## open([+docid_generated]) / query():NOK / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

RUN select `invalid_column_name`;
Got expected error: Unknown column 'invalid_column_name' in 'field list' (code 1054)
RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

#
## Expectation block
## open([+docid_generated,  +no_error]):OK/
##  query():OK / query(error):NOK / query():NOK
## close():NOK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
  cond {
    condition_key: 1
  }
}

RUN recvok
Got expected error (code 0)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
RUN select `invalid_column_name`;
Got expected error: Unknown column 'invalid_column_name' in 'field list' (code 1054)
RUN select "No" as 'Is_ok';
Got expected error: Expectation failed: no_error (code 5159)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: no_error (code 5159)

#
## Nested expectation block
## open([+docid_generated]):OK/
##  open([+no_error]):OK/
##   query():OK / query(error):NOK / query():NOK
##  close():NOK
##  query():OK
## close():OK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
Got expected error (code 0)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
RUN select `invalid_column_name`;
Got expected error: Unknown column 'invalid_column_name' in 'field list' (code 1054)
RUN select "No" as 'Is_ok';
Got expected error: Expectation failed: no_error (code 5159)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: no_error (code 5159)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

#
## Nested expectation block
## open([+no_error]):OK/
##  open([+docid_generated]):OK/
##   query(error):NOK
##  close():NOK
##  query():NOK
## close():NOK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
Got expected error (code 0)

send Mysqlx.Expect.Open {
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

RUN select `invalid_column_name`;
Got expected error: Unknown column 'invalid_column_name' in 'field list' (code 1054)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: no_error (code 5159)

RUN select "No" as 'Is_ok';
Got expected error: Expectation failed: no_error (code 5159)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: no_error (code 5159)

#
## Nested expectation block
## open([+no_error]):OK/
##  open([+docid_generated], new_context):OK/
##   query():NOK
##  close():OK
##  query():OK
## close():OK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
Got expected error (code 0)

send Mysqlx.Expect.Open {
  op: EXPECT_CTX_EMPTY
  cond {
    condition_key: 3
  }
}

RUN recvok
Got expected error (code 0)

RUN select `invalid_column_name`;
Got expected error: Unknown column 'invalid_column_name' in 'field list' (code 1054)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

RUN select "Yes" as 'Is_ok';
Is_ok
Yes
0 rows affected
Got expected error (code 0)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error (code 0)

Mysqlx.Ok {
  msg: "bye!"
}
ok
