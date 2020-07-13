

##
## Table of testing
#
# 1. localhost bindings, skip 127.0.0.1
# 2. "skip-networking" bindings
# 3. "not existing hosts" bindings
# 4. skipped "*" binding
# 5. skipped "0.0.0.0" binding
# 6. skipped "empty address" binding
# 7. skipped "empty network namespace" binding
# 8. skipped "wildcard with namespace" binding
# 9. skipped "wrong network namespace" binding
# 10. skipped "distanthost" binding
# 11. "ifconfig" bindings
# 12. default bindings


#
# 1. localhost bindings, skip 127.0.0.1
call mtr.add_suppression("Plugin mysqlx reported: 'Value '127.0.0.1' set to `Mysqlx_bind_address`, X Plugin can't bind to it. Skipping this value.'");
call mtr.add_suppression("Plugin mysqlx reported: 'Setup of bind-address: '127.0.0.1' port: [0-9]* failed, `bind\\(\\)` failed with error: Address already in use \\([0-9]+\\). Do you already have another mysqld server running with Mysqlx \\?'");
# restart: --mysqlx_bind_address=localhost,127.0.0.1
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to localhost,127.0.0.1]

#
# 2. "skip-networking" bindings
# restart: --skip-networking --mysqlx_bind_address=localhost,distanthost
include/assert.inc [There status variable must point to UNDEFINED]
include/assert.inc [There variable must point to localhost,distanthost]

#
# 3. "not existing hosts" bindings
call mtr.add_suppression("Plugin mysqlx reported: 'Value 'otherhost' set to `Mysqlx_bind_address`, X Plugin can't bind to it. Skipping this value.'");
call mtr.add_suppression("Plugin mysqlx reported: 'Setup of bind-address: 'otherhost' port: [0-9]* failed, can't resolve `hostname`'");
call mtr.add_suppression("Plugin mysqlx reported: 'Value 'anotherhost' set to `Mysqlx_bind_address`, X Plugin can't bind to it. Skipping this value.'");
call mtr.add_suppression("Plugin mysqlx reported: 'Setup of bind-address: 'anotherhost' port: [0-9]* failed, can't resolve `hostname`'");
# restart: --mysqlx_bind_address=otherhost,anotherhost
include/assert.inc [There status variable must point to UNDEFINED]
include/assert.inc [There variable must point to otherhost,anotherhost]

#
# 4. skipped "*" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Validation of value '\\*' set to `Mysqlx_bind_address` failed: wildcards are not allowed when there are more than one address. Skipping this value.'");
# restart: --mysqlx_bind_address=*,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to *,localhost]

#
# 5. skipped "0.0.0.0" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Validation of value '0.0.0.0' set to `Mysqlx_bind_address` failed: wildcards are not allowed when there are more than one address. Skipping this value.'");
# restart: --mysqlx_bind_address=0.0.0.0,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to 0.0.0.0,localhost]

#
# 6. skipped "empty address" binding
call mtr.add_suppression("Validation of value '' set to `Mysqlx_bind_address` failed: wildcards are not allowed when there are more than one address. Skipping this value.'");
# restart: --mysqlx_bind_address=,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to ,localhost]

#
# 7. skipped "empty network namespace" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Validation of value 'localhost/' set to `Mysqlx_bind_address` failed: can't be parsed as an address. Skipping this value.'");
# restart: --mysqlx_bind_address=localhost/,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to localhost/,localhost]

#
# 8. skipped "wildcard with namespace" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Validation of value '\\*/wrong' set to `Mysqlx_bind_address` failed: network namespace are not allowed for wildcards. Skipping this value.'");
# restart: --mysqlx_bind_address=*/wrong,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to */wrong,localhost]

#
# 9. skipped "wrong network namespace" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Value 'localhost/wrong' set to `Mysqlx_bind_address`, X Plugin can't bind to it. Skipping this value.'");
call mtr.add_suppression("Plugin mysqlx reported: 'Setup of bind-address: 'localhost/wrong' port: [0-9]* failed, '");
call mtr.add_suppression("Unknown network namespace 'wrong'");
call mtr.add_suppression("Plugin mysqlx reported: 'Network Namespaces is not supported on this platform'");
# restart: --mysqlx_bind_address=localhost/wrong,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to localhost/wrong,localhost]

#
# 10. skipped "distanthost" binding
call mtr.add_suppression("Plugin mysqlx reported: 'Value 'distanthost' set to `Mysqlx_bind_address`, X Plugin can't bind to it. Skipping this value.'");
call mtr.add_suppression("Plugin mysqlx reported: 'Setup of bind-address: 'distanthost' port: [0-9]* failed, can't resolve `hostname`'");
# restart: --mysqlx_bind_address=distanthost,localhost
include/assert.inc [There status variable must point to 127.0.0.1]
include/assert.inc [There variable must point to distanthost,localhost]

#
# 11. "ifconfig" bindings
# restart: --mysqlx_bind_address=$IPs
SELECT variable_value FROM performance_schema.global_status WHERE variable_name LIKE "Mysqlx_address" INTO @MYSQLX_ADDRESSES;
include/assert.inc [There status variable must point some of $IPs]
include/assert.inc [There variable must point to $IPs]

#
# 12. default bindings
# restart:
include/assert.inc [There status variable must point to ::]
include/assert.inc [There variable must point to *]
DROP TABLE test.ip;
