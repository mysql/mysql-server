

## I. Validate multiple compression-configuration changes
#
# 1. Set first lz4, then deflate, and verify that deflate is the final algorithm that server uses
# 2. Set first lz4, then deflate, and verify that deflate is the final algorithm that client uses
# 3. Set first server_combine_mixed_msg to true, then set it to false, and verify that server places
#    only concrete message types into the compressed message
# 4. Set first server_combine_limit to 10, then set it to 3, and verify that server compresses max
#    three X messages per compression message
# 5. Set first server_combine_limit to 1, then set server_combine_mixed_msg to true, and verify that
#    server uses second mixed_msg and sends single compressed message which contains whole resultset

## II. Verify that compression is not used before authentication
#
# 1. Enable compression, send compressed frame before authentication
# 2. Enable compression, check that server didn't send any compressed frames before authentication


#
# I.1
connecting...
active session is now 's1.1'
##########################################################
## Scenario: algorithm=lz4_message
##########################################################
## Scenario: algorithm=deflate_stream
Login OK
RUN SELECT 1
1
1
0 rows affected
# Asserting [ Mysqlx_bytes_sent_compressed_payload	>	0 ]
Verify ["SELECT VARIABLE_VALUE > 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_sent_compressed_payload'" returns in first column, following value 1]
# Asserting [ Mysqlx_bytes_received_compressed_payload	=	0 ]
Verify ["SELECT VARIABLE_VALUE = 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]

#
# I.2
connecting...
active session is now 's1.2'
##########################################################
## Scenario: algorithm=lz4_message
##########################################################
## Scenario: algorithm=deflate_stream
Login OK
1
1
0 rows affected
# Asserting [ Mysqlx_bytes_received_compressed_payload	>	0 ]
Verify ["SELECT VARIABLE_VALUE > 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]

#
# I.3
connecting...
active session is now 's1.3'
##########################################################
## Scenario: algorithm=lz4_message, server_combine_mixed_messages=1
##########################################################
## Scenario: algorithm=lz4_message, server_combine_mixed_messages=0
Login OK
RUN SELECT 1,2,3
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
# Asserting [ Mysqlx_bytes_sent_compressed_payload	>	0 ]
Verify ["SELECT VARIABLE_VALUE > 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_sent_compressed_payload'" returns in first column, following value 1]
# Asserting [ Mysqlx_bytes_received_compressed_payload	=	0 ]
Verify ["SELECT VARIABLE_VALUE = 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]
1
1
0 rows affected

#
# I.4
connecting...
active session is now 's1.4'
##########################################################
## Scenario: algorithm=lz4_message, server_max_combine_messages=10
##########################################################
## Scenario: algorithm=lz4_message, server_max_combine_messages=3
Login OK
RUN SELECT 1,2,3
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
1
1
0 rows affected
# Asserting [ Mysqlx_bytes_received_compressed_payload	>	0 ]
Verify ["SELECT VARIABLE_VALUE > 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]

#
# I.5
connecting...
active session is now 's1.5'
##########################################################
## Scenario: algorithm=lz4_message, server_max_combine_messages=1
##########################################################
## Scenario: algorithm=lz4_message, server_combine_mixed_messages=1
Login OK
RUN SELECT 1,2,3
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
1
1
0 rows affected
# Asserting [ Mysqlx_bytes_received_compressed_payload	>	0 ]
Verify ["SELECT VARIABLE_VALUE > 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]

#
# II.1
connecting...
active session is now 's2.1'
##########################################################
## Scenario: algorithm=lz4_message
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5170
  msg: "Client didn\'t enable the compression."
  sql_state: "HY000"
}

closing session s2.1
switched to session default

#
# II.2
connecting...
active session is now 's2.2'
##########################################################
## Scenario: algorithm=lz4_message
Login OK
# Asserting [ Mysqlx_bytes_sent_compressed_payload	=	0 ]
Verify ["SELECT VARIABLE_VALUE = 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_sent_compressed_payload'" returns in first column, following value 1]
# Asserting [ Mysqlx_bytes_received_compressed_payload	=	0 ]
Verify ["SELECT VARIABLE_VALUE = 0 FROM performance_schema.session_status WHERE VARIABLE_NAME LIKE 'Mysqlx_bytes_received_compressed_payload'" returns in first column, following value 1]
closing session s2.2
Mysqlx.Ok {
  msg: "bye!"
}
ok
