## I. Validate the fix for the bug. The issue occures between two
##    session, where one shows X Plugins status variables, second
##    triggers reauthentication
#
# 1. Create classic session (S2), show any X Plugins status variable
# 2. Create X Protocol session (S1), which triggers reauthentication
# 3. Synchronize S1 and S2, in a way that triggers the issue
#

RUN SET DEBUG_SYNC= 'syncpoint_create_session_locked SIGNAL reached_create_session_locked WAIT_FOR continue_create_session_locked'

0 rows affected

#
# I.1

#
# I.2
send Mysqlx.Session.Close {
}


#
# I.3
Mysqlx.Ok {
}

ok
