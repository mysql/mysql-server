# Counts the Large_transaction_header events in all binlog files of the
# server. Each promoted file holds exactly one.
#
# Sets:
#   $bolt_promoted_files  the number of Large_transaction_header events
#
--source include/not_windows.inc
--let $_bcpf_datadir = `SELECT @@DATADIR`
--let $_bcpf_base = `SELECT SUBSTRING_INDEX(@@GLOBAL.log_bin_basename, '/', -1)`
--let $_bcpf_inc = $MYSQLTEST_VARDIR/tmp/bolt_count_promoted_files.inc
--exec $MYSQL_BINLOG --force-if-open $_bcpf_datadir/$_bcpf_base.0* | grep -c 'Large transaction header' | xargs printf -- '--let \044bolt_promoted_files = %s\n' > $_bcpf_inc
--source $_bcpf_inc
--remove_file $_bcpf_inc
