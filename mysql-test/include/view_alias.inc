# Routine to be called by t/view.inc
#
# The variable $after_select must be set before calling this routine.

eval CREATE VIEW v1 AS SELECT $after_select;
SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 'v1' ORDER BY COLUMN_NAME;
#
# Extract the VIEW's SELECT from INFORMATION_SCHEMA.VIEWS
let $query1 = `SELECT VIEW_DEFINITION FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 'v1'`;
#
# Extract the VIEW's SELECT from SHOW CREATE VIEW
# SHOW CREATE VIEW v1
# View    Create View     character_set_client    collation_connection
# v1      CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v1` AS select '<--- .....
let $value= query_get_value(SHOW CREATE VIEW v1, Create View, 1);
let $query2 = `SELECT SUBSTR("$value",INSTR("$value",' as select ') + CHAR_LENGTH(' as '))`;
DROP VIEW v1;

# Recreate the view based on SELECT from INFORMATION_SCHEMA.VIEWS
eval CREATE VIEW v1 AS $query1;
DROP VIEW v1;
# Recreate the view based on SHOW CREATE VIEW
eval CREATE VIEW v1 AS $query2;
DROP VIEW v1;

