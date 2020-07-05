

## I. Verify CapabilitySet
#
# 1. Check that server allows only valid algorithm names, other
#    are rejected
# 2. Check that when compression is enabled on server side,
#    and client didn't enabled compression then an error
#    is returned by client when it received compressed message
# 3. Check if server verifies the "compression" object in CapabilitySet
#    a. Send compression capability without any configuration
#    b. Send compression capability with invalid type (not object)
#    c. Send algorithm capability with invalid type (not scalar)
#    d. Send server_max_combine_messages capability with invalid type (not scalar)
#    e. Send server_combine_mixed_messages capability with invalid type (not scalar)
#    f. Send level capability with invalid type (not integer)

#
# I.1
##########################################################
## Scenario: algorithm=deflate_stream
Mysqlx.Ok {
}

##########################################################
## Scenario: algorithm=lz4_message
Mysqlx.Ok {
}

##########################################################
## Scenario: algorithm=zstd_stream
Mysqlx.Ok {
}

##########################################################
## Scenario: algorithm=wrong
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

##########################################################
## Scenario: wrong=deflate_stream
Mysqlx.Error {
  severity: ERROR
  code: 5178
  msg: "Invalid or unsupported option \'compression.wrong\'"
  sql_state: "HY000"
}

ok

#
# I.2
Mysqlx.Ok {
}

Login OK
While executing SELECT 1:
Got expected error: Compression is disabled or required compression style was not selected (code 2512)
ok

#
# I.3

#
# I.3.a
Mysqlx.Error {
  severity: ERROR
  code: 5179
  msg: "The algorithm is required for \'compression\'"
  sql_state: "HY000"
}


#
# I.3.b
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'compression\'"
  sql_state: "HY000"
}


#
# I.3.c
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'compression\'"
  sql_state: "HY000"
}


#
# I.3.d
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'compression\'"
  sql_state: "HY000"
}


#
# I.3.e
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'compression\'"
  sql_state: "HY000"
}


#
# I.3.f
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'compression\'"
  sql_state: "HY000"
}

ok
