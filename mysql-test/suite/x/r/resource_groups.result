#
# BUG#30900411 - RESOURCE GROUP MANAGEMENT STATEMENTS ARE NOT SUPPORTED BY PLUGGABLE PROTOCOLS
# 
# All resource group related SQL fails with error before the fix for bug#30900411.
RUN CREATE RESOURCE GROUP rg1 TYPE=USER VCPU=1-2

0 rows affected
RUN ALTER RESOURCE GROUP rg1 VCPU=1

0 rows affected
RUN SELECT /*+ RESOURCE_GROUP(rg1) */ 1
1
1
0 rows affected
RUN SET RESOURCE GROUP rg1

0 rows affected
RUN SET RESOURCE GROUP USR_default

0 rows affected
RUN DROP RESOURCE GROUP rg1

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
