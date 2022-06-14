#
# A) Test code handling virtual columns in mysqldump tool.
#
SET timestamp=1;
CREATE DATABASE test1;
use test1;
CREATE TABLE t1 (a1 INTEGER,
a2 INTEGER GENERATED ALWAYS AS (a1 * 2) STORED,
a3 INTEGER GENERATED ALWAYS AS (a1 * 3) VIRTUAL,
KEY (a1) );
DROP TABLE IF EXISTS `t1`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t1` (
  `a1` int DEFAULT NULL,
  `a2` int GENERATED ALWAYS AS ((`a1` * 2)) STORED,
  `a3` int GENERATED ALWAYS AS ((`a1` * 3)) VIRTUAL,
  KEY `a1` (`a1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
<?xml version="1.0"?>
<mysqldump xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
<database name="test1">
DROP TABLE IF EXISTS `t1`;
	<table_structure name="t1">
		<field Field="a1" Type="int" Null="YES" Key="MUL" Extra="" Comment="" />
		<key Table="t1" Non_unique="1" Key_name="a1" Seq_in_index="1" Column_name="a1" Collation="A" Cardinality="0" Null="YES" Index_type="BTREE" Comment="" Index_comment="" Visible="YES" />
		<options Name="t1" Engine="InnoDB" Version="10" Row_format="Dynamic" Rows="0" Avg_row_length="0" Data_length="16384" Max_data_length="0" Index_length="16384" Data_free="0" Create_time="1970-01-01 00:00:01" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
</database>
</mysqldump>
DROP TABLE t1;
DROP DATABASE test1;
