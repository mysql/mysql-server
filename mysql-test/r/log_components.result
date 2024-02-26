#
# WL#9323:  Logging services: Improved error logging in 8.0
#
SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET @@global.log_error_verbosity=3;
FLUSH LOGS;
SET @@session.debug="+d,parser_stmt_to_error_log";
SET @@session.debug="+d,log_error_normalize";
SELECT @@global.log_error_services;
@@global.log_error_services
log_filter_internal; log_sink_internal

SELECT "*** SWITCHING ERROR LOG TO JSON ***";
*** SWITCHING ERROR LOG TO JSON ***
*** SWITCHING ERROR LOG TO JSON ***
INSTALL COMPONENT "file://component_log_sink_json";
SET @@global.log_error_services="log_filter_internal; log_sink_json;";
# may not start with a delimiter
SET @@global.log_error_services=";log_sink_internal";
ERROR 42000: Variable 'log_error_services' can't be set to the value of ';log_sink_internal'
SHOW WARNINGS;
Level	Code	Message
Warning	3701	Value for log_error_services got confusing at or around ";log_sink_internal". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
Error	1231	Variable 'log_error_services' can't be set to the value of ';log_sink_internal'
# may not end on a filter
SET @@global.log_error_services="log_sink_internal; log_filter_internal";
ERROR 42000: Variable 'log_error_services' can't be set to the value of 'log_sink_internal; log_filter_internal'
SHOW WARNINGS;
Level	Code	Message
Warning	3701	Value for log_error_services got confusing at or around "". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
Error	1231	Variable 'log_error_services' can't be set to the value of 'log_sink_internal; log_filter_internal'
# may not mix delimiters
SET @@global.log_error_services="log_filter_internal;,log_sink_internal";
ERROR 42000: Variable 'log_error_services' can't be set to the value of 'log_filter_internal;,log_sink_internal'
SHOW WARNINGS;
Level	Code	Message
Warning	3701	Value for log_error_services got confusing at or around ",log_sink_internal". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
Error	1231	Variable 'log_error_services' can't be set to the value of 'log_filter_internal;,log_sink_internal'
# whitespace alone is no valid delimiter
SET @@global.log_error_services="log_filter_internal log_sink_internal";
ERROR 42000: Variable 'log_error_services' can't be set to the value of 'log_filter_internal log_sink_internal'
SHOW WARNINGS;
Level	Code	Message
Warning	3701	Value for log_error_services got confusing at or around "log_sink_internal". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
Error	1231	Variable 'log_error_services' can't be set to the value of 'log_filter_internal log_sink_internal'
# unknown component
SET @@global.log_error_services="azundris";
ERROR HY000: Can't open shared library 'component_azundris'
SHOW WARNINGS;
Level	Code	Message
Error	1126	Can't open shared library 'component_azundris'
Error	3529	Cannot load component from specified URN: 'file://component_azundris'.
Warning	3701	Value for log_error_services got confusing at or around "azundris". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
# Internal component no longer exists and is therefore looked for
# as a loadable component (which also does not exist).
SET @@global.log_error_services="log_sink_buffer";
ERROR HY000: Can't open shared library 'component_log_sink_buffer'
SHOW WARNINGS;
Level	Code	Message
Error	1126	Can't open shared library 'component_log_sink_buffer'
Error	3529	Cannot load component from specified URN: 'file://component_log_sink_buffer'.
Warning	3701	Value for log_error_services got confusing at or around "log_sink_buffer". Syntax may be wrong, component may not be INSTALLed, or a component that does not support instances may be listed more than once.
SELECT "logging as JSON";
logging as JSON
logging as JSON
SET @@global.log_error_services="log_filter_internal, log_sink_internal,";
UNINSTALL COMPONENT "file://component_log_sink_json";

