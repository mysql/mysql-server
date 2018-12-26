call mtr.add_suppression("Maximum number of authentication attempts reached");
call mtr.add_suppression("Access denied for user .*");


#
## Execute three "PLAIN" authentications
## checking severity of returned errors (last one must be FATAL)
#
connecting...
active session is now 'seq'
Mysqlx.Ok {
}


# First authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "test\000not_existing_user\000invalid_password"
}

Mysqlx.Error(severity) = ERROR

# Second authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "test\000not_existing_user\000invalid_password"
}

Mysqlx.Error(severity) = ERROR

# Third authentication attempt (last one)
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "test\000not_existing_user\000invalid_password"
}

Mysqlx.Error(severity) = FATAL
closing session seq
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok


#
## Execute three "SHA256_MEMORY" authentications
## checking severity of returned errors (last one must be FATAL)
#
connecting...
active session is now 'seq'
Mysqlx.Ok {
}


# First authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "SHA256_MEMORY"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Second authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "SHA256_MEMORY"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Third authentication attempt (last one)
send Mysqlx.Session.AuthenticateStart {
  mech_name: "SHA256_MEMORY"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = FATAL
closing session seq
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok


#
## Execute three mixed authentications
## checking severity of returned errors (last one must be FATAL)
#
connecting...
active session is now 'seq'
Mysqlx.Ok {
}


# First authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "MYSQL41"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Second authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "SHA256_MEMORY"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Third authentication attempt (last one)
send Mysqlx.Session.AuthenticateStart {
  mech_name: "MYSQL41"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = FATAL
closing session seq
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok


#
## Execute "SHA256_MEMORY", "MYSQL41", "PLAIN"
## checking severity of returned errors (last one must be FATAL)
#
connecting...
active session is now 'seq'
Mysqlx.Ok {
}


# First authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "SHA256_MEMORY"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Second authentication attempt
send Mysqlx.Session.AuthenticateStart {
  mech_name: "MYSQL41"
}

Mysqlx.Session.AuthenticateContinue {...}
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "test\000not_existing_user\000invalid_scramble"
}

Mysqlx.Error(severity) = ERROR

# Third authentication attempt (last one)
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "test\000not_existing_user\000invalid_password"
}

Mysqlx.Error(severity) = FATAL
closing session seq
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok


#
## Verify that AuthenticateContinue message instead AuthStart
## will trigger FATAL error and disconnection
#
send Mysqlx.Session.AuthenticateContinue {
  auth_data: "not meaningful field payload"
}

Mysqlx.Error(severity) = FATAL
ok


#
## Verify that StmtExecute message instead AuthStart
## will trigger FATAL error and disconnection
#
send Mysqlx.Sql.StmtExecute {
  stmt: "not meaningful field payload"
}

Mysqlx.Error(severity) = FATAL
ok
