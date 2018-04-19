#
## Expectation block
## open([+field_exists=1]) / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1"
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
## open([+field_exists=1]) / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1"
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
## open([+field_exists=1, +field_exists=2]) / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1"
  }
  cond {
    condition_key: 2
    condition_value: "2"
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
## open([+field_exists=1, +field_exists=unknown]) / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1"
  }
  cond {
    condition_key: 2
    condition_value: "1.1"
  }
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

RUN select "Yes" as 'Is_ok';
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

#
## Expectation block
## open([+field_exists=1]) / query():NOK / query() / close()
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1"
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
## open([+field_exists=unknown]):NOK / query():NOK / close():NOK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1.1"
  }
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

RUN select "No" as 'Is_ok';
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

#
## Expectation block 
## open([+field_exists=unknown, +no_error]):NOK / query():NOK / close():NOK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "1.1"
  }
  cond {
    condition_key: 1
  }
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

RUN select "No" as 'Is_ok';
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)
send Mysqlx.Expect.Close {
}

RUN recvok
Got expected error: Expectation failed: field_exists = '1.1' (code 5168)

#
## Expectation block
## open([+field_exists=2,  +no_error]):OK/
##  query():OK / query(error):NOK / query():NOK
## close():NOK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "2"
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
## open([+field_exists=2]):OK/
##  open([+no_error]):OK/
##   query():OK / query(error):NOK / query():NOK
##  close():NOK
##  query():OK
## close():OK
#
send Mysqlx.Expect.Open {
  cond {
    condition_key: 2
    condition_value: "2"
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
##  open([+field_exists=2]):OK/
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
    condition_key: 2
    condition_value: "2"
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
##  open([+field_exists=2], new_context):OK/
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
    condition_key: 2
    condition_value: "2"
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
