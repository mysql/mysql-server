
## Test shows that user can DISABLE/ENABLE ServerHello notice
## using 'mysqlx_enable_hello_notice' variable:
#
# 1. Verify raw connection to X Plugin with ServerHello notice on
# 2. Verify raw connection to X Plugin with ServerHello notice off
# 3. Verify SSL connection to X Plugin with ServerHello notice on
# 4. Verify SSL connection to X Plugin with ServerHello notice off
# 5. Unprivileged user fails to change ServerHello notice configuration;
#
CREATE USER 'some_user' @'localhost' IDENTIFIED WITH 'mysql_native_password';

#
# 1. Raw connection to X Plugin with ServerHello notice on
RUN SET GLOBAL mysqlx_enable_hello_notice= TRUE

0 rows affected
connecting...
active session is now 'attempt1'
Mysqlx.Notice.Frame {
  type: 5
  payload: ""
}

Login OK

#
# 2. Raw connection to X Plugin with ServerHello notice off
RUN SET GLOBAL mysqlx_enable_hello_notice= FALSE

0 rows affected
connecting...
active session is now 'attempt2'
Login OK

#
# 3. SSL connection to X Plugin with ServerHello notice on
RUN SET GLOBAL mysqlx_enable_hello_notice= TRUE

0 rows affected
connecting...
active session is now 'attempt3'
Mysqlx.Notice.Frame {
  type: 5
  payload: ""
}

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

Login OK

#
# 4. SSL connection to X Plugin with ServerHello notice off
RUN SET GLOBAL mysqlx_enable_hello_notice= FALSE

0 rows affected
connecting...
active session is now 'attempt4'
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

Login OK

#
# 5. Unprivileged user tries to enable ServerHello notice
RUN SET GLOBAL mysqlx_enable_hello_notice= FALSE

0 rows affected
connecting...
active session is now 'attempt5'
RUN SET GLOBAL mysqlx_enable_hello_notice= TRUE
While executing SET GLOBAL mysqlx_enable_hello_notice= TRUE:
Got expected error: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation (code 1227)
closing session attempt5
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_enable_hello_notice= DEFAULT;
DROP USER 'some_user' @'localhost';
