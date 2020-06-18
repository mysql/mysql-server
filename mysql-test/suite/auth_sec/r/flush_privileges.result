#
# Bug#31491261: ENUM_SP_TYPE TO_SP_TYPE(LONGLONG):
#               ASSERTION `VAL >= 1 && VAL <= 4' FAILED
#
# Setup
CREATE USER u31491261;
CREATE DATABASE db31491261;
CREATE PROCEDURE db31491261.p31491261() SELECT 1;
GRANT EXECUTE ON PROCEDURE db31491261.p31491261 TO u31491261;
CALL mtr.add_suppression("'procs_priv' entry 'p31491261' ignored, bad routine type");
# FLUSH PRIVILEGES should succeed
FLUSH PRIVILEGES;
# SHOW GRANTS should show EXECUTE privilege on db31491261.p31491261
SHOW GRANTS FOR 'u31491261';
Grants for u31491261@%
GRANT USAGE ON *.* TO `u31491261`@`%`
GRANT EXECUTE ON PROCEDURE `db31491261`.`p31491261` TO `u31491261`@`%`
UPDATE mysql.procs_priv SET Routine_type = '0' WHERE User like 'u31491261';
# FLUSH PRIVILEGES should succeed
FLUSH PRIVILEGES;
# Search for: 'procs_priv' entry 'p31491261' ignored, bad routine type
Pattern "'procs_priv' entry 'p31491261' ignored, bad routine type" found
# Search completed.
# SHOW GRANTS should not show EXECUTE privilege on db31491261.p31491261
SHOW GRANTS FOR 'u31491261';
Grants for u31491261@%
GRANT USAGE ON *.* TO `u31491261`@`%`
UPDATE mysql.procs_priv SET Routine_type = 'PROCEDURE' WHERE User like 'u31491261';
# FLUSH PRIVILEGES should succeed
FLUSH PRIVILEGES;
# SHOW GRANTS should show EXECUTE privilege on db31491261.p31491261
SHOW GRANTS FOR 'u31491261';
Grants for u31491261@%
GRANT USAGE ON *.* TO `u31491261`@`%`
GRANT EXECUTE ON PROCEDURE `db31491261`.`p31491261` TO `u31491261`@`%`
# Cleanup
DROP DATABASE db31491261;
DROP USER u31491261;
# End of 8.0 tests
