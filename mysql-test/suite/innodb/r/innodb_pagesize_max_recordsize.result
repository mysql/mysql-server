# Stop DB server which was created by MTR default
# create bootstrap file
# Run the bootstrap command with page size 32k
# Start the DB server with page size 32k
# restart: --innodb_page_size=32K --datadir=MYSQLD_DATADIR1
SET innodb_strict_mode=ON;
SELECT @@innodb_page_size;
@@innodb_page_size
32768
SELECT @@Innodb_file_per_table;
@@Innodb_file_per_table
1
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT= COMPACT charset latin1;
ERROR 42000: Row size too large (> 16318). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT= DYNAMIC;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is 65535. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
SELECT @@innodb_file_per_table;
@@innodb_file_per_table
1
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=COMPRESSED charset latin1;
ERROR HY000: Table storage engine for 'tab5' doesn't have this option
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=REDUNDANT;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is 65535. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=FIXED charset latin1;
ERROR HY000: Table storage engine for 'tab5' doesn't have this option
SET innodb_strict_mode=OFF;
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=COMPACT charset latin1;
Warnings:
Warning	139	Row size too large (> NNNN). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
DROP TABLE tab5;
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=DYNAMIC charset latin1;
Warnings:
Warning	139	Row size too large (> 16318). Changing some columns to TEXT or BLOB may help. In current row format, BLOB prefix of 0 bytes is stored inline.
DROP TABLE tab5;
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ROW_FORMAT=COMPRESSED charset latin1;
Warnings:
Warning	1478	InnoDB: Cannot create a COMPRESSED table when innodb_page_size > 16k. Assuming ROW_FORMAT=DYNAMIC.
Warning	139	Row size too large (> 16318). Changing some columns to TEXT or BLOB may help. In current row format, BLOB prefix of 0 bytes is stored inline.
DROP TABLE tab5;
# Stop DB server with page size 32k
# Check Server startup failure when page size 64K
Pattern "\[Warning\] .*MY-\d+.* innodb-page-size has been changed from the default value 16384 to 32768" found
# Restart the DB server with datadir1 with same 32k page size
# restart: --innodb_page_size=32K --datadir=MYSQLD_DATADIR1
SELECT @@innodb_page_size;
@@innodb_page_size
32768
# Stop DB server which was created by page size 32k
# Run the bootstrap command with page size 64k
# Start the DB server with page size 64K
# restart: --innodb_page_size=64K --datadir=MYSQLD_DATADIR1
SELECT @@innodb_page_size;
@@innodb_page_size
65536
SET innodb_strict_mode=ON;
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255), col65  CHAR(255))
ENGINE = innodb ;
ERROR 42000: Row size too large. The maximum row size for the used table type, not counting BLOBs, is 65535. This includes storage overhead, check the manual. You have to change some columns to TEXT or BLOBs
CREATE TABLE tab5(col1 CHAR (255), col2 CHAR (255), col3 CHAR(255),col4 CHAR(255), col5 CHAR(255),
col6   CHAR(255), col7 CHAR(255),  col8 CHAR(255), col9 CHAR(255),col10 CHAR(255), col11  CHAR(255),
col12 CHAR(255), col13 CHAR(255),col14 CHAR(255),col15 CHAR(255),col16  CHAR(255), col17 CHAR(255),
col18 CHAR(255),col19 CHAR(255),col20 CHAR(255),col21 CHAR(255),col22  CHAR(255), col23 CHAR(255),
col24 CHAR(255),col25 CHAR(255),col26 CHAR(255),col27 CHAR(255),col28  CHAR(255), col29 CHAR(255),
col30 CHAR(255),col31 CHAR(255),col32 CHAR(255),col33 CHAR(255),col34  CHAR(255), col35 CHAR(255),
col36 CHAR(255),col37 CHAR(255),col38 CHAR(255),col39 CHAR(255),col40  CHAR(255), col41 CHAR(255),
col42 CHAR(255),col43 CHAR(255),col44 CHAR(255),col45 CHAR(255),col46  CHAR(255), col47 CHAR(255),
col48 CHAR(255),col49 CHAR(255),col50 CHAR(255),col51 CHAR(255),col52  CHAR(255), col53 CHAR(255),
col54 CHAR(255),col55 CHAR(255),col56 CHAR(255),col57 CHAR(255),col58  CHAR(255), col59 CHAR(255),
col60 CHAR(255),col61 CHAR(255),col62 CHAR(255),col63 CHAR(255),col64  CHAR(255)) ENGINE = innodb ;
DROP TABLE tab5;
# restart
