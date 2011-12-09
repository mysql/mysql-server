--source ndb_mgm.inc

--echo Starting all nodes
--exec $_mgm_cmd -e "ALL START"  > /dev/null

--source ndb_wait_started.inc
