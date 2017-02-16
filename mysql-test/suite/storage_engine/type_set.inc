#
# SET column type
#

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

# Valid values.

let $create_definition =
  a SET('') $col_opts,
  b SET('test1','test2','test3','test4','test5') $col_opts,
  c SET('01','02','03','04','05','06','07','08','09','10','11','12','13','14','15','16','17','18','19','20','21','22','23','24','25','26','27','28','29','30','31','32','33','34','35','36','37','38','39','40','41','42','43','44','45','46','47','48','49','50''51','52','53','54','55','56','57','58','59','60','61','62','63','64') $col_opts
;
--source create_table.inc
if ($mysql_errname)
{
  --let $my_last_stmt = $create_statement
  --let $functionality = SET types
  --source unexpected_result.inc
}
if (!$mysql_errname)
{
  --replace_column 3 # 4 # 5 #
  SHOW COLUMNS IN t1;

  INSERT INTO t1 (a,b,c) VALUES 
  ('','test2,test3','01,34,44,,23'),
  ('',5,2),
  (',','test4,test2','');
  --sorted_result
  SELECT a,b,c FROM t1;

  # Out of range values 
  # (should produce warnings)

  INSERT INTO t1 (a,b,c) VALUES (0,'test6',-1);
  --sorted_result
  SELECT a,b,c FROM t1;

  # Non-unique values in set
  # (should produce a warning)
  --let $alter_definition = ADD COLUMN e SET('a','A') $col_opts
  --source alter_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $alter_statement
    --let $functionality = ALTER TABLE
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --replace_column 3 # 4 # 5 #
    SHOW COLUMNS IN t1;

    --let $error_codes = ER_TOO_BIG_SET
    --let $alter_definition = ADD COLUMN f SET('1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',' ','11','12','13','14','15','16','17','18','19','1a','1b','1c','1d','1e','1f','1g','1h','1i','1j','1k','1l','1m','1n','1o','1p','1q','1r','1s','1t','1u','1v','1w','1x','1y','1z','20','21','22','23','24','25','26','27','28','29','2a','2b','2c','2d','2e','2f','2g','2h','2i','2j','2k','2l','2m','2n','2o','2p','2q','2r','2s','2t','2u','2v','2w','2x','2y','2z','30','31','32','33','34','35','36','37','38','39','3a','3b','3c','3d','3e','3f','3g','3h','3i') $col_opts
    --source alter_table.inc
  }

  # Simple comparison

  --sorted_result
  SELECT a,b,c,e FROM t1 WHERE FIND_IN_SET('test2',b)>0 OR a != '';

  DROP TABLE t1;
}

