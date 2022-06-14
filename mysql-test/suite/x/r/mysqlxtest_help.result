
############################################################
## Test mysqlxtest executable help printout
##

Copyright DATE Oracle and/or its affiliates.

Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

mysqlxtest <options> [SCHEMA]
Options:
-f, --file=<file>     Reads input from file
-I, --import=<dir>    Reads macro files from dir; required by -->import
--sql=<SQL>           Use SQL as input and execute it like in -->sql block
-e=<SQL>, --execute=<SQL> Aliases for "--sql" option
-n, --no-auth         Skip authentication which is required by -->sql block (run mode)
--no-connect-attrs    Skip send session connection attributes
--plain-auth          Use PLAIN text authentication mechanism
--cached-auth         Use SHA256_MEMORY authentication mechanism
--mysql41-auth        Use MYSQL41 authentication mechanism
--using-cap-auth      Get capabilities to check which
                      authentication mechanism are supported
--mysql57-compatible  Use features that are 5.7 compatible:
                      * limit auth-mechanisms
-u, --user=<user>     Connection user
-p, --password=<pass> Connection password
-h, --host=<host>     Connection host
-P, --port=<port>     Connection port (default:33060)
--ipv=<mode>          Force internet protocol (default:4):
                      0 - allow system to resolve IPv6 and IPv4, for example
                          resolving of 'localhost' can return both '::1' and '127.0.0.1'
                      4 - allow system to resolve only IPv4, for example
                          resolving of 'localhost' is going to return '127.0.0.1'
                      6 - allow system to resolve only IPv6, for example
                          resolving of 'localhost' is going to return '::1'
-t, --timeout=<ms>    I/O timeouts in milliseconds
--close-no-sync       Do not wait for connection to be closed by server(disconnect first)
--schema=<schema>     Default schema to connect to
--uri=<uri>           Connection URI
                      URI takes precedence before options like: user, host, password, port
--socket=<file>       Connection through UNIX socket
--use-socket          Connection through UNIX socket, using default file name 'SOCK'
                      --use-socket* options take precedence before options like: uri, user,
                      host, password, port
--compression-mode=<mode>  Enable/disable compression negotiation algorithm (default: "DISABLED")
                      "DISABLED" - do not negotiate compression
                      "PREFERRED" - if server doesn't support selected compression, continue without compression.
                      "REQUIRED" - if server doesn't support selected compression, fail the connection.
--compression-algorithm=<algo[,algo...]>  Try to negotiate specified compression algorithm with the server (default:"DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM")
                      "" - compression not enabled
                      "DEFLATE_STREAM" - zlib compression enabled
                      "LZ4_MESSAGE" - lz4f compression enabled
                      "ZSTD_STREAM" - zstd compression enabled
--compression-combine-mixed-messages=<0/1>  If 1, server is allowed to combine different message types into a compressed message (default: 1)
--compression-max-combine-messages=<N>  If set, the server MUST not store more than N uncompressed messages into a compressed message (default: no limit)
--compression-level=<N>  If set, the server MUST compress messages with given level N; otherwise the server use default level (depend on compression algorithm)
--ssl-mode            SSL configuration (default: "")
                      "" - require encryption when at last one ssl option is set, otherwise is should be disabled.
                      "PREFERRED" - encryption is optional, client tries to set it up
                      "DISABLED" - encryption is disabled
                      "REQUIRED" - encryption is required
                      "VERIFY_CA" - verify server certificate
                      "VERIFY_IDENTITY" - verify certificate issuer
--ssl-key             X509 key in PEM format
--ssl-ca              CA file in PEM format
--ssl-ca_path         CA directory
--ssl-cert            X509 cert in PEM format
--ssl-cipher          SSL cipher to use
--tls-version         TLS version to use
--ssl-fips-mode       Fips mode to use
--connect-expired-password Allow expired password
--client-interactive  Connect in interactive mode
--quiet               Don't print out messages sent
-vVARIABLE_NAME=VALUE Set variable VARIABLE_NAME from command line
--fatal-errors=<0|1>  Mysqlxtest is started with ignoring or stopping on fatal error (default: 1)
--expect-error=<error_code> Default connection must fail with specified code (default: OFF)
-B, --bindump         Dump binary representation of messages sent, in format suitable for
--trace-protocol      Enable X Protocol tracing
--history-protocol=<0|1|true|false>
                      Enable/disable X Protocol message tracing into
                      history buffer, which is printed at test failure
--verbose             Enable extra verbose messages
--daemon              Work as a daemon (unix only)
--help                Show command line help
--help-commands       Show help for input commands
-V, --version         Show version of mysqlxtest

Only one option that changes run mode is allowed.

