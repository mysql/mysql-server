# -----------------------------------------------------
# Schema basic_schema
# -----------------------------------------------------
# Create schema that contains each basic MRS object type
--disable_query_log
--disable_result_log
DROP SCHEMA IF EXISTS `paging_schema` ;

--let $router_test_schema=paging_schema
CREATE SCHEMA IF NOT EXISTS `paging_schema`;
USE `paging_schema`;

CREATE TABLE IF NOT EXISTS `paging_schema`.`short10` (
  `id` INTEGER NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `paging_schema`.`medium25` (
  `id` INTEGER NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));

CREATE TABLE IF NOT EXISTS `paging_schema`.`long101` (
  `id` INTEGER NOT NULL AUTO_INCREMENT, comment VARCHAR(255),
  PRIMARY KEY (`id`));


SET @str="This is some string, that is going to be rolled in the `comment` column of the table";
SET @len=LENGTH(@str);
INSERT INTO short10(id, comment)
  WITH RECURSIVE cte( id, comment ) AS (
    SELECT 1 as id, SUBSTR(@str,1,10) as comment
    UNION ALL
    SELECT id + 1, SUBSTR(@str, (id) MOD (@len -1) +1, 10) FROM cte WHERE id<10
  )
  SELECT * FROM cte;

INSERT INTO medium25(id, comment)
  WITH RECURSIVE cte( id, comment ) AS (
    SELECT 1 as id, SUBSTR(@str,1,10) as comment
    UNION ALL
    SELECT id + 1, SUBSTR(@str, (id) MOD (@len -1) +1, 10) FROM cte WHERE id<25
  )
  SELECT * FROM cte;

INSERT INTO long101(id, comment)
  WITH RECURSIVE cte( id, comment ) AS (
    SELECT 1 as id, SUBSTR(@str,1,10) as comment
    UNION ALL
    SELECT id + 1, SUBSTR(@str, (id) MOD (@len -1) +1, 10) FROM cte WHERE id<101
  )
  SELECT * FROM cte;


--enable_query_log
--enable_result_log

--echo # DB `paging_schema` - created