SELECT "*** SWITCHING ERROR LOG TO TRAD AND JSON ***";
*** SWITCHING ERROR LOG TO TRAD AND JSON ***
*** SWITCHING ERROR LOG TO TRAD AND JSON ***
INSTALL COMPONENT "file://component_log_sink_json";
SET @@global.log_error_services="log_filter_internal, log_sink_internal, log_sink_json";
SELECT "logging as traditional MySQL error log and as JSON";
logging as traditional MySQL error log and as JSON
logging as traditional MySQL error log and as JSON
SET @@global.log_error_services="log_filter_internal; log_sink_internal";
UNINSTALL COMPONENT "file://component_log_sink_json";

SELECT "*** TRYING TO LOG THINGS FROM EXTERNAL SERVICE ***";
*** TRYING TO LOG THINGS FROM EXTERNAL SERVICE ***
*** TRYING TO LOG THINGS FROM EXTERNAL SERVICE ***
# NB: log_sink_test must self-disable its sink after one line.
INSTALL COMPONENT "file://component_log_sink_json";
INSTALL COMPONENT "file://component_log_sink_test";
SET @@global.log_error_services="log_filter_internal; log_sink_test; log_sink_json";
SELECT "logging as traditional MySQL error log and as JSON";
logging as traditional MySQL error log and as JSON
logging as traditional MySQL error log and as JSON
# Force a call to variable-checking in all loaded log-services
SET @@global.log_error_filter_rules= DEFAULT;
ERROR HY000: Unknown system variable 'log_error_filter_rules'
# Reload logging services
FLUSH ERROR LOGS;
# Should not be able to uninstall as it's in use.
UNINSTALL COMPONENT "file://component_log_sink_test";
ERROR HY000: Unregistration of service implementation 'log_service.log_sink_test' provided by component 'mysql:log_sink_test' failed during unloading of the component.
# Remove from active set:
SET @@global.log_error_services="log_filter_internal; log_sink_internal";
# Now we can uninstall.
UNINSTALL COMPONENT "file://component_log_sink_test";
UNINSTALL COMPONENT "file://component_log_sink_json";

# cleanup
SET @@session.debug="-d,parser_stmt_to_error_log";
SET @@session.debug="-d,log_error_normalize";
SET @@global.log_error_verbosity= @orig_log_error_verbosity;
FLUSH ERROR LOGS;

###
### error log file
###

