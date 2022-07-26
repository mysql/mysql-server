#
# WL#14793: Load log-components such as the JSON-logger earlier (before InnoDB)
#
SET @@session.time_zone=@@global.log_timestamps;

# Find (timestamp of) most recent row in performance_schema.error_log.
# This will exclude log entries from other tests or --repeat=...
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
INTO @pfs_errlog_latest
FROM performance_schema.global_status
WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

# NFR-1.1 log_error_services
#
# The MySQL server will attempt to implicitly load any loadable
# logging component listed in log_error_services that has not been
# loaded yet. The loading is attempted on assignment of the variable.

# No logging components were explicitly loaded with INSTALL COMPONENT:
SELECT component_urn FROM mysql.component;
component_urn

# Enable logging in the JSON format.
# Since we did not "INSTALL COMPONENT log_sink_json;" (and proved so
# above), this will fail on a server pre-WL#14793 that cannot implicitly
# load error logging components. On a server post WL#14793, log_sink_json
# will be loaded implicitly at this point.
SET @@global.log_error_services="log_sink_json";

# Start echoing all statements to the error log.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";

# Stop echoing all statements to the error log.
SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";

# Extract a key/value pair from a JSON record.
# This will show that JSON logging is now active, and by implication,
# that the JSON component was implicitly loaded.
SELECT prio,error_code,subsystem,
JSON_EXTRACT(data,'$.err_symbol'),JSON_EXTRACT(data,'$.msg')
FROM performance_schema.error_log
WHERE logged>@pfs_errlog_latest
AND LEFT(data,1)='{'
   AND JSON_EXTRACT(data,'$.err_symbol')="ER_PARSER_TRACE"
 ORDER BY logged;
prio	error_code	subsystem	JSON_EXTRACT(data,'$.err_symbol')	JSON_EXTRACT(data,'$.msg')
System	MY-010000	Server	"ER_PARSER_TRACE"	"Parser saw: SET @@session.debug=\"+d,parser_stmt_to_error_log_with_system_prio\""

# NFR-1.2 Component variables
#
# Components may provide component variables. To be able to assign
# values to a component's variable, that component must first be loaded,
# either implicitly by setting log_error_services, or explicitly using
# INSTALL COMPONENT. If a component was explicitly loaded using
# INSTALL COMPONENT, it need not be set active using log_error_services
# to be able to access that component's variables.

# Part 1: Show that the variable provided by the dragnet filtering
#         component is not available because the component has not
#         been loaded. Both statements should fail.
SELECT @@global.dragnet.log_error_filter_rules;
ERROR HY000: Unknown system variable 'dragnet.log_error_filter_rules'
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';
ERROR HY000: Unknown system variable 'dragnet.log_error_filter_rules'

# FR-1.3 INSTALL COMPONENT
#
# The established workflow of persisting what components should be
# added to the server in an InnoDB table, mysql.component, will remain
# available. This is shown below where use INSTALL COMPONENT and
# UNINSTALL COMPONENT. It is further shown in various other
# log_component* test cases.

# FR-1 @@global.log_error_services
#
# Loading a component in this way will not add it to the table,
# mysql.component.

# Show that the JSON component was not explicitly loaded with
# INSTALL COMPONENT, and therefore was not added to mysql.component.
SELECT component_urn FROM mysql.component;
component_urn

# Using INSTALL COMPONENT instead would add it to said table.
INSTALL COMPONENT "file://component_log_filter_dragnet";
# Show that INSTALL COMPONENT added the component to mysql.component.
SELECT component_urn FROM mysql.component;
component_urn
file://component_log_filter_dragnet
# Show that INSTALL COMPONENT did not change @@global.log_error_services:
SELECT @@global.log_error_services;
@@global.log_error_services
log_sink_json

# NFR-1.2 Component variables
#

# Part 2: Show that the variable provided by the dragnet filtering
#         component are now available because the component was
#         loaded. Both statements should succeed.
SELECT @@global.dragnet.log_error_filter_rules;
@@global.dragnet.log_error_filter_rules
IF prio>=INFORMATION THEN drop. IF EXISTS source_line THEN unset source_line.
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';

# UNINSTALL explicitly loaded component.
UNINSTALL COMPONENT "file://component_log_filter_dragnet";
# Prove UNINSTALL COMPONENT removed the component from mysql.component.
SELECT component_urn FROM mysql.component;
component_urn

# UNINSTALL no longer present component. Should fail.
UNINSTALL COMPONENT "file://component_log_filter_dragnet";
ERROR HY000: Component specified by URN 'file://component_log_filter_dragnet' to unload has not been loaded before.

# Part 3: Show that the variable provided by the dragnet filtering
#         component are no longer available because the component
#         been unloaded. Both statements should fail.
SELECT @@global.dragnet.log_error_filter_rules;
ERROR HY000: Unknown system variable 'dragnet.log_error_filter_rules'
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';
ERROR HY000: Unknown system variable 'dragnet.log_error_filter_rules'

# Part 4: When implicit loading with log_error_services is used,
# log_error_services should always be assigned first so the listed
# components are loaded and their variables become available.
# The component variables can then be set afterwards.
# Implicitly load log_filter_dragnet; then get/set its variable.
SET @@global.log_error_services="log_filter_dragnet;log_sink_json";
SELECT @@global.dragnet.log_error_filter_rules;
@@global.dragnet.log_error_filter_rules
IF prio>=INFORMATION THEN drop. IF EXISTS source_line THEN unset source_line.
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';
SHOW VARIABLES LIKE 'dragnet.log_error_filter_rules';
Variable_name	Value
dragnet.log_error_filter_rules	IF err_symbol=="ER_STARTUP" THEN drop.

# Clean up.
SET @@global.log_error_services=DEFAULT;

# Show that log_error_services was reset to internal components:
SELECT @@global.log_error_services;
@@global.log_error_services
log_filter_internal; log_sink_internal
# Show that when we removed the dragnet component from log_error_services,
# we not only set it inactive, but unloaded it as well. This is shown
# indirectly by proving that the component's variables are no longer
# available.
SELECT @@global.dragnet.log_error_filter_rules;
ERROR HY000: Unknown system variable 'dragnet.log_error_filter_rules'
SHOW VARIABLES LIKE 'dragnet.log_error_filter_rules';
Variable_name	Value

# The End
