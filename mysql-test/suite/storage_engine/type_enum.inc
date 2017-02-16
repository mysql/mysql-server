#
# ENUM column type
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

# Valid values.
# We cannot test the maximum of 65,536 here,
# because mysqltest has the standard limit of MAX_QUERY=256K;
# but we can at least try 257

let $create_definition = 
  a ENUM('') $col_opts,
  b ENUM('test1','test2','test3','test4','test5') $col_opts,
  c ENUM('1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',' ','11','12','13','14','15','16','17','18','19','1a','1b','1c','1d','1e','1f','1g','1h','1i','1j','1k','1l','1m','1n','1o','1p','1q','1r','1s','1t','1u','1v','1w','1x','1y','1z','20','21','22','23','24','25','26','27','28','29','2a','2b','2c','2d','2e','2f','2g','2h','2i','2j','2k','2l','2m','2n','2o','2p','2q','2r','2s','2t','2u','2v','2w','2x','2y','2z','30','31','32','33','34','35','36','37','38','39','3a','3b','3c','3d','3e','3f','3g','3h','3i','3j','3k','3l','3m','3n','3o','3p','3q','3r','3s','3t','3u','3v','3w','3x','3y','3z','40','41','42','43','44','45','46','47','48','49','4a','4b','4c','4d','4e','4f','4g','4h','4i','4j','4k','4l','4m','4n','4o','4p','4q','4r','4s','4t','4u','4v','4w','4x','4y','4z','50','51','52','53','54','55','56','57','58','59','5a','5b','5c','5d','5e','5f','5g','5h','5i','5j','5k','5l','5m','5n','5o','5p','5q','5r','5s','5t','5u','5v','5w','5x','5y','5z','60','61','62','63','64','65','66','67','68','69','6a','6b','6c','6d','6e','6f','6g','6h','6i','6j','6k','6l','6m','6n','6o','6p','6q','6r','6s','6t','6u','6v','6w','6x','6y','6z','70','71','72','73','74','75') $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = ENUM type
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  INSERT INTO t1 (a,b,c) VALUES ('','test2','4'),('',5,2);
  --sorted_result
  SELECT a,b,c FROM t1;

  # Out of range values 
  # (should produce warnings)

  INSERT INTO t1 (a,b,c) VALUES (0,'test6',-1);
  --sorted_result
  SELECT a,b,c FROM t1;

  # Non-unique values in enum
  # (should produce a warning)
  --let $alter_definition = ADD COLUMN e ENUM('a','A') $col_opts
  --source alter_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }

  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  INSERT INTO t1 (a,b,c,e) VALUES ('','test3','75','A');
  --sorted_result
  SELECT a,b,c,e FROM t1;

  # Simple comparison

  --sorted_result
  SELECT a,b,c,e FROM t1 WHERE b='test2' OR a != '';

  DROP TABLE t1;
}



