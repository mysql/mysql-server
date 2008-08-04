#############################################################
#
# Author:  Serge Kozlov <skozlov@mysql.com>
# Date:    03/11/2008
# Purpose: Sync all hosts for circular replication based on 
# schema A->B->C->D->A
# 
# Notes: see include/circular_rpl_for_4_hosts_init.inc
#  
#############################################################

# Make the full loop of sync
--connection master_a
--disable_query_log
--sync_slave_with_master master_b
--sync_slave_with_master master_c
--sync_slave_with_master master_d
--sync_slave_with_master master_a
--sync_slave_with_master master_b
--sync_slave_with_master master_c
--save_master_pos
--connection master_a
--enable_query_log
