# Remove number output from "last_altered" value
--let $p1=/("last_altered": )[0-9]+/\1NNN/

# Remove number output from "created" value
--let $p2=/("created": )[0-9]+/\1NNN/

# Remove se_private_data: id & trx_id output. retain the root page number value
--let $p3=/("se_private_data":) "id=[0-9]+;root=[0-9]+;trx_id=[0-9]+;"/\1 "id=X;root=Y;trx_id=Z"/

# Remove se_private_id: id. This is innodb table_id
--let $p4=/("se_private_id":) [0-9]+/\1NNN/

# Remove object_id output
--let $p5=/("id": )[0-9]+/\1X/

# Remove file_per_tablespace id output in dd::Table
--let $p6=/("tablespace_ref": "innodb_file_per_table.)[0-9]+"/\1X"/

# Remove file_per_tablespace id output in dd::Tablespace
--let p7=/("name": "innodb_file_per_table.)[0-9]+"/\1X"/

# Remove id output in se_prvate_data of dd::Tablespace
--let $p8=/("se_private_data": ")(discard=[0-1];)*(flags=)([0-9]+)(;id=)[0-9]+;"/\1\3X\5Y;"/

# Remove mysql version id
--let $p9=/("mysqld_version_id": )[0-9]+/\1X/

# Remove extra path separator seen on windows and partition names
--let $p10=/\/\//\// /#P#/#p#/ /#SP#/#sp#/

--let $ibd2sdi_pattern=$p1 $p2 $p3 $p4 $p5 $p6 $p7 $p8 $p9 $p10

# Remove root also for tables in System tablespace
--let $p3_1=/("se_private_data":) "id=[0-9]+;root=[0-9]+;trx_id=[0-9]+;"/\1 "id=X;root=Y;trx_id=Z"/

--let $ibd2sdi_pattern_system=$p1 $p2 $p3_1 $p4 $p5 $p6 $p7 $p8 $p9 $p10

# Remove collation_id only for dictionary tables
--let $p11=/("collation_id": )[0-9]+/\1X/

--let $ibd2sdi_pattern_mysql= $ibd2sdi_pattern $p11
