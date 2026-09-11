# The AHI hash table cell count and node heap usage are not exposed through a
# status variable or Performance Schema. SHOW ENGINE INNODB STATUS is the only
# SQL-visible source for these values, so extract both from its Status text.
--let $AHI_STATUS=query_get_value(SHOW ENGINE INNODB STATUS, Status, 1)
--let $AHI_SIZE_VALUE=`SELECT CAST(SUBSTRING_INDEX(SUBSTRING_INDEX("$AHI_STATUS", "Hash table size ", -1), ",", 1) AS UNSIGNED)`
--let $AHI_NODE_HEAP_USED=`SELECT REGEXP_LIKE("$AHI_STATUS", "node heap has [1-9][0-9]* buffer[(]s[)]")`
