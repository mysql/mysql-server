#
# This test upgrades a 5.7 database with a table containing GEOMETRY columns. This table
# was originally created in 5.6 and then upgraded to 5.7. An additional table containing
# GEOMETRY columns was created in 5.7
#
# Stop the running server
#################################################################################
# Copy and unzip the datadir created with 5.6 version and upgraded to 5.7
#################################################################################
# Restart the server with the unzipped datadir
# restart: --datadir=DATADIR --innodb_page_size=16k
# Check the contents of INFORMATION_SCHEMA
SHOW TABLES;
Tables_in_test
t56_geometry_table
t57_geometry_table
SELECT NAME, ROW_FORMAT, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE name LIKE '%test/t_%'
ORDER BY name;
NAME	ROW_FORMAT	SPACE_TYPE
test/t56_geometry_table	Compact	Single
test/t57_geometry_table	Dynamic	Single
SHOW CREATE TABLE t56_geometry_table;
Table	Create Table
t56_geometry_table	CREATE TABLE `t56_geometry_table` (
  `c56_pk` int NOT NULL DEFAULT '0',
  `c56_geometry` geometry NOT NULL,
  `c56_geometrycollection` geomcollection NOT NULL,
  `c56_linestring` linestring NOT NULL,
  `c56_multilinestring` multilinestring NOT NULL,
  `c56_multipoint` multipoint NOT NULL,
  `c56_multipolygon` multipolygon NOT NULL,
  `c56_point` point NOT NULL,
  `c56_polygon` polygon NOT NULL,
  PRIMARY KEY (`c56_pk`),
  SPATIAL KEY `si56_point` (`c56_point`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE t57_geometry_table;
Table	Create Table
t57_geometry_table	CREATE TABLE `t57_geometry_table` (
  `c57_pk` int NOT NULL,
  `c57_geometry` geometry NOT NULL,
  `c57_geometrycollection` geomcollection NOT NULL,
  `c57_linestring` linestring NOT NULL,
  `c57_multilinestring` multilinestring NOT NULL,
  `c57_point` point NOT NULL,
  `c57_multipoint` multipoint NOT NULL,
  `c57_polygon` polygon NOT NULL,
  `c57_multipolygon` multipolygon NOT NULL,
  PRIMARY KEY (`c57_pk`),
  SPATIAL KEY `si57_point` (`c57_point`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
# Perform some basic operations on the tables
ALTER TABLE t56_geometry_table ADD SPATIAL INDEX si56_geometry (`c56_geometry`);
Warnings:
Warning	3674	The spatial index on column 'c56_geometry' will not be used by the query optimizer since the column does not have an SRID attribute. Consider adding an SRID attribute to the column.
SHOW CREATE TABLE t56_geometry_table;
Table	Create Table
t56_geometry_table	CREATE TABLE `t56_geometry_table` (
  `c56_pk` int NOT NULL DEFAULT '0',
  `c56_geometry` geometry NOT NULL,
  `c56_geometrycollection` geomcollection NOT NULL,
  `c56_linestring` linestring NOT NULL,
  `c56_multilinestring` multilinestring NOT NULL,
  `c56_multipoint` multipoint NOT NULL,
  `c56_multipolygon` multipolygon NOT NULL,
  `c56_point` point NOT NULL,
  `c56_polygon` polygon NOT NULL,
  PRIMARY KEY (`c56_pk`),
  SPATIAL KEY `si56_point` (`c56_point`),
  SPATIAL KEY `si56_geometry` (`c56_geometry`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
ALTER TABLE t57_geometry_table ADD SPATIAL INDEX si57_geometry (`c57_geometry`);
Warnings:
Warning	3674	The spatial index on column 'c57_geometry' will not be used by the query optimizer since the column does not have an SRID attribute. Consider adding an SRID attribute to the column.
SHOW CREATE TABLE t57_geometry_table;
Table	Create Table
t57_geometry_table	CREATE TABLE `t57_geometry_table` (
  `c57_pk` int NOT NULL,
  `c57_geometry` geometry NOT NULL,
  `c57_geometrycollection` geomcollection NOT NULL,
  `c57_linestring` linestring NOT NULL,
  `c57_multilinestring` multilinestring NOT NULL,
  `c57_point` point NOT NULL,
  `c57_multipoint` multipoint NOT NULL,
  `c57_polygon` polygon NOT NULL,
  `c57_multipolygon` multipolygon NOT NULL,
  PRIMARY KEY (`c57_pk`),
  SPATIAL KEY `si57_point` (`c57_point`),
  SPATIAL KEY `si57_geometry` (`c57_geometry`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
ALTER TABLE t56_geometry_table DROP INDEX si56_point;
ALTER TABLE t57_geometry_table DROP INDEX si57_point;
SHOW CREATE TABLE t56_geometry_table;
Table	Create Table
t56_geometry_table	CREATE TABLE `t56_geometry_table` (
  `c56_pk` int NOT NULL DEFAULT '0',
  `c56_geometry` geometry NOT NULL,
  `c56_geometrycollection` geomcollection NOT NULL,
  `c56_linestring` linestring NOT NULL,
  `c56_multilinestring` multilinestring NOT NULL,
  `c56_multipoint` multipoint NOT NULL,
  `c56_multipolygon` multipolygon NOT NULL,
  `c56_point` point NOT NULL,
  `c56_polygon` polygon NOT NULL,
  PRIMARY KEY (`c56_pk`),
  SPATIAL KEY `si56_geometry` (`c56_geometry`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
SHOW CREATE TABLE t57_geometry_table;
Table	Create Table
t57_geometry_table	CREATE TABLE `t57_geometry_table` (
  `c57_pk` int NOT NULL,
  `c57_geometry` geometry NOT NULL,
  `c57_geometrycollection` geomcollection NOT NULL,
  `c57_linestring` linestring NOT NULL,
  `c57_multilinestring` multilinestring NOT NULL,
  `c57_point` point NOT NULL,
  `c57_multipoint` multipoint NOT NULL,
  `c57_polygon` polygon NOT NULL,
  `c57_multipolygon` multipolygon NOT NULL,
  PRIMARY KEY (`c57_pk`),
  SPATIAL KEY `si57_geometry` (`c57_geometry`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1
# Cleanup
# Shutdown server
# Remove copied files
# Restart the server
# restart
