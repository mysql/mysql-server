CREATE USER xtest_client1@localhost;
CREATE USER xtest_client2@localhost;
GRANT ALL ON *.* TO xtest_client1@localhost;
GRANT ALL ON *.* TO xtest_client2@localhost;


## I. Check valid uses of the session_connect_attrs capability only.
#
# 1. Set the session_connect_attrs capability once and verify it in the PFS.
# 2. Set the session_connect_attrs capability with some duplicated conneection attribute keys and verify that the duplicated keys are present in the PFS.
# 3. Set the session_connect_attrs capability twice and verify that only the last set is present in the PFS.
# 4. Set the session_connect_attrs capability with two distinct attributes and verify if they are in the PFS.

## II. Check session_connect_attrs capability with invalid contents.
#
# 1. Assert that session_connect_attrs which contains value with length exceeding 1024 character limit triggers an error.
#   a. Send just the invalid capability.
#   b. Send the invalid capability followed by a valid one.
# 2. Assert that session_connect_attrs which contains key with length exceeding 32 character limit triggers an error.
#   a. Send just the invalid capability.
#   b. Send the invalid capability followed by a valid one.
# 3. Assert that session_connect_attrs which contains an empty key triggers an error.
#   a. Send just the invalid capability.
#   b. Send the invalid capability followed by a valid one.
# 4. Assert that session_connect_attrs which contains both key and value with lengths exceeding their limits triggers an error.
#   a. Send just the invalid capability.
#   b. Send the invalid capability followed by a valid one.
# 5. Assert that session_connect_attrs which contains a message that after decoding exceeds 64k size limit triggers an error.
#   a. Send just the invalid capability.
#   b. Send the invalid capability followed by a valid one.

## III. Check session_connect_attrs capability when used with other capabilities.
#
# 1. Set the session_connect_attrs capability and other valid capability.
# 2. Set the session_connect_attrs capability and other capability, one of which is invalid. Verify that none of the capabilities are not set in the PFS.
#   a. The session_connect_attrs capability is invalid.
#   b. The additional capability is invalid.

## IV. Check session connect attributes after client disconnects.
#
# 1. Verify that new client cannot see the session connection attibutes of a previously disconnected client.
# 2. Verify that new client can see the session connection attibutes of another connected client.

## V. Check that it is not possible to send session connection attributes after the authentication.

## VI. Check that malformed session connection attributes capability triggers an error.

#
# I.1
connecting...
active session is now 'test_session.I.1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someOS")]
closing session test_session.I.1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# I.2
connecting...
active session is now 'test_session.I.2'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someOS")]
[Verify that session_connect_attrs contains the following pair ("custom_test_key","otherOS")]
closing session test_session.I.2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# I.3
connecting...
active session is now 'test_session.I.3'
RUN recvok
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","shownOS")]
closing session test_session.I.3
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# I.4
connecting...
active session is now 'test_session.I.4'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("test_key1","value1")]
[Verify that session_connect_attrs contains the following pair ("test_key2","value2")]
closing session test_session.I.4
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.1.a
connecting...
active session is now 'test_session.II.1.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5004
  msg: "Value is too long for \'custom_test_key\' attribute, currently limited to 1024"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.II.1.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.1.b
connecting...
active session is now 'test_session.II.1.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5004
  msg: "Value is too long for \'custom_test_key\' attribute, currently limited to 1024"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.II.1.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.2.a
connecting...
active session is now 'test_session.II.2.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5005
  msg: "Key name beginning with \'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\'... is too long, currently limited to 32"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.II.2.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.2.b
connecting...
active session is now 'test_session.II.2.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5005
  msg: "Key name beginning with \'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\'... is too long, currently limited to 32"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.II.2.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.3.a
connecting...
active session is now 'test_session.II.3.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5006
  msg: "Empty key name given"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.II.3.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.3.b
connecting...
active session is now 'test_session.II.3.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5006
  msg: "Empty key name given"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.II.3.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.4.a
connecting...
active session is now 'test_session.II.4.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5005
  msg: "Key name beginning with \'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\'... is too long, currently limited to 32"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.II.4.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.4.b
connecting...
active session is now 'test_session.II.4.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5005
  msg: "Key name beginning with \'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\'... is too long, currently limited to 32"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.II.4.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.5.a
connecting...
active session is now 'test_session.II.5.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5007
  msg: "There are too many bytes in connection session attributes the capability is limited to 65536"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.II.5.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.5.b
connecting...
active session is now 'test_session.II.5.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5007
  msg: "There are too many bytes in connection session attributes the capability is limited to 65536"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.II.5.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# III.1
connecting...
active session is now 'test_session.III.1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.III.1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# III.2.a
connecting...
active session is now 'test_session.III.2.a'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5004
  msg: "Value is too long for \'custom_test_key\' attribute, currently limited to 1024"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.III.2.a
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# III.2.b
connecting...
active session is now 'test_session.III.2.b'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5001
  msg: "Capability prepare failed for \'client.interactive\'"
  sql_state: "HY000"
}

Login OK
[Assert that no session connect attributes were inserted]
closing session test_session.III.2.b
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# IV.1
connecting...
active session is now 'test_session.IV.1_xtest_client_1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","client1OS")]
closing session test_session.IV.1_xtest_client_1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
connecting...
active session is now 'test_session.IV.1_xtest_client_2'
Login OK
Verify [Global status variable "Mysqlx_sessions" needs to have a value of 2]
[Assert that no session connect attributes were inserted]
closing session test_session.IV.1_xtest_client_2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# IV.2
connecting...
active session is now 'test_session.IV.2_xtest_client_1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","client_1_OS")]
connecting...
active session is now 'test_session.IV.2_xtest_client_2'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","client_1_OS")]
[Verify that session_connect_attrs contains the following pair ("custom_test_key","client_2_OS")]
closing session test_session.IV.2_xtest_client_2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# V
connecting...
active session is now 'test_session.V_xtest_client_1'
Login OK
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1047
  msg: "Unexpected message received"
  sql_state: "HY000"
}

closing session test_session.V_xtest_client_1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# VI
connecting...
active session is now 'test_session.VI_xtest_client_1'
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5008
  msg: "Key and values support only string values"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","valid_value")]
closing session test_session.VI_xtest_client_1
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP USER xtest_client1@localhost;
DROP USER xtest_client2@localhost;