1970-01-01T00:00:00.000001Z 0 [Note] [MY-010000] [Server] Parser saw: SET @@session.debug="+d,log_error_normalize"
1970-01-01T00:00:00.000002Z 0 [Note] [MY-010000] [Server] Parser saw: SELECT @@global.log_error_services
1970-01-01T00:00:00.000003Z 0 [Note] [MY-010000] [Server] Parser saw: SELECT "*** SWITCHING ERROR LOG TO JSON ***"
1970-01-01T00:00:00.000004Z 0 [Note] [MY-010000] [Server] Parser saw: INSTALL COMPONENT "file://component_log_sink_json"
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_filter_internal; log_sink_json;\"", "time" : "1970-01-01T00:00:00.000005Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\";log_sink_internal\"", "time" : "1970-01-01T00:00:00.000006Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000007Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_sink_internal; log_filter_internal\"", "time" : "1970-01-01T00:00:00.000008Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000009Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_filter_internal;,log_sink_internal\"", "time" : "1970-01-01T00:00:00.000010Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000011Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_filter_internal log_sink_internal\"", "time" : "1970-01-01T00:00:00.000012Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000013Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"azundris\"", "time" : "1970-01-01T00:00:00.000014Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000015Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_sink_buffer\"", "time" : "1970-01-01T00:00:00.000016Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SHOW WARNINGS", "time" : "1970-01-01T00:00:00.000017Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SELECT \"logging as JSON\"", "time" : "1970-01-01T00:00:00.000018Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
1970-01-01T00:00:00.000019Z 0 [Note] [MY-010000] [Server] Parser saw: SET @@global.log_error_services="log_filter_internal, log_sink_internal,"
1970-01-01T00:00:00.000020Z 0 [Note] [MY-010000] [Server] Parser saw: UNINSTALL COMPONENT "file://component_log_sink_json"
1970-01-01T00:00:00.000021Z 0 [Note] [MY-010000] [Server] Parser saw: SELECT "*** SWITCHING ERROR LOG TO TRAD AND JSON ***"
1970-01-01T00:00:00.000022Z 0 [Note] [MY-010000] [Server] Parser saw: INSTALL COMPONENT "file://component_log_sink_json"
1970-01-01T00:00:00.000023Z 0 [Note] [MY-010000] [Server] Parser saw: SET @@global.log_error_services="log_filter_internal, log_sink_internal, log_sink_json"
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_filter_internal, log_sink_internal, log_sink_json\"", "time" : "1970-01-01T00:00:00.000023Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
1970-01-01T00:00:00.000024Z 0 [Note] [MY-010000] [Server] Parser saw: SELECT "logging as traditional MySQL error log and as JSON"
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SELECT \"logging as traditional MySQL error log and as JSON\"", "time" : "1970-01-01T00:00:00.000024Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
1970-01-01T00:00:00.000025Z 0 [Note] [MY-010000] [Server] Parser saw: SET @@global.log_error_services="log_filter_internal; log_sink_internal"
1970-01-01T00:00:00.000026Z 0 [Note] [MY-010000] [Server] Parser saw: UNINSTALL COMPONENT "file://component_log_sink_json"
1970-01-01T00:00:00.000027Z 0 [Note] [MY-010000] [Server] Parser saw: SELECT "*** TRYING TO LOG THINGS FROM EXTERNAL SERVICE ***"
1970-01-01T00:00:00.000028Z 0 [Note] [MY-010000] [Server] Parser saw: INSTALL COMPONENT "file://component_log_sink_json"
1970-01-01T00:00:00.000029Z 0 [Note] [MY-010000] [Server] Parser saw: INSTALL COMPONENT "file://component_log_sink_test"
[prio=3];[err_code=10000];[subsystem=Server];[source_file=sql_parse.cc];[function=dispatch_command];[msg=Parser saw: SET @@global.log_error_services="log_filter_internal; log_sink_test; log_sink_json"];[time=1970-01-01T00:00:00.000030Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
[prio=3];[err_symbol=ER_PARSER_TRACE];[msg=using log_message() in external service];[time=1970-01-01T00:00:00.000031Z];[ts=0];[thread=0];[err_code=10000];[SQL_state=XX999];[subsystem=Server];[label=Note];
{ "prio" : 3, "err_symbol" : "ER_PARSER_TRACE", "msg" : "using log_message() in external service", "time" : "1970-01-01T00:00:00.000031Z", "ts" : 0, "thread" : 0, "err_code" : 10000, "SQL_state" : "XX999", "subsystem" : "Server", "label" : "Note" }
[prio=1];[err_symbol=ER_PARSER_TRACE];[subsystem=Server];[msg=using log_message() with duplicate source-line k/v pair];[time=1970-01-01T00:00:00.000032Z];[ts=0];[thread=0];[err_code=10000];[SQL_state=XX999];[label=Error];
{ "prio" : 1, "err_symbol" : "ER_PARSER_TRACE", "subsystem" : "Server", "msg" : "using log_message() with duplicate source-line k/v pair", "time" : "1970-01-01T00:00:00.000032Z", "ts" : 0, "thread" : 0, "err_code" : 10000, "SQL_state" : "XX999", "label" : "Error" }
[prio=1];[err_symbol=ER_PARSER_TRACE];[key=val];[msg=using log_message() with duplicate generic C-string k/v pair];[time=1970-01-01T00:00:00.000033Z];[ts=0];[thread=0];[err_code=10000];[SQL_state=XX999];[subsystem=Server];[label=Error];
{ "prio" : 1, "err_symbol" : "ER_PARSER_TRACE", "key" : "val", "msg" : "using log_message() with duplicate generic C-string k/v pair", "time" : "1970-01-01T00:00:00.000033Z", "ts" : 0, "thread" : 0, "err_code" : 10000, "SQL_state" : "XX999", "subsystem" : "Server", "label" : "Error" }
[prio=1];[err_symbol=ER_PARSER_TRACE];[key=4711];[msg=using log_message() with duplicate generic mixed k/v pair];[time=1970-01-01T00:00:00.000034Z];[ts=0];[thread=0];[err_code=10000];[SQL_state=XX999];[subsystem=Server];[label=Error];
{ "prio" : 1, "err_symbol" : "ER_PARSER_TRACE", "key" : 4711, "msg" : "using log_message() with duplicate generic mixed k/v pair", "time" : "1970-01-01T00:00:00.000034Z", "ts" : 0, "thread" : 0, "err_code" : 10000, "SQL_state" : "XX999", "subsystem" : "Server", "label" : "Error" }
[prio=1];[err_symbol=ER_PARSER_TRACE];[msg=using log_message() with errno 0];[time=1970-01-01T00:00:00.000035Z];[ts=0];[thread=0];[err_code=10000];[SQL_state=XX999];[subsystem=Server];[label=Error];
{ "prio" : 1, "err_symbol" : "ER_PARSER_TRACE", "msg" : "using log_message() with errno 0", "time" : "1970-01-01T00:00:00.000035Z", "ts" : 0, "thread" : 0, "err_code" : 10000, "SQL_state" : "XX999", "subsystem" : "Server", "label" : "Error" }
[prio=1];[err_code=13151];[msg=Simulated error];[time=1970-01-01T00:00:00.000036Z];[ts=0];[thread=0];[err_symbol=ER_SERVER_TEST_MESSAGE];[SQL_state=HY000];[subsystem=Server];[label=Error];
{ "prio" : 1, "err_code" : 13151, "msg" : "Simulated error", "time" : "1970-01-01T00:00:00.000036Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_SERVER_TEST_MESSAGE", "SQL_state" : "HY000", "subsystem" : "Server", "label" : "Error" }
[prio=1];[err_symbol=ER_SERVER_TEST_MESSAGE];[msg=using log_message() with errsymbol];[time=1970-01-01T00:00:00.000037Z];[ts=0];[thread=0];[err_code=13151];[SQL_state=HY000];[subsystem=Server];[label=Error];
{ "prio" : 1, "err_symbol" : "ER_SERVER_TEST_MESSAGE", "msg" : "using log_message() with errsymbol", "time" : "1970-01-01T00:00:00.000037Z", "ts" : 0, "thread" : 0, "err_code" : 13151, "SQL_state" : "HY000", "subsystem" : "Server", "label" : "Error" }
[prio=1];[time=1970-01-01T00:00:00.000038Z];[ts=0];[thread=0];[subsystem=Server];[label=Error];
{ "prio" : 1, "time" : "1970-01-01T00:00:00.000038Z", "ts" : 0, "thread" : 0, "subsystem" : "Server", "label" : "Error" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=XX999];[source_file=log_sink_test];[test_float=3.141592692700];[test_int=739241];[test_cstring=cstring];[test_lexstring=lexstring];[msg=using LogEvent() object in external service];[time=1970-01-01T00:00:00.000039Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "XX999", "source_file" : "log_sink_test", "test_float" : 3.141592692700, "test_int" : 739241, "test_cstring" : "cstring", "test_lexstring" : "lexstring", "msg" : "using LogEvent() object in external service", "time" : "1970-01-01T00:00:00.000039Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#1): NOT_FOUND];[time=1970-01-01T00:00:00.000040Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#1): NOT_FOUND", "time" : "1970-01-01T00:00:00.000040Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#2): CLASS_MISMATCH];[time=1970-01-01T00:00:00.000041Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#2): CLASS_MISMATCH", "time" : "1970-01-01T00:00:00.000041Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#2): KEY_MISMATCH];[time=1970-01-01T00:00:00.000042Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#2): KEY_MISMATCH", "time" : "1970-01-01T00:00:00.000042Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#3): KEY_NULL];[time=1970-01-01T00:00:00.000043Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#3): KEY_NULL", "time" : "1970-01-01T00:00:00.000043Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#4): STRING_NULL];[time=1970-01-01T00:00:00.000044Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#4): STRING_NULL", "time" : "1970-01-01T00:00:00.000044Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[msg=item_inconsistent(#5): OK];[time=1970-01-01T00:00:00.000045Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "item_inconsistent(#5): OK", "time" : "1970-01-01T00:00:00.000045Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[SQL_state=XX999];[subsystem=log_sink_test];[component=log_sink_test];[source_file=log_sink_test];[msg=below: 3*unknown error per writer == correct.  >3*unknown error per writer == filter fail. 0*unknown error == log_sink_test fail.];[time=1970-01-01T00:00:00.000046Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "SQL_state" : "XX999", "subsystem" : "log_sink_test", "component" : "log_sink_test", "source_file" : "log_sink_test", "msg" : "below: 3*unknown error per writer == correct.  >3*unknown error per writer == filter fail. 0*unknown error == log_sink_test fail.", "time" : "1970-01-01T00:00:00.000046Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=XX999];[source_file=log_sink_test];[msg=filter_rules: (throttle: delta in medias res) 1];[time=1970-01-01T00:00:00.000047Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "XX999", "source_file" : "log_sink_test", "msg" : "filter_rules: (throttle: delta in medias res) 1", "time" : "1970-01-01T00:00:00.000047Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
[log_type=1];[prio=3];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=HY000];[source_file=log_sink_test];[err_code=13151];[msg=Simulated error];[time=1970-01-01T00:00:00.000048Z];[ts=0];[thread=0];[err_symbol=ER_SERVER_TEST_MESSAGE];[label=Note];
{ "log_type" : 1, "prio" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "HY000", "source_file" : "log_sink_test", "err_code" : 13151, "msg" : "Simulated error", "time" : "1970-01-01T00:00:00.000048Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_SERVER_TEST_MESSAGE", "label" : "Note" }
[log_type=1];[prio=3];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=HY000];[source_file=log_sink_test];[err_code=13151];[msg=Simulated error];[time=1970-01-01T00:00:00.000049Z];[ts=0];[thread=0];[err_symbol=ER_SERVER_TEST_MESSAGE];[label=Note];
{ "log_type" : 1, "prio" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "HY000", "source_file" : "log_sink_test", "err_code" : 13151, "msg" : "Simulated error", "time" : "1970-01-01T00:00:00.000049Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_SERVER_TEST_MESSAGE", "label" : "Note" }
[log_type=1];[prio=3];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=HY000];[source_file=log_sink_test];[err_code=13151];[msg=Simulated error];[time=1970-01-01T00:00:00.000050Z];[ts=0];[thread=0];[err_symbol=ER_SERVER_TEST_MESSAGE];[label=Note];
{ "log_type" : 1, "prio" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "HY000", "source_file" : "log_sink_test", "err_code" : 13151, "msg" : "Simulated error", "time" : "1970-01-01T00:00:00.000050Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_SERVER_TEST_MESSAGE", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[subsystem=log_sink_test];[component=log_sink_test];[SQL_state=XX999];[source_file=log_sink_test];[msg=filter_rules: (throttle: delta ex post) 0];[time=1970-01-01T00:00:00.000064Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "subsystem" : "log_sink_test", "component" : "log_sink_test", "SQL_state" : "XX999", "source_file" : "log_sink_test", "msg" : "filter_rules: (throttle: delta ex post) 0", "time" : "1970-01-01T00:00:00.000064Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "label" : "Note" }
[log_type=1];[err_code=10000];[meow=test_me_for_presence];[SQL_state=XX999];[prio_change=yes];[subsystem=log_sink_test];[component=log_sink_test];[source_file=log_sink_test];[msg=filter_rules: (add_item delta in medias res) 3];[prio=2];[time=1970-01-01T00:00:00.000065Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[far=added_by_rule];[label=Warning];
{ "log_type" : 1, "err_code" : 10000, "meow" : "test_me_for_presence", "SQL_state" : "XX999", "prio_change" : "yes", "subsystem" : "log_sink_test", "component" : "log_sink_test", "source_file" : "log_sink_test", "msg" : "filter_rules: (add_item delta in medias res) 3", "prio" : 2, "time" : "1970-01-01T00:00:00.000065Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "far" : "added_by_rule", "label" : "Warning" }
[log_type=1];[err_code=10000];[meow=test_me_for_presence];[bark=delete_me_by_rule];[prio_change=yes];[subsystem=log_sink_test];[component=log_sink_test];[source_file=log_sink_test];[msg=filter_rules: (add_item delta ex post) 0];[prio=1];[time=1970-01-01T00:00:00.000066Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Error];
{ "log_type" : 1, "err_code" : 10000, "meow" : "test_me_for_presence", "bark" : "delete_me_by_rule", "prio_change" : "yes", "subsystem" : "log_sink_test", "component" : "log_sink_test", "source_file" : "log_sink_test", "msg" : "filter_rules: (add_item delta ex post) 0", "prio" : 1, "time" : "1970-01-01T00:00:00.000066Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Error" }
[log_type=1];[prio=3];[err_code=10000];[wl9651_val1=0];[wl9651_val2=1];[wl9651_val3a=1];[wl9651_val3b=2];[wl9651_val3c=3];[subsystem=log_sink_test];[component=log_sink_test];[msg=WL#9651 expected: r1-IF, r2-SUCCESS, r3-SUCCESS];[time=1970-01-01T00:00:00.000067Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "wl9651_val1" : 0, "wl9651_val2" : 1, "wl9651_val3a" : 1, "wl9651_val3b" : 2, "wl9651_val3c" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "WL#9651 expected: r1-IF, r2-SUCCESS, r3-SUCCESS", "time" : "1970-01-01T00:00:00.000067Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[wl9651_val1=1];[wl9651_val2=2];[wl9651_val3a=0];[wl9651_val3b=2];[wl9651_val3c=3];[subsystem=log_sink_test];[component=log_sink_test];[msg=WL#9651 expected: r1-ELSEIF1, r2-SUCCESS, r3-FAILURE];[time=1970-01-01T00:00:00.000068Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "wl9651_val1" : 1, "wl9651_val2" : 2, "wl9651_val3a" : 0, "wl9651_val3b" : 2, "wl9651_val3c" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "WL#9651 expected: r1-ELSEIF1, r2-SUCCESS, r3-FAILURE", "time" : "1970-01-01T00:00:00.000068Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[wl9651_val1=2];[wl9651_val2=3.100000000000];[wl9651_val3a=1];[wl9651_val3b=0];[wl9651_val3c=3];[subsystem=log_sink_test];[component=log_sink_test];[msg=WL#9651 expected: r1-ELSEIF2, r2-FAILURE, r3-FAILURE];[time=1970-01-01T00:00:00.000069Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "wl9651_val1" : 2, "wl9651_val2" : 3.100000000000, "wl9651_val3a" : 1, "wl9651_val3b" : 0, "wl9651_val3c" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "WL#9651 expected: r1-ELSEIF2, r2-FAILURE, r3-FAILURE", "time" : "1970-01-01T00:00:00.000069Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[wl9651_val1=3];[wl9651_val2=4];[wl9651_val3a=1];[wl9651_val3b=2];[wl9651_val3c=0];[subsystem=log_sink_test];[component=log_sink_test];[msg=WL#9651 expected: r1-ELSE, r2-FAILURE, r3-FAILURE];[time=1970-01-01T00:00:00.000070Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "wl9651_val1" : 3, "wl9651_val2" : 4, "wl9651_val3a" : 1, "wl9651_val3b" : 2, "wl9651_val3c" : 0, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "WL#9651 expected: r1-ELSE, r2-FAILURE, r3-FAILURE", "time" : "1970-01-01T00:00:00.000070Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[log_type=1];[prio=3];[err_code=10000];[wl9651_val1=-7];[wl9651_val2=1];[wl9651_val3a=1];[wl9651_val3c=3];[subsystem=log_sink_test];[component=log_sink_test];[msg=WL#9651 expected: r1-IF, r2-FAILURE, r3-FAILURE];[time=1970-01-01T00:00:00.000071Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "log_type" : 1, "prio" : 3, "err_code" : 10000, "wl9651_val1" : -7, "wl9651_val2" : "1", "wl9651_val3a" : 1, "wl9651_val3c" : 3, "subsystem" : "log_sink_test", "component" : "log_sink_test", "msg" : "WL#9651 expected: r1-IF, r2-FAILURE, r3-FAILURE", "time" : "1970-01-01T00:00:00.000071Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[label=log_sink_test: broken item with class 3, type 131072];[err_code=10000];[prio=1];[time=1970-01-01T00:00:00.000072Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[subsystem=Server];[label=Error];
{ "label" : "log_sink_json: broken item with class 3, type 131072", "err_code" : 10000, "prio" : 1, "time" : "1970-01-01T00:00:00.000072Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "subsystem" : "Server", "label" : "Error" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_services=\"log_filter_internal; log_sink_test; log_sink_json\"", "time" : "1970-01-01T00:00:00.000030Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SELECT \"logging as traditional MySQL error log and as JSON\"", "time" : "1970-01-01T00:00:00.000073Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: SET @@global.log_error_filter_rules= DEFAULT", "time" : "1970-01-01T00:00:00.000074Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[prio=3];[err_code=10000];[subsystem=Server];[source_file=sql_parse.cc];[function=dispatch_command];[msg=Parser saw: FLUSH ERROR LOGS];[time=1970-01-01T00:00:00.000075Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: FLUSH ERROR LOGS", "time" : "1970-01-01T00:00:00.000075Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
[prio=3];[err_code=10000];[subsystem=Server];[source_file=sql_parse.cc];[function=dispatch_command];[msg=Parser saw: UNINSTALL COMPONENT "file://component_log_sink_test"];[time=1970-01-01T00:00:00.000076Z];[ts=0];[thread=0];[err_symbol=ER_PARSER_TRACE];[SQL_state=XX999];[label=Note];
{ "prio" : 3, "err_code" : 10000, "subsystem" : "Server", "source_file" : "sql_parse.cc", "function" : "dispatch_command", "msg" : "Parser saw: UNINSTALL COMPONENT \"file://component_log_sink_test\"", "time" : "1970-01-01T00:00:00.000076Z", "ts" : 0, "thread" : 0, "err_symbol" : "ER_PARSER_TRACE", "SQL_state" : "XX999", "label" : "Note" }
1970-01-01T00:00:00.000077Z 0 [Note] [MY-010000] [Server] Parser saw: SET @@global.log_error_services="log_filter_internal; log_sink_internal"
1970-01-01T00:00:00.000078Z 0 [Note] [MY-010000] [Server] Parser saw: UNINSTALL COMPONENT "file://component_log_sink_test"
1970-01-01T00:00:00.000079Z 0 [Note] [MY-010000] [Server] Parser saw: UNINSTALL COMPONENT "file://component_log_sink_json"

###
### done
###
