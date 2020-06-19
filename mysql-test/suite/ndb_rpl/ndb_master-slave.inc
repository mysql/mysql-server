# NDB setups don't allow for RESET SLAVE ALL to be executed as of now
# This relates to BUG#31515760 and should be re-evaluated after its fix
# This flag is also used on other include files and NDB tests.
--let $rpl_shallow_reset_slave= 1
--source include/master-slave.inc
