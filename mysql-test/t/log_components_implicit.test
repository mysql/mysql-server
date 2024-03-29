--echo #
--echo # WL#14793: Load log-components such as the JSON-logger earlier (before InnoDB)
--echo #

# Some of the changes implemented by WL#14793 are demonstrated elsewhere,
# either explicitly by new test coverage, or implicitly by the updates to
# existing tests that aligns them with the new behavior:
#
# FR-1.1: see log_options_cmdline, "WL#14793 FR-1" (section 4.2)
# This test sets log_error_services on the command-line at server start-up
# without first adding them to the server using INSTALL COMPONENT. This used
# to fail (because of the requested components not being present); now the
# missing components are implicitly loaded when the variable is set and the
# assignment succeeds.
#
# FR-1.1.1: see log_options_cmdline, "WL#14793 - FR-1.1.1" (section 4.2)
# Existing test was extended to show that loading a logging component
# implictly by assigning log_error_services from the command-line will
# not add it to the components table (as explicitly adding it to the
# server with INSTALL COMPONENT would have done).
#
# FR-1.2: see log_options_cmdline, "WL#14793 - FR-1.2"
# New test coverage shows that "SET PERSIST log_error_services"
# results in the variable's value being restore correctly on start-up,
# and that logging components listed therein are loaded as needed.
# Further shows that component-variables for such implicitly loaded
# components can also be PERSISTed, as long as this happens in order,
# that is to say, as long as log_error_services is persisted before
# component variables are persisted. This is necessary because persisted
# variables are restored in chronological order of assignment. Thus, we
# need to take care to first restore log_error_services (and thereby
# implicitly load components as needed) as components need to be loaded
# for their variables to exist. As long as the component-variables don't
# exist (yet), restoring their values will fail.
#
# FR-2.1: see log_options_cmdline, "WL#14793 - FR-2.1"
# New test coverage shows interaction of explicit and implicit
# component-loading.


# In debug mode, we can echo statements to the error log.
--source include/have_debug.inc
# Without logging components, this is a pointless exercise.
# --source include/have_log_component.inc
# Make sure nobody else logs to performance_schema.error_log.
--source include/not_parallel.inc

SET @@session.time_zone=@@global.log_timestamps;

--echo
--echo # Find (timestamp of) most recent row in performance_schema.error_log.
--echo # This will exclude log entries from other tests or --repeat=...
SELECT FROM_UNIXTIME(VARIABLE_VALUE/1000000)
  INTO @pfs_errlog_latest
  FROM performance_schema.global_status
 WHERE VARIABLE_NAME LIKE "Error_log_latest_write";

--echo
--echo # NFR-1.1 log_error_services
--echo #
--echo # The MySQL server will attempt to implicitly load any loadable
--echo # logging component listed in log_error_services that has not been
--echo # loaded yet. The loading is attempted on assignment of the variable.

--echo
--echo # No logging components were explicitly loaded with INSTALL COMPONENT:
SELECT component_urn FROM mysql.component;

--echo
--echo # Enable logging in the JSON format.
--echo # Since we did not "INSTALL COMPONENT log_sink_json;" (and proved so
--echo # above), this will fail on a server pre-WL#14793 that cannot implicitly
--echo # load error logging components. On a server post WL#14793, log_sink_json
--echo # will be loaded implicitly at this point.
SET @@global.log_error_services="log_sink_json";

--echo
--echo # Start echoing all statements to the error log.
SET @@session.debug="+d,parser_stmt_to_error_log_with_system_prio";

--echo
--echo # Stop echoing all statements to the error log.
SET @@session.debug="-d,parser_stmt_to_error_log_with_system_prio";

--echo
--echo # Extract a key/value pair from a JSON record.
--echo # This will show that JSON logging is now active, and by implication,
--echo # that the JSON component was implicitly loaded.
SELECT prio,error_code,subsystem,
       JSON_EXTRACT(data,'$.err_symbol'),JSON_EXTRACT(data,'$.msg')
  FROM performance_schema.error_log
 WHERE logged>@pfs_errlog_latest
   AND LEFT(data,1)='{'
   AND JSON_EXTRACT(data,'$.err_symbol')="ER_PARSER_TRACE"
 ORDER BY logged;