############################################################
## Test mysqlxtest executable help-command printout
##
Input may be a file (or if no --file is specified, it stdin will be used)
The following commands may appear in the input script:
-->echo <text>
  Prints the text (allows variables)
-->title <c><text>
  Prints the text with an underline, using the character <c>
-->sql
  Begins SQL block. SQL statements that appear will be executed and results printed (allows variables).
-->endsql
  End SQL block. End a block of SQL started by -->sql
-->begin_compress
  Begins block of protobuf messages to compress and
  encapsulate inside single 'Compressed' message.
-->end_compress
  End compressed message block. End a block started by -->begin_compress
-->macro <macroname> <argname1> ...
  Start a block of text to be defined as a macro. Must be terminated with -->endmacro
-->endmacro
  Ends a macro block
-->callmacro <macro>	<argvalue1>	...
  Executes the macro text, substituting argument values with the provided ones (args separated by tabs).
-->import <macrofile>
  Loads macros from the specified file. The file must be in the directory specified by --import option in command line.
-->macro_delimiter_compress TRUE|FALSE|0|1
  Enable/disable grouping of adjacent delimiters into
  single one at "callmacro" command.
-->do_ssl_handshake
  Execute SSL handshake, enables SSL on current connection
<protomsg>
  Encodes the text format protobuf message and sends it to the server (allows variables).
-->enable_compression deflate_stream|lz4_message|zstd_stream [#level]
  Enable compression
-->recv [quiet|<FIELD PATH>]
  quiet        - received message isn't printed
  <FIELD PATH> - print only selected part of the message using
                 "field-path" filter:
                 * field_name1
                 * field_name1.field_name2
                 * repeated_field_name1[1].field_name1
-->recvresult [print-columnsinfo] [be-quiet]
  Read and print one resultset from the server; if print-columnsinfo is present also print short columns status
-->recvtovar <varname> [COLUMN_NAME]
  Read first row and first column (or column with name COLUMN_NAME) of resultset
  and set the variable <varname>
-->recverror <errno>
  Read a message and ensure that it's an error of the expected type
-->recvtype (<msgtype> [<msg_fied>] [<expected_field_value>] [be-quiet])|<msgid>
  - In case when user specified <msgtype> - read one message and print it,
    checks if its type is <msgtype>, additionally its fields may be matched.
    Compressed messages are decompressed, thus user will receive inner X Protocol messages.
  - In case when user specified <msgid> - read one message and print the ID,
    checks the RAW message ID if its match <msgid>.
    Compressed messages are not decompressed, thus their IDs may be matched against <msgid>.
-->recvok
  Expect to receive 'Mysqlx.Ok' message. Works with 'expecterror' command.
-->recvuntil <msgtype> [do_not_show_intermediate]
  Read messages and print them, until a msg of the specified type (or Error) is received
  do_not_show_intermediate - if this argument is present then printing of intermediate message should be omitted
-->repeat <N> [<VARIABLE_NAME>]
  Begin block of instructions that should be repeated N times
-->endrepeat
  End block of instructions that should be repeated - next iteration
-->stmtsql <CMD>
  Send StmtExecute with sql command
-->env <XVARIABLE> <ENV>
  Assign environment variable to X variable.
-->stmtadmin <CMD> [json_string]
  Send StmtExecute with admin command with given aguments (formated as json object)
-->system_in_background <CMD>
  Execute application or script.
-->system <CMD>
  Execute application or script
-->exit
  Stops reading commands, disconnects and exits (same as <eof>/^D)
-->abort
  Exit immediately, without performing cleanup
-->shutdown_server [timeout]
  Kills the server associated with current session.
-->nowarnings/-->yeswarnings
  Whether to print warnings generated by the statement (default no)
-->recvuntildisc [show-received, keep-session]...
  Receive all messages until server drops current connection.
  show-received - received messages are printed to standard output.
  keep-session - session descriptor is not released in mysqlxtest, user may execute 'reconnect'.
-->peerdisc <MILLISECONDS> [TOLERANCE]
  Expect that xplugin disconnects after given number of milliseconds and tolerance
-->sleep <SECONDS>
  Stops execution of mysqlxtest for given number of seconds (may be fractional)
-->login <user>	<pass>	<db>	<mysql41|plain|sha256_memory>]
  Performs authentication steps (use with --no-auth)
-->loginerror <errno>	<user>	<pass>	<db>
  Performs authentication steps expecting an error (use with --no-auth)
-->fatalerrors/nofatalerrors
  Whether to immediately exit on MySQL errors.
  All expected errors are ignored.
-->fatalwarnings [yes|no|true|false|1|0]
  Whether to immediately exit on MySQL warnings.
  All expected warnings are ignored.
-->expectwarnings <errno>[,<errno>[,<errno>...]]
  Expect a specific warning for the next command. Fails if warning other than specified occurred.
  When this command was not used then all warnings are expected.
  Works for: recvresult, SQL
