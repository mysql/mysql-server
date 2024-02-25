--source ndb_waiter.inc

--echo Waiting for not started
--exec $_waiter_cmd --not-started  > /dev/null
