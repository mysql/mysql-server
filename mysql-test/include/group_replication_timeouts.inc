#
# Resets the default timeouts for inc files
#

# wait_condition.inc
#
# Set to 60 by default instead of 30.
#
#
--let $wait_timeout= 60
--let $explicit_default_wait_timeout= 60

# wait_for_slave...
#
# Set to 60 instead of 30
#
--let $slave_timeout= 60
