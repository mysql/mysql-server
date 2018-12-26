#   - Check SHOW CREATE statement;

--echo
--echo

SHOW CREATE EVENT ev1|
--echo
SHOW CREATE EVENT ev2|
--echo
SHOW CREATE EVENT mysqltest2.ev3|
--echo
SHOW CREATE EVENT mysqltest2.ev3|

#   - Check SHOW statement;

--echo
--echo

SHOW EVENTS LIKE 'ev1'|

--echo
SHOW EVENTS LIKE 'ev2'|

--echo
SHOW EVENTS LIKE 'ev3'|

--echo
SHOW EVENTS LIKE 'ev4'|

#   - Check INFORMATION_SCHEMA;

--echo
--echo

--replace_column 17 CREATED 18 LAST_ALTERED
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE event_name = 'ev1'|

--echo
--replace_column 17 CREATED 18 LAST_ALTERED
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE event_name = 'ev2'|

--echo
--replace_column 17 CREATED 18 LAST_ALTERED
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE event_name = 'ev3'|

--echo
--replace_column 17 CREATED 18 LAST_ALTERED
SELECT * FROM INFORMATION_SCHEMA.EVENTS WHERE event_name = 'ev4'|
