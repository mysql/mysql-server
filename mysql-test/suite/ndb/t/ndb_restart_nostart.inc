--source ndb_mgm.inc

--echo Restarting all nodes "no start"
--exec $_mgm_cmd -e "ALL RESTART -n" > /dev/null

--source ndb_wait_nostart.inc
