#
--source include/resource_group_init.inc
--source include/have_debug.inc

--echo #
--echo # Bug#34748973 - CPU AFFINITY IS NOT DROPPED ON RESOURCE GROUP DROP
--echo #

CREATE RESOURCE GROUP rg1 TYPE=USER VCPU=0;
SET RESOURCE GROUP rg1;
SET DEBUG='+d,make_sure_cpu_affinity_is_dropped';
--echo # Without fix, following statement fails.
DROP RESOURCE GROUP rg1 FORCE;
SET DEBUG='-d,make_sure_cpu_affinity_is_dropped';

