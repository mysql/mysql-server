--replace_result $SLAVE_MYPORT SLAVE_PORT $DEFAULT_MASTER_PORT DEFAULT_PORT
--replace_regex /[0-9, a-f]{8}-[0-9, a-f]{4}-[0-9, a-f]{4}-[0-9, a-f]{4}-[0-9, a-f]{12}/SLAVE_UUID/
SHOW SLAVE HOSTS;
