#
# Delete the user
# Check that upgrade recreates it
#
DELETE FROM mysql.user WHERE user='mysql.infoschema';
COMMIT;
Run mysql_upgrade
# restart:--upgrade=FORCE
include/assert.inc [The mysql.infoschema user should exist after upgrade]
#
# Clean up
#

End of test
