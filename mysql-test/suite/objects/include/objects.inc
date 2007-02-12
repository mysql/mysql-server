# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             objects.inc                            |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

#
# Here we set the desired storage engine.
#
eval SET SESSION storage_engine = $engine_type;

# Verify that setting storage engine worked.
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
CREATE TABLE t1 (a int);
SHOW CREATE TABLE t1;

# Final cleanup.
DROP TABLE t1;

#
# Create the objects.
#
--disable_query_log
--disable_warnings
SET sql_mode = "ANSI_QUOTES,NO_BACKSLASH_ESCAPES,TRADITIONAL,IGNORE_SPACE";
DROP SCHEMA IF EXISTS OBJECTS_TEST;
--enable_warnings
CREATE SCHEMA OBJECTS_TEST;
USE OBJECTS_TEST;

--source suite/objects/include/tbl01.inc
--source suite/objects/include/tbl02.inc
--source suite/objects/include/tbl03.inc
--source suite/objects/include/tbl04.inc
--source suite/objects/include/tbl05.inc
--source suite/objects/include/tbl06.inc
--source suite/objects/include/tbl07.inc
--source suite/objects/include/tbl08.inc
--source suite/objects/include/tbl09.inc
--source suite/objects/include/tbl10.inc
--source suite/objects/include/tbl11.inc
--source suite/objects/include/tbl12.inc
--source suite/objects/include/tbl13.inc
--source suite/objects/include/tbl14.inc
--source suite/objects/include/tbl15.inc
--source suite/objects/include/tbl16.inc
--source suite/objects/include/tbl17.inc
--source suite/objects/include/tbl18.inc
--source suite/objects/include/tbl19.inc
--source suite/objects/include/tbl20.inc
--source suite/objects/include/tbl21.inc
--source suite/objects/include/tbl22.inc
--source suite/objects/include/tbl23.inc
--source suite/objects/include/tbl24.inc
--source suite/objects/include/tbl25.inc
--source suite/objects/include/tbl26.inc
--source suite/objects/include/tbl27.inc
--source suite/objects/include/tbl28.inc
--source suite/objects/include/tbl29.inc
--source suite/objects/include/tbl30.inc
--source suite/objects/include/tbl31.inc
--source suite/objects/include/tbl32.inc
--source suite/objects/include/tbl33.inc
--source suite/objects/include/tbl34.inc
--source suite/objects/include/tbl35.inc
--source suite/objects/include/tbl36.inc
--source suite/objects/include/tbl37.inc
--source suite/objects/include/tbl38.inc
--source suite/objects/include/tbl39.inc
--source suite/objects/include/tbl40.inc
--source suite/objects/include/tbl41.inc
--source suite/objects/include/tbl42.inc
--source suite/objects/include/tbl43.inc
--source suite/objects/include/tbl44.inc
--source suite/objects/include/tbl45.inc
--source suite/objects/include/tbl46.inc
--source suite/objects/include/tbl47.inc
--source suite/objects/include/tbl48.inc
--source suite/objects/include/view.inc

--enable_query_log

SHOW TABLES;
SHOW CREATE TABLE AAABLQUS;

--disable_query_log
if ($drop_database)
{
    --source suite/objects/include/drop_all.inc
}
--enable_query_log