--echo
--echo # NFR-1.2 Component variables
--echo #
--echo # Components may provide component variables. To be able to assign
--echo # values to a component's variable, that component must first be loaded,
--echo # either implicitly by setting log_error_services, or explicitly using
--echo # INSTALL COMPONENT. If a component was explicitly loaded using
--echo # INSTALL COMPONENT, it need not be set active using log_error_services
--echo # to be able to access that component's variables.

--echo
--echo # Part 1: Show that the variable provided by the dragnet filtering
--echo #         component is not available because the component has not
--echo #         been loaded. Both statements should fail.
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@global.dragnet.log_error_filter_rules;
--error ER_UNKNOWN_SYSTEM_VARIABLE
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';

--echo
--echo # FR-1.3 INSTALL COMPONENT
--echo #
--echo # The established workflow of persisting what components should be
--echo # added to the server in an InnoDB table, mysql.component, will remain
--echo # available. This is shown below where use INSTALL COMPONENT and
--echo # UNINSTALL COMPONENT. It is further shown in various other
--echo # log_component* test cases.

--echo
--echo # FR-1 @@global.log_error_services
--echo #
--echo # Loading a component in this way will not add it to the table,
--echo # mysql.component.

--echo
--echo # Show that the JSON component was not explicitly loaded with
--echo # INSTALL COMPONENT, and therefore was not added to mysql.component.
SELECT component_urn FROM mysql.component;


--echo
--echo # Using INSTALL COMPONENT instead would add it to said table.
INSTALL COMPONENT "file://component_log_filter_dragnet";
--echo # Show that INSTALL COMPONENT added the component to mysql.component.
SELECT component_urn FROM mysql.component;
--echo # Show that INSTALL COMPONENT did not change @@global.log_error_services:
SELECT @@global.log_error_services;

--echo
--echo # NFR-1.2 Component variables
--echo #

--echo
--echo # Part 2: Show that the variable provided by the dragnet filtering
--echo #         component are now available because the component was
--echo #         loaded. Both statements should succeed.
SELECT @@global.dragnet.log_error_filter_rules;
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';

--echo
--echo # UNINSTALL explicitly loaded component.
UNINSTALL COMPONENT "file://component_log_filter_dragnet";

--echo # Prove UNINSTALL COMPONENT removed the component from mysql.component.
SELECT component_urn FROM mysql.component;

--echo
--echo # UNINSTALL no longer present component. Should fail.
--error ER_COMPONENTS_UNLOAD_NOT_LOADED
UNINSTALL COMPONENT "file://component_log_filter_dragnet";

--echo
--echo # Part 3: Show that the variable provided by the dragnet filtering
--echo #         component are no longer available because the component
--echo #         been unloaded. Both statements should fail.
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@global.dragnet.log_error_filter_rules;
--error ER_UNKNOWN_SYSTEM_VARIABLE
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';

--echo
--echo # Part 4: When implicit loading with log_error_services is used,
--echo # log_error_services should always be assigned first so the listed
--echo # components are loaded and their variables become available.
--echo # The component variables can then be set afterwards.
--echo # Implicitly load log_filter_dragnet; then get/set its variable.
SET @@global.log_error_services="log_filter_dragnet;log_sink_json";
SELECT @@global.dragnet.log_error_filter_rules;
SET @@global.dragnet.log_error_filter_rules='IF err_symbol=="ER_STARTUP" THEN drop.';
SHOW VARIABLES LIKE 'dragnet.log_error_filter_rules';

--echo
--echo # Clean up.
SET @@global.log_error_services=DEFAULT;

--echo
--echo # Show that log_error_services was reset to internal components:
SELECT @@global.log_error_services;

--echo # Show that when we removed the dragnet component from log_error_services,
--echo # we not only set it inactive, but unloaded it as well. This is shown
--echo # indirectly by proving that the component's variables are no longer
--echo # available.
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@global.dragnet.log_error_filter_rules;
SHOW VARIABLES LIKE 'dragnet.log_error_filter_rules';

--echo
--echo # The End
