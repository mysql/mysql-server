#   - Check SHOW CREATE statement;

--echo
--echo

SHOW CREATE VIEW v1|

--echo

SHOW CREATE VIEW v2|

--echo

SHOW CREATE VIEW v3|

#   - Check INFORMATION_SCHEMA;

--echo
--echo

SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE table_name = 'v1'|

--echo

SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE table_name = 'v2'|

--echo

SELECT * FROM INFORMATION_SCHEMA.VIEWS WHERE table_name = 'v3'|

#   - Execute the views;

--echo
--echo

SELECT COLLATION(c1), COLLATION(c2) FROM v1|

--echo

SELECT COLLATION(c1) FROM v2|

--echo

SELECT * FROM v3|