-->expecterror <errno>[,<errno>[,<errno>...]]
  Expect a specific error for the next command. Fails if error other than specified occurred
  Works for: newsession, closesession, recvresult, recvok, SQL
-->newsession <name>	<user>	<pass>	<db>
  Create a new connection which is going to be authenticate using sequence of mechanisms (AUTO). Use '-' in place of the user for raw connection.
-->newsession_mysql41 <name>	<user>	<pass>	<db>
  Create a new connection which is going to be authenticate using MYSQL41 mechanism.
-->newsession_memory <name>	<user>	<pass>	<db>
  Create a new connection which is going to be authenticate using SHA256_MEMORY mechanism.
-->newsession_plain <name>	<user>	<pass>	<db>
  Create a new connection which is going to be authenticate using PLAIN mechanism.
-->reconnect
  Try to restore the connection/session. Default connection  is restored or session established by '-->newsession*'.
-->setsession <name>
  Activate the named session
-->closesession [abort]
  Close the active session (unless its the default session)
-->wait_for <VALUE_EXPECTED>	<SQL QUERY>
  Wait until SQL query returns value matches expected value (time limit 30 second)
-->assert <VALUE_EXPECTED>	<OP>	<VALUE_TESTED>
  Ensure that expression described by argument parameters is true
  <OP> can take following values:
  "==" ensures that expected value and tested value are equal
  "!=" ensures that expected value and tested value are not equal
  ">=" ensures that expected value is greater or equal to tested value
  "<=" ensures that expected value is less or equal to tested value
  "<" ensures that expected value is less than tested value
  ">" ensures that expected value is grater than tested value

  For example: -->assert 1 < %SOME_VARIABLE%
               -->assert %V1% == %V2%
-->assert_eq <VALUE_EXPECTED>	<VALUE_TESTED>
  Ensure that 'TESTED' value equals 'EXPECTED' by comparing strings lexicographically
-->assert_ne <VALUE_EXPECTED>	<VALUE_TESTED>
  Ensure that 'TESTED' value doesn't equals 'EXPECTED' by comparing strings lexicographically
-->assert_gt <VALUE_EXPECTED>	<VALUE_TESTED>
  Ensure that 'TESTED' value is greater than 'EXPECTED' (only when the both are numeric values)
-->assert_ge <VALUE_EXPECTED>	<VALUE_TESTED>
  Ensure that 'TESTED' value is greater  or equal to 'EXPECTED' (only when the both are numeric values)
-->varfile <varname> <datafile>
  Assigns the contents of the file to the named variable
-->varlet <varname> <value>
  Assign the value (can be another variable) to the variable
-->varinc <varname> <n>
  Increment the value of varname by n (assuming both convert to integral)
-->varsub <varname>
  Add a variable to the list of variables to replace for the next recv or sql command (value is replaced by the name)
-->varreplace <varname>	<old_txt>	<new_txt>
  Replace all occurrence of <old_txt> with <new_txt> in <varname> value.
-->varescape <varname>
  Escape end-line and backslash characters.
-->compress_bin <VAR> <bindump>
  Compress <bindump> using output-compression context
  and put the output to <VAR> encoded using bindump
  (compatible with protobuf text format).
-->compress_hex <VAR> <hexdump>
  Compress <hexdump> using output-compression context
  and put the output to <VAR> encoded using hexdecimal string.
-->binsend <bindump>[<bindump>...]
  Sends one or more binary message dumps to the server (generate those with --bindump)
-->binsendoffset <srcvar> [offset-begin[percent]> [offset-end[percent]]]
  Same as binsend with begin and end offset of data to be send
-->binparse <VAR_NAME> MESSAGE.NAME {
    MESSAGE.DATA
}
  Dump given message to variable <VAR_NAME>, encoded as 
  binary string (compatible with protobuf text format).
-->hexparse <VAR_NAME> MESSAGE.NAME {
    MESSAGE.DATA
}
  Dump given message to variable <VAR_NAME>, encoded as 
  hexdecimal string.
-->quiet/noquiet
  Toggle verbose messages
-->query_result/noquery_result
  Toggle visibility for query results
-->received <msgtype>	<varname>
  Assigns number of received messages of indicated type (in active session) to a variable
-->clear_received
  Clear number of received messages.
-->recvresult_store_metadata <METADATA_TAG> [print-columnsinfo] [be-quiet]
  Receive result and store metadata for future use; if print-columnsinfo is present also print short columns status
-->recv_with_stored_metadata <METADATA_TAG>
  Receive a message using a previously stored metadata
-->clear_stored_metadata
  Clear metadata information stored by the recvresult_store_metadata
# comment
