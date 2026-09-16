# internal/mysql-test/suite/thread_pol/inc/thread_pool_err.inc
#
# Purpose:
# There are several tests in the pattern
# thread_pool_<variable name (maybe shortened)>_wrong which check the
# content of mysqld.1.err for specific messages when the test was
# started with invalid values.
#
# Some tests expect the same messages. In order to avoid code
# duplication we store the expected messages here and depend on the
# ./inc/wrong_value.inc to validate that appropriate messages have been
# logged in the error log.

# We get the following error when thread poool fails to load.
--let TP_INIT_ERROR_PATTERN= Plugin 'thread_pool' init function returned error

#---------- thread_pool_algorithm ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-algorithm': value -1 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-algorithm': unsigned value 2 adjusted to 1.

--let $tp_algo_min_warn= option 'thread-pool-algorithm':.*value .* adjusted to 0
--let $tp_algo_max_warn= option 'thread-pool-algorithm':.*value .* adjusted to 1

#---------- thread_pool_longrun_trx_limit ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-longrun-trx-limit': value -1 adjusted to 10.
# [Warning] [MY-000081] [Server] option 'thread-pool-longrun-trx-limit': unsigned value 86400001 adjusted to 86400000.

--let $tp_lrtl_min_warn= option 'thread-pool-longrun-trx-limit':.*value .* adjusted to 10
--let $tp_lrtl_max_warn= option 'thread-pool-longrun-trx-limit':.*value .* adjusted to 86400000

#---------- thread_pool_dedicated_listeners ----------
# Snip of mysqld.1.err
# [Warning] [MY-000076] [Server] option 'thread-pool-dedicated-listeners': boolean value '-1' was not recognized. Set to OFF.
# [Warning] [MY-000076] [Server] option 'thread-pool-dedicated-listeners': boolean value '2' was not recognized. Set to OFF.

--let $tp_dl_min_warn= option 'thread-pool-dedicated-listeners': boolean value .* was not recognized. Set to OFF
--let $tp_dl_max_warn= option 'thread-pool-dedicated-listeners': boolean value .* was not recognized. Set to OFF

#---------- thread_pool_max_active_query_threads ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-max-active-query-threads': value -1 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-max-active-query-threads': unsigned value 513 adjusted to 512.

--let $tp_maqt_min_warn= option 'thread-pool-max-active-query-threads':.*value .* adjusted to 0
--let $tp_maqt_max_warn= option 'thread-pool-max-active-query-threads':.*value .* adjusted to 512

#---------- thread_pool_max_transactions_limit ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-max-transactions-limit': value -1 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-max-transactions-limit': unsigned value 1100000 adjusted to 1000000.

--let $tp_mtl_min_warn= option 'thread-pool-max-transactions-limit':.*value .* adjusted to 0
--let $tp_mtl_max_warn= option 'thread-pool-max-transactions-limit':.*value .* adjusted to 1000000

#---------- thread_pool_max_unused_threads ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-max-unused-threads': value -1 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-max-unused-threads': unsigned value 40970 adjusted to 4096.

--let $tp_mut_min_warn= option 'thread-pool-max-unused-threads':.*value .* adjusted to 0
--let $tp_mut_max_warn= option 'thread-pool-max-unused-threads':.*value .* adjusted to 4096

#---------- thread_pool_priority_kickup_timer ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-prio-kickup-timer': value -12 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-prio-kickup-timer': unsigned value 4294967296 adjusted to 4294967295.

--let $tp_pkt_min_warn= option 'thread-pool-prio-kickup-timer':.*value .* adjusted to 0
--let $tp_pkt_max_warn= option 'thread-pool-prio-kickup-timer':.*value .* adjusted to 4294967295

#---------- thread_pool_query_threads_per_group ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-query-threads-per-group': value -1 adjusted to 1.
# [Warning] [MY-000081] [Server] option 'thread-pool-query-threads-per-group': unsigned value 4097 adjusted to 4096.

--let $tp_qtpg_min_warn= option 'thread-pool-query-threads-per-group':.*value .* adjusted to 1
--let $tp_qtpg_max_warn= option 'thread-pool-query-threads-per-group':.*value .* adjusted to 4096

#---------- thread_pool_size ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-size': value -1 adjusted to 1.
# [Warning] [MY-000081] [Server] option 'thread-pool-size': unsigned value 513 adjusted to 512.
# Note: No tests are currently using $tp_size_max_warn as it can consume more resources.

--let $tp_size_min_warn= option 'thread-pool-size':.*value .* adjusted to 1
--let $tp_size_max_warn= option 'thread-pool-size':.*value .* adjusted to 512

#---------- thread_pool_stall_limit ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-stall-limit': value 3 adjusted to 4.
# [Warning] [MY-000081] [Server] option 'thread-pool-stall-limit': unsigned value 700 adjusted to 600.

--let $tp_sl_min_warn= option 'thread-pool-stall-limit':.*value .* adjusted to 4
--let $tp_sl_max_warn= option 'thread-pool-stall-limit':.*value .* adjusted to 600

#---------- thread_pool_size_transaction_delay value ----------
# Snip of mysqld.1.err
# [Warning] [MY-000082] [Server] option 'thread-pool-transaction-delay': value -1 adjusted to 0.
# [Warning] [MY-000081] [Server] option 'thread-pool-transaction-delay': unsigned value 300001 adjusted to 300000.

--let $tp_trx_delay_min_warn= option 'thread-pool-transaction-delay':.*value .* adjusted to 0
--let $tp_trx_delay_max_warn= option 'thread-pool-transaction-delay':.*value .* adjusted to 300000
