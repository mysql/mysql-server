
--write_file $MYSQL_TMP_DIR/resultset.xpl
-->import assert_messages.macro
-->import assert_notices.macro
-->import crud_insert.macro

-->echo
-->echo
-->echo ## Test setup

##
##  Execution of SQL through X Protocol can generate flow
##  described by following UML flow:
##
##   ...
##   loop has more resultsets or not at end of fetch
##     group resultset
##       loop has more columns
##         server --> client: ColumnMetaData
##       end
##       loop has more rows
##         server --> client: Row
##       end
##     end
##     alt has more resultsets
##       server --> client: FetchDoneMoreResultsets
##     end
##   end
##   loop has more OUT-paramsets or not at end of fetch
##     server --> client: FetchDoneMoreOutParams
##     group resultset
##       loop has more columns
##         server --> client: ColumnMetaData
##       end
##       loop has more rows
##         server --> client: Row
##       end
##     end
##   end
##   alt at end of all resultsets
##     server --> client: FetchDone
##   else cursor is opened
##     server --> client: FetchSuspended
##   end
##   ...
##
## Following suite is going to ensure that all CRUD
## executions work according to this flow.
##

## Test case description:
-->echo
-->echo
-->echo ## I. Validate simple resultsets for CRUD messages
-->echo #
-->echo # 1. Assert resultset for CRUD-delete message
-->echo # 2. Assert resultset for CRUD-update message
-->echo # 3. Assert resultset for CRUD-create message (table with auto_inc column)
-->echo # 4. Assert resultset for CRUD-create message (table without auto_inc column)
-->echo # 5. Assert resultset for CRUD-read message
-->echo
-->echo ## II. Validate simple resultset with warnings
-->echo #
-->echo # 1. Assert one resultsets for select queries

-->echo
-->echo #
-->echo # I.1
Mysqlx.Crud.Delete {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          name: "prio"
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_SINT
          v_signed_int: 3
        }
      }
    }
  }
}
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.2
Mysqlx.Crud.Update {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  operation {
    source {
      name: "prio"
    }
    operation: SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
      }
    }
  }
}
-->callmacro Assert_notice_session_state	PRODUCED_MESSAGE	type: V_STRING
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:4
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.3
Mysqlx.Crud.Insert {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "sixth"
        }
      }
    }
    field {
      type: LITERAL
      literal {
        type: V_SINT
        v_signed_int: 3
      }
    }
  }
  upsert: false
}
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.4
Mysqlx.Crud.Insert {
  collection {
    name: "xtable_with_pk"
  }
  
  projection {
    name: "description"
  }
  
  data_model: TABLE
  row {
    field {
      type: LITERAL
      literal {
        type: V_STRING
        v_string {
          value: "test"
        }
      }
    }
  }
  upsert: false
}
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_notice_session_state	GENERATED_INSERT_ID	v_unsigned_int: 2
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.5
Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    alias:"prio"
    source {
      type: IDENT
      identifier {
        name: "prio"
      }
    }
  }
}
-->callmacro Assert_metadata	SINT	"prio"
-->callmacro Assert_rows	5
-->callmacro Assert_message	Mysqlx.Resultset.FetchDone
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk


-->echo
-->echo #
-->echo # II.1
Mysqlx.Crud.Find {
  collection {
    name: "xtable"
  }
  data_model: TABLE
  projection {
    alias:"name"
    source {
      type: OPERATOR
      operator {
        name: "/"
        param {
          type: LITERAL
          literal {
            type: V_SINT
            v_signed_int: 1
          }
        }
        param {
          type: IDENT
          identifier {
            name: "prio"
          }
        }
      }
    }
  }
}
-->callmacro Assert_metadata	DECIMAL	"name"
-->callmacro Assert_rows	5
-->callmacro Assert_message	Mysqlx.Resultset.FetchDone
## ER_DIVISION_BY_ZERO == 1365
-->callmacro Assert_notice_warning	WARNING	1365
-->callmacro Assert_notice_warning	WARNING	1365
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

EOF

source sample_tables.inc;
