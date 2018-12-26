Wait for the X Plugin before restarting the server
Mysqlx.Ok {
  msg: "bye!"
}
ok
Test different connection types while secure transport when the server was started with this flag set to ON
# restart: --require_secure_transport=ON
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "\000x_root\000"
}

Mysqlx.Session.AuthenticateOk {
  auth_data: ""
}

ok
send Mysqlx.Connection.CapabilitiesSet {
  capabilities {
    capabilities {
      name: "tls"
      value {
        type: SCALAR
        scalar {
          type: V_BOOL
          v_bool: true
        }
      }
    }
  }
}

Mysqlx.Ok {
}

send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "\000x_root\000"
}

Mysqlx.Session.AuthenticateOk {
  auth_data: ""
}

ok
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "\000x_root\000"
}

Mysqlx.Error {
  severity: FATAL
  code: 1251
  msg: "Invalid authentication method PLAIN"
  sql_state: "HY000"
}

not ok
Application terminated with expected error: Connections using insecure transport are prohibited while --require_secure_transport=ON. (code 3159)
ok
SET GLOBAL require_secure_transport=OFF;
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "\000x_root\000"
}

Mysqlx.Error {
  severity: FATAL
  code: 1251
  msg: "Invalid authentication method PLAIN"
  sql_state: "HY000"
}

not ok
send Mysqlx.Session.AuthenticateStart {
  mech_name: "PLAIN"
  auth_data: "\000x_root\000"
}

Mysqlx.Session.AuthenticateOk {
  auth_data: ""
}

ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
