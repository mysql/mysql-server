#
# Show name of tables whose name starts with "t_special" in NDB
# and all the MySQL Servers
#

# Show name of table in NDB
--disable_query_log ONCE
SELECT CONCAT("'", fq_name, "'") as Name_in_NDB
  FROM ndbinfo.dict_obj_info
    WHERE type = 2 AND fq_name LIKE "%/def/t_special%";

# Show name of table and its engine in DD on all MySQL Servers
let $i = $NUM_MYSQLDS;
while($i)
{
  --connection mysqld$i
  --disable_query_log ONCE
  eval SELECT CONCAT("'", TABLE_SCHEMA, ".", TABLE_NAME,
                     "' - ", ENGINE) AS Name_in_mysqld$i
    FROM information_schema.tables
      WHERE TABLE_NAME LIKE 't_special%';
  dec $i;
}
connection default;
