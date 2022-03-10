## Test: Upgrade data directory.
#   a) start MySQLd with 5.7 data directory, do the upgrade.
#   b) verify that X Plugin "information about acceptin client
#      connections (logs) are printed after upgrade finishes.

## a) Restart the server with 5.7 data directory.
# Copy and unzip the datadir.
# Mark that error-logs generated until now are not important.
include/assert_error_log_sequence.inc [server: 1, pattern: SAVEPOS]
# Restart the server with "5.7" data directory.
# restart: --datadir=DATADIR

## b) Confirm that X Plugin "accepting" logs are printed after upgrade.
include/assert_error_log_sequence.inc [server: 1, pattern: System.*Server upgrade from '50700' to '800..' started.
System.*Server upgrade from '50700' to '800..' completed.
System.*X Plugin ready for connections. Bind-address: .* port: ]

## Cleanup: restart the server and restore the default database.
# restart:
