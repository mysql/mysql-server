
--write_file $MYSQL_TMP_DIR/resultset.xpl
-->import assert_messages.macro
-->import assert_notices.macro
-->import crud_insert.macro

-->echo
-->echo
-->echo ## Test setup

-->macro Insert_doc	%COLL%	%DOC%
callmacro Insert_one
	DOCUMENT	%COLL%
	type: LITERAL literal{type: V_OCTETS v_octets{value:%DOC% content_type:2}}
	ER_SUCCESS;
-->endmacro

DROP TABLE IF EXISTS xtest.coll;

stmtadmin create_collection	{"schema":"xtest","name":"coll"};
recvresult;
callmacro Insert_doc	coll	'{"_id": "1", "name":"first", "prio":1}';
callmacro Insert_doc	coll	'{"_id": "2", "name":"second", "prio":2}';
callmacro Insert_doc	coll	'{"_id": "3", "name":"third", "prio":1}';
callmacro Insert_doc	coll	'{"_id": "5", "name":"fifth", "prio":4}';


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
-->echo # 3. Assert resultset for CRUD-create message (doc with id)
-->echo # 4. Assert resultset for CRUD-create message (doc without id)
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
    name: "coll"
  }
  data_model: DOCUMENT
  criteria {
    type: OPERATOR
    operator {
      name: "=="
      param {
        type: IDENT
        identifier {
          document_path {
            type: MEMBER
            value: "name"
          }
        }
      }
      param {
        type: LITERAL
        literal {
          type: V_OCTETS
          v_octets {
            value: "fifth"
          }
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
    name: "coll"
  }
  data_model: DOCUMENT
  operation {
    source {
      document_path {
        type: MEMBER
        value: "prio"
      }
    }
    operation: ITEM_SET
    value {
      type: OPERATOR
      operator {
        name: "-"
        param {
          type: IDENT
          identifier {
            document_path {
              type: MEMBER
              value: "prio"
            }
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
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:3
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.3
Mysqlx.Crud.Insert {
  collection {
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"_id\": \"6\", \"name\":\"sixth\", \"prio\":3}"
          content_type: 2
        }
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
    name: "coll"
    schema: "xtest"
  }
  data_model: DOCUMENT
  row {
    field {
      type: LITERAL
      literal {
        type: V_OCTETS
        v_octets {
          value: "{\"name\":\"sixth\", \"prio\":3}"
          content_type: 2
        }
      }
    }
  }
  upsert: false
}
-->callmacro Assert_notice_session_state	ROWS_AFFECTED	v_unsigned_int:1
-->callmacro Assert_notice_session_state	GENERATED_DOCUMENT_IDS	type: V_OCTETS
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

-->echo
-->echo #
-->echo # I.5
Mysqlx.Crud.Find {
  collection {
    name: "coll"
  }
  data_model: DOCUMENT
  projection {
    alias:"name"
    source {
      type: IDENT
      identifier {
        document_path {
          type: MEMBER
          value: "name"
        }
      }
    }
  }
}
-->callmacro Assert_metadata	BYTES	"doc"
-->callmacro Assert_rows	5
-->callmacro Assert_message	Mysqlx.Resultset.FetchDone
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk


-->echo
-->echo #
-->echo # II.1
Mysqlx.Crud.Find {
  collection {
    name: "coll"
  }
  data_model: DOCUMENT
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
            document_path {
              type: MEMBER
              value: "prio"
            }
          }
        }
      }
    }
  }
}
-->callmacro Assert_metadata	BYTES	"doc"
-->callmacro Assert_rows	5
-->callmacro Assert_message	Mysqlx.Resultset.FetchDone
## ER_DIVISION_BY_ZERO == 1365
-->callmacro Assert_notice_warning	WARNING	1365
-->callmacro Assert_notice_warning	WARNING	1365
-->callmacro Assert_message Mysqlx.Sql.StmtExecuteOk

EOF


CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;
