# To be used with WL#4445: EXCHANGE PARTITION WITH TABLE.
SET SESSION debug="+d,exchange_partition_abort_1";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_1";
SET SESSION debug="+d,exchange_partition_abort_2";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_2";
SET SESSION debug="+d,exchange_partition_abort_3";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_3";
SET SESSION debug="+d,exchange_partition_abort_4";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_4";
SET SESSION debug="+d,exchange_partition_abort_5";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_5";
SET SESSION debug="+d,exchange_partition_abort_6";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_6";
SET SESSION debug="+d,exchange_partition_abort_7";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_7";
SET SESSION debug="+d,exchange_partition_abort_8";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_8";
SET SESSION debug="+d,exchange_partition_abort_9";
--source suite/parts/inc/partition_crash_t2.inc
SET SESSION debug="-d,exchange_partition_abort_9";
