CREATE USER xtest_client@localhost;
GRANT ALL ON *.* TO xtest_client@localhost;


## I. Check the session_connect_attrs capability with Session.Reset with keep_open=false.
#
# 1. Set session_connect_attrs capability, then reset session and verify that the connection attributes are not present in the PFS.
# 2. Set session_connect_attrs capability, then reset session and set new session_connect_attrs, after that verify that only the later connection attributes are present in the PFS.

## II. Check Session.Reset with different capabilities.
#
# 1. Set session_connect_attrs capability, then reset session, after that try to set capability of a type different than session_connect_attrs and verify that it fails.
# 2. Set session_connect_attrs capability, then reset session, after that try to set session_connect_attrs and other valid capability and verify that it fails and no connection attributes are added to the PFS.
# 3. Set session_connect_attrs capability, then reset session, after that try to set capability of a type different than session_connect_attrs and verify that it fails. After that send a session_connect_attrs cabapibility and verify if it succeeded.


## III. Check the session_connect_attrs capability with Session.Reset with keep_open=true.
#
# 1. Set session_connect_attr capability, then reset session with keep_open=true, verify that session connection attributes are still visible in the PFS.

#
# I.1
connecting...
active session is now 'test_session.I.1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
Login OK
[Verify that session_connect_attrs does not contain the following pair ("custom_test_key","someValue")]
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
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","nextValue")]
[Verify that session_connect_attrs does not contain the following pair ("custom_test_key","someValue")]
closing session test_session.I.2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.1
connecting...
active session is now 'test_session.II.1'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5009
  msg: "Only session_connect_attr capability is allowed after Session.Reset"
  sql_state: "HY000"
}


#
# II.2
connecting...
active session is now 'test_session.II.2'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5009
  msg: "Only session_connect_attr capability is allowed after Session.Reset"
  sql_state: "HY000"
}

Login OK
[Verify that session_connect_attrs does not contain the following pair ("custom_test_key","someValue")]
[Verify that session_connect_attrs does not contain the following pair ("custom_test_key","nextValue")]
closing session test_session.II.2
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

#
# II.3
connecting...
active session is now 'test_session.II.3'
RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5009
  msg: "Only session_connect_attr capability is allowed after Session.Reset"
  sql_state: "HY000"
}

RUN recvok
Login OK
[Verify that session_connect_attrs contains the following pair ("custom_test_key","nextValue")]
closing session test_session.II.3
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
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
RUN recvok
[Verify that session_connect_attrs contains the following pair ("custom_test_key","someValue")]
closing session test_session.III.1
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP USER xtest_client@localhost;
