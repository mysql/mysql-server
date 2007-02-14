# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl12.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAJPVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJPVP" varchar(22) NOT NULL default ' ',
  "AAABFTQU" varchar(4) NOT NULL default '0000',
  "AAABJTNV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJPVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJPWF" varchar(12) NOT NULL default '000000000000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJPWF"),
  KEY "AAAAJPWA" ("AAABMRIL","AAABLUSC","AAABLUPE","AAABDHBD","AAAAJPWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJPWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABMLEL" int(11) NOT NULL default '0',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHCE" varchar(3) NOT NULL default '000',
  "AAABDHCD" varchar(10) NOT NULL default '0000000000',
  "AAABDHBE" varchar(3) NOT NULL default ' ',
  "AAABDHBG" varchar(3) NOT NULL default ' ',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDHBM" varchar(1) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTN","AAABLDEN","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQHM" (
  "AAAAJQHM" varchar(120) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAATMBK" varchar(1) NOT NULL default ' ',
  "AAAAJQFG" varchar(210) NOT NULL default ' ',
  "AAAAJQLN" varchar(25) NOT NULL default ' ',
  "AAAAJQLO" varchar(1) NOT NULL default ' ',
  "AAAAJQLP" varchar(3) NOT NULL default ' ',
  "AAAAJQLQ" varchar(12) NOT NULL default ' ',
  "AAAAJQLR" varchar(64) NOT NULL default ' ',
  "AAAAJQLS" varchar(1) NOT NULL default ' ',
  "AAAAJQLT" varchar(1) NOT NULL default ' ',
  "AAAAJQLU" varchar(1) NOT NULL default ' ',
  "AAAAJQLV" varchar(1) NOT NULL default ' ',
  "AAAAJQLW" varchar(1) NOT NULL default ' ',
  "AAAAJQLX" varchar(1) NOT NULL default ' ',
  "AAAAJQLY" varchar(8) NOT NULL default ' ',
  "AAAAJQLZ" varchar(20) NOT NULL default ' ',
  "AAAAJQMA" varchar(1) NOT NULL default ' ',
  "AAAAJQMB" varchar(1) NOT NULL default ' ',
  "AAAAJQMC" varchar(1) NOT NULL default ' ',
  "AAAAJQMD" varchar(1) NOT NULL default ' ',
  "AAAAJQME" int(11) NOT NULL default '0',
  "AAAAJQMF" varchar(255) NOT NULL default ' ',
  "AAAAJQMG" varchar(6) NOT NULL default '000000',
  "AAAAJQMH" varchar(12) NOT NULL default ' ',
  "AAAAJQMI" varchar(8) NOT NULL default '00000000',
  "AAAAJQMJ" varchar(12) NOT NULL default ' ',
  "AAAAJQMK" varchar(8) NOT NULL default '00000000',
  "AAAAJQML" varchar(1) NOT NULL default ' ',
  "AAAAJQMM" varchar(1) NOT NULL default '0',
  "AAAAJQMN" varchar(32) NOT NULL default ' ',
  "AAAAJQMO" varchar(32) NOT NULL default ' ',
  "AAAAJQMP" varchar(200) NOT NULL default ' ',
  "AAAAJQMQ" varchar(1) NOT NULL default ' ',
  "AAAAJQMR" varchar(1) NOT NULL default ' ',
  "AAAAJQMS" varchar(32) NOT NULL default ' ',
  "AAAAJQMT" varchar(32) NOT NULL default ' ',
  "AAAAJQMU" varchar(200) NOT NULL default ' ',
  "AAAAJQMV" varchar(1) NOT NULL default ' ',
  "AAAAJQMW" varchar(1) NOT NULL default ' ',
  "AAAAJQMX" varchar(1) NOT NULL default ' ',
  "AAAAJQMY" varchar(8) NOT NULL default ' ',
  "AAAAJQMZ" varchar(1) NOT NULL default ' ',
  "AAAAJQNA" varchar(1) NOT NULL default ' ',
  "AAAAJQNB" varchar(1) NOT NULL default ' ',
  "AAAAJQNC" varchar(1) NOT NULL default ' ',
  "AAAAJQND" varchar(120) NOT NULL default ' ',
  "AAAAJQNE" varchar(32) NOT NULL default ' ',
  "AAAAJQNF" varchar(32) NOT NULL default ' ',
  "AAAAJQNG" varchar(200) NOT NULL default ' ',
  "AAAAJQNH" varchar(1) NOT NULL default ' ',
  "AAAAJQNI" varchar(1) NOT NULL default ' ',
  "AAAAJQNJ" varchar(1) NOT NULL default ' ',
  "AAAAJQNK" varchar(3) NOT NULL default '000',
  "AAAAJQNL" varchar(3) NOT NULL default '000',
  "AAAAJQNM" varchar(32) NOT NULL default ' ',
  "AAAAJQNN" varchar(32) NOT NULL default ' ',
  "AAAAJQNO" varchar(200) NOT NULL default ' ',
  "AAAAJQNP" varchar(1) NOT NULL default ' ',
  "AAAAJQNQ" varchar(1) NOT NULL default ' ',
  "AAAAJQNR" varchar(1) NOT NULL default ' ',
  "AAAAJQOD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQHM","AAAAJQOM","AAAATMBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJPZP" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQHM" varchar(120) NOT NULL default ' ',
  "AAAAJPZU" longblob NOT NULL,
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM","AAAAJQNY","AAAAJQHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJPZZ" (
  "AAABLECD" varchar(24) NOT NULL default ' ',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  "AAAAJQOJ" int(11) NOT NULL default '0',
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAABLHAH" varchar(200) NOT NULL default ' ',
  "AAABCODC" varchar(1) NOT NULL default ' ',
  "AAAAJQAH" varchar(12) NOT NULL default ' ',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAJQGY" varchar(3) NOT NULL default '000',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABBOOU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLECD","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQAQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAJQBN" varchar(15) NOT NULL default ' ',
  "AAABLEEW" varchar(1) NOT NULL default ' ',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAABLHAH" varchar(120) NOT NULL default ' ',
  "AAAAJQAX" varchar(12) NOT NULL default ' ',
  "AAAAJQAY" varchar(32) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAAAVNMQ" varchar(6) NOT NULL default '000000',
  "AAAAJQBC" varchar(64) NOT NULL default ' ',
  "AAAAJQBD" varchar(1) NOT NULL default ' ',
  "AAAAJQBE" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQBF" varchar(6) NOT NULL default '000000',
  "AAAAJQBG" varchar(30) NOT NULL default ' ',
  "AAAAJQBH" varchar(1) NOT NULL default ' ',
  "AAAAJQBI" varchar(12) NOT NULL default ' ',
  "AAAAJQBJ" varchar(8) NOT NULL default ' ',
  "AAAAJQBK" longtext,
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAAAJQBN","AAABLEEW","AAABLITM","AAABLHAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQBR" (
  "AAABLHAH" varchar(116) NOT NULL default ' ',
  "AAAAJQER" varchar(121) NOT NULL default ' ',
  "AAABAVXT" varchar(3) NOT NULL default '000',
  "AAAAJQBV" varchar(40) NOT NULL default ' ',
  "AAAAJQBW" varchar(15) NOT NULL default ' ',
  "AAAAJQBX" varchar(1) NOT NULL default ' ',
  "AAAAJQBY" varchar(255) NOT NULL default ' ',
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQLJ" varchar(1) NOT NULL default ' ',
  "AAAAJQLK" smallint(6) NOT NULL default '0',
  "AAAAJQLL" smallint(6) NOT NULL default '0',
  "AAAAJQLM" varchar(1) NOT NULL default ' ',
  "AAAAJQLN" varchar(25) NOT NULL default ' ',
  "AAAAJQLO" varchar(1) NOT NULL default ' ',
  "AAAAJQLP" varchar(3) NOT NULL default ' ',
  "AAAAJQLQ" varchar(12) NOT NULL default ' ',
  "AAAAJQLR" varchar(64) NOT NULL default ' ',
  "AAAAJQLS" varchar(1) NOT NULL default ' ',
  "AAAAJQLT" varchar(1) NOT NULL default ' ',
  "AAAAJQLU" varchar(1) NOT NULL default ' ',
  "AAAAJQLV" varchar(1) NOT NULL default ' ',
  "AAAAJQLW" varchar(1) NOT NULL default ' ',
  "AAAAJQLX" varchar(1) NOT NULL default ' ',
  "AAAAJQLY" varchar(8) NOT NULL default ' ',
  "AAAAJQLZ" varchar(20) NOT NULL default ' ',
  "AAAAJQMA" varchar(1) NOT NULL default ' ',
  "AAAAJQMB" varchar(1) NOT NULL default ' ',
  "AAAAJQMC" varchar(1) NOT NULL default ' ',
  "AAAAJQMD" varchar(1) NOT NULL default ' ',
  "AAAAJQME" int(11) NOT NULL default '0',
  "AAAAJQMF" varchar(255) NOT NULL default ' ',
  "AAAAJQMG" varchar(6) NOT NULL default '000000',
  "AAAAJQMH" varchar(12) NOT NULL default ' ',
  "AAAAJQMI" varchar(8) NOT NULL default '00000000',
  "AAAAJQMJ" varchar(12) NOT NULL default ' ',
  "AAAAJQMK" varchar(8) NOT NULL default '00000000',
  "AAAAJQML" varchar(1) NOT NULL default ' ',
  "AAAAJQMM" varchar(1) NOT NULL default '0',
  "AAAAJQMN" varchar(32) NOT NULL default ' ',
  "AAAAJQMO" varchar(32) NOT NULL default ' ',
  "AAAAJQMP" varchar(200) NOT NULL default ' ',
  "AAAAJQMQ" varchar(1) NOT NULL default ' ',
  "AAAAJQMR" varchar(1) NOT NULL default ' ',
  "AAAAJQMS" varchar(32) NOT NULL default ' ',
  "AAAAJQMT" varchar(32) NOT NULL default ' ',
  "AAAAJQMU" varchar(200) NOT NULL default ' ',
  "AAAAJQMV" varchar(1) NOT NULL default ' ',
  "AAAAJQMW" varchar(1) NOT NULL default ' ',
  "AAAAJQMX" varchar(1) NOT NULL default ' ',
  "AAAAJQMY" varchar(8) NOT NULL default ' ',
  "AAAAJQMZ" varchar(1) NOT NULL default ' ',
  "AAAAJQNA" varchar(1) NOT NULL default ' ',
  "AAAAJQNB" varchar(1) NOT NULL default ' ',
  "AAAAJQNC" varchar(1) NOT NULL default ' ',
  "AAAAJQND" varchar(120) NOT NULL default ' ',
  "AAAAJQNE" varchar(32) NOT NULL default ' ',
  "AAAAJQNF" varchar(32) NOT NULL default ' ',
  "AAAAJQNG" varchar(200) NOT NULL default ' ',
  "AAAAJQNH" varchar(1) NOT NULL default ' ',
  "AAAAJQNI" varchar(1) NOT NULL default ' ',
  "AAAAJQNJ" varchar(1) NOT NULL default ' ',
  "AAAAJQNK" varchar(3) NOT NULL default '000',
  "AAAAJQNL" varchar(3) NOT NULL default '000',
  "AAAAJQNM" varchar(32) NOT NULL default ' ',
  "AAAAJQNN" varchar(32) NOT NULL default ' ',
  "AAAAJQNO" varchar(200) NOT NULL default ' ',
  "AAAAJQNP" varchar(1) NOT NULL default ' ',
  "AAAAJQNQ" varchar(1) NOT NULL default ' ',
  "AAAAJQNR" varchar(1) NOT NULL default ' ',
  "AAAAJQNS" varchar(15) NOT NULL default ' ',
  "AAAAJQEM" varchar(120) NOT NULL default ' ',
  "AAAAJQNT" varchar(120) NOT NULL default ' ',
  "AAAAJQNU" varchar(120) NOT NULL default ' ',
  "AAAAJQEP" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHAH","AAAAJQER","AAABAVXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQET" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQGJ" varchar(2) NOT NULL default ' ',
  "AAAAJQLN" varchar(25) NOT NULL default ' ',
  "AAAAJQGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQFC" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQLS" varchar(1) NOT NULL default ' ',
  "AAAAJQFG" varchar(210) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM","AAAAJQLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQFZ" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQHO" varchar(2) NOT NULL default '00',
  "AAAAJQGE" varchar(1) NOT NULL default ' ',
  "AAAAJQFZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM","AAAAJQHO","AAAAJQGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQFU" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQHO" varchar(2) NOT NULL default '00',
  "AAAAJQGE" varchar(1) NOT NULL default ' ',
  "AAAAJQFZ" varchar(32) NOT NULL default ' ',
  "AAAAJQII" varchar(116) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM","AAAAJQHO","AAAAJQGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQGF" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQGJ" varchar(2) NOT NULL default ' ',
  "AAAAJQLN" varchar(25) NOT NULL default ' ',
  "AAAAJQGL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQGO" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLECD" varchar(24) NOT NULL default ' ',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  "AAABLQHQ" varchar(200) NOT NULL default ' ',
  "AAABJZUZ" varchar(1) NOT NULL default ' ',
  "AAAAJQOJ" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(60) NOT NULL default ' ',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAJQGY" varchar(3) NOT NULL default '000',
  "AAABLPSU" varchar(120) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLECD","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQHE" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQHM" varchar(120) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQHO" varchar(2) NOT NULL default '00',
  "AAAAJQHP" varchar(30) NOT NULL default ' ',
  "AAAAJQHK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQHM","AAAAJQOM","AAAAJQHO","AAAAJQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQHQ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(55) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQIA" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQMH" varchar(12) NOT NULL default ' ',
  "AAAAJQMI" varchar(8) NOT NULL default '00000000',
  "AAAAJQMJ" varchar(12) NOT NULL default ' ',
  "AAAAJQMK" varchar(8) NOT NULL default '00000000',
  "AAAAJQII" varchar(116) NOT NULL default ' ',
  "AAAAJQIJ" varchar(1) NOT NULL default ' ',
  "AAAAJQIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQIN" (
  "AAAAJQKT" varchar(3) NOT NULL default '000',
  "AAABGFYY" varchar(32) NOT NULL default ' ',
  "AAABIXBL" varchar(1) NOT NULL default ' ',
  "AAAAJQKX" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(8) NOT NULL default ' ',
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  "AAAAJQIU" varchar(32) NOT NULL default ' ',
  "AAAAJQIV" varchar(12) NOT NULL default ' ',
  "AAAAJQIW" varchar(1) NOT NULL default ' ',
  "AAAAJQIX" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAJQIY" int(11) NOT NULL default '0',
  "AAAAJQIZ" varchar(1) NOT NULL default ' ',
  "AAAAJQJA" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAJQJB" varchar(1) NOT NULL default ' ',
  "AAAAJQJC" varchar(64) NOT NULL default ' ',
  "AAAAJQJD" varchar(32) NOT NULL default ' ',
  "AAAAJQJE" varchar(3) NOT NULL default '000',
  "AAAAJQJF" varchar(12) NOT NULL default ' ',
  "AAAAJQJG" varchar(1) NOT NULL default ' ',
  "AAAAJQJH" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAJQLC" varchar(32) NOT NULL default ' ',
  "AAAAJQJJ" int(11) NOT NULL default '0',
  "AAAAJQLE" varchar(32) NOT NULL default ' ',
  "AAAAJQLA" varchar(32) NOT NULL default ' ',
  "AAAAJQJM" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAAAJQJO" int(11) NOT NULL default '0',
  "AAAAMYUI" varchar(1) NOT NULL default ' ',
  "AAAAJQJQ" varchar(1) NOT NULL default ' ',
  "AAAAJQJR" varchar(128) NOT NULL default ' ',
  "AAAAJQJS" varchar(32) NOT NULL default ' ',
  "AAAAJQJT" varchar(250) NOT NULL default ' ',
  "AAAAJQJU" int(11) NOT NULL default '0',
  "AAABHEYG" longblob,
  "AAAAJQJW" varchar(240) NOT NULL default ' ',
  "AAAAVQRH" longblob,
  "AAAAJQJY" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLQJP" longblob,
  "AAAAJQKA" varchar(12) NOT NULL default ' ',
  "AAAAJQKB" varchar(3) NOT NULL default '000',
  "AAAAJQKC" varchar(12) NOT NULL default ' ',
  "AAAAJQKD" varchar(32) NOT NULL default ' ',
  "AAAAJQKE" varchar(32) NOT NULL default ' ',
  "AAAAJQKF" varchar(32) NOT NULL default ' ',
  "AAAAJQKG" varchar(32) NOT NULL default ' ',
  "AAAAJQKH" varchar(32) NOT NULL default ' ',
  "AAAAJQKI" varchar(1) NOT NULL default ' ',
  "AAAAJQKJ" varchar(64) NOT NULL default ' ',
  "AAAAJQKK" varchar(1) NOT NULL default ' ',
  "AAAAJQKL" varchar(1) NOT NULL default ' ',
  "AAABIJRN" int(11) NOT NULL default '0',
  "AAAAJQKN" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAJQKO" varchar(8) NOT NULL default ' ',
  "AAAAJQKP" varchar(3) NOT NULL default '000',
  "AAAAJQKQ" varchar(12) NOT NULL default ' ',
  "AAAAJQKR" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAJQKS" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQKT","AAABGFYY","AAABIXBL"),
  KEY "AAAAJQKW" ("AAAAJQKX","AAAAJQLA"),
  KEY "AAAAJQKZ" ("AAAAJQLA"),
  KEY "AAAAJQLB" ("AAAAJQLC"),
  KEY "AAAAJQLD" ("AAAAJQLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQLF" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQNY" varchar(25) NOT NULL default ' ',
  "AAAAJQLJ" varchar(1) NOT NULL default ' ',
  "AAAAJQLK" smallint(6) NOT NULL default '0',
  "AAAAJQLL" smallint(6) NOT NULL default '0',
  "AAAAJQLM" varchar(1) NOT NULL default ' ',
  "AAAAJQLN" varchar(25) NOT NULL default ' ',
  "AAAAJQLO" varchar(1) NOT NULL default ' ',
  "AAAAJQLP" varchar(3) NOT NULL default ' ',
  "AAAAJQLQ" varchar(12) NOT NULL default ' ',
  "AAAAJQLR" varchar(64) NOT NULL default ' ',
  "AAAAJQLS" varchar(1) NOT NULL default ' ',
  "AAAAJQLT" varchar(1) NOT NULL default ' ',
  "AAAAJQLU" varchar(1) NOT NULL default ' ',
  "AAAAJQLV" varchar(1) NOT NULL default ' ',
  "AAAAJQLW" varchar(1) NOT NULL default ' ',
  "AAAAJQLX" varchar(1) NOT NULL default ' ',
  "AAAAJQLY" varchar(8) NOT NULL default ' ',
  "AAAAJQLZ" varchar(20) NOT NULL default ' ',
  "AAAAJQMA" varchar(1) NOT NULL default ' ',
  "AAAAJQMB" varchar(1) NOT NULL default ' ',
  "AAAAJQMC" varchar(1) NOT NULL default ' ',
  "AAAAJQMD" varchar(1) NOT NULL default ' ',
  "AAAAJQME" int(11) NOT NULL default '0',
  "AAAAJQMF" varchar(255) NOT NULL default ' ',
  "AAAAJQMG" varchar(6) NOT NULL default '000000',
  "AAAAJQMH" varchar(12) NOT NULL default ' ',
  "AAAAJQMI" varchar(8) NOT NULL default '00000000',
  "AAAAJQMJ" varchar(12) NOT NULL default ' ',
  "AAAAJQMK" varchar(8) NOT NULL default '00000000',
  "AAAAJQML" varchar(1) NOT NULL default ' ',
  "AAAAJQMM" varchar(1) NOT NULL default '0',
  "AAAAJQMN" varchar(32) NOT NULL default ' ',
  "AAAAJQMO" varchar(32) NOT NULL default ' ',
  "AAAAJQMP" varchar(200) NOT NULL default ' ',
  "AAAAJQMQ" varchar(1) NOT NULL default ' ',
  "AAAAJQMR" varchar(1) NOT NULL default ' ',
  "AAAAJQMS" varchar(32) NOT NULL default ' ',
  "AAAAJQMT" varchar(32) NOT NULL default ' ',
  "AAAAJQMU" varchar(200) NOT NULL default ' ',
  "AAAAJQMV" varchar(1) NOT NULL default ' ',
  "AAAAJQMW" varchar(1) NOT NULL default ' ',
  "AAAAJQMX" varchar(1) NOT NULL default ' ',
  "AAAAJQMY" varchar(8) NOT NULL default ' ',
  "AAAAJQMZ" varchar(1) NOT NULL default ' ',
  "AAAAJQNA" varchar(1) NOT NULL default ' ',
  "AAAAJQNB" varchar(1) NOT NULL default ' ',
  "AAAAJQNC" varchar(1) NOT NULL default ' ',
  "AAAAJQND" varchar(120) NOT NULL default ' ',
  "AAAAJQNE" varchar(32) NOT NULL default ' ',
  "AAAAJQNF" varchar(32) NOT NULL default ' ',
  "AAAAJQNG" varchar(200) NOT NULL default ' ',
  "AAAAJQNH" varchar(1) NOT NULL default ' ',
  "AAAAJQNI" varchar(1) NOT NULL default ' ',
  "AAAAJQNJ" varchar(1) NOT NULL default ' ',
  "AAAAJQNK" varchar(3) NOT NULL default '000',
  "AAAAJQNL" varchar(3) NOT NULL default '000',
  "AAAAJQNM" varchar(32) NOT NULL default ' ',
  "AAAAJQNN" varchar(32) NOT NULL default ' ',
  "AAAAJQNO" varchar(200) NOT NULL default ' ',
  "AAAAJQNP" varchar(1) NOT NULL default ' ',
  "AAAAJQNQ" varchar(1) NOT NULL default ' ',
  "AAAAJQNR" varchar(1) NOT NULL default ' ',
  "AAAAJQNS" varchar(15) NOT NULL default ' ',
  "AAAAJQNT" varchar(120) NOT NULL default ' ',
  "AAAAJQNU" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM"),
  KEY "AAAAJQNX" ("AAAAJQNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQNZ" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQOC" varchar(1) NOT NULL default ' ',
  "AAAAJQOD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQOG" (
  "AAAAJQOL" varchar(15) NOT NULL default ' ',
  "AAAAJQOM" varchar(25) NOT NULL default ' ',
  "AAAAJQOJ" int(11) NOT NULL default '0',
  "AAABJRGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQOL","AAAAJQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQON" (
  "AAAAJQQZ" varchar(30) NOT NULL default ' ',
  "AAAAJQRA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBKIK" varchar(1) NOT NULL default ' ',
  "AAABLROU" VARBINARY(255),
  "AAAAJQOT" varchar(14) NOT NULL default ' ',
  "AAAARAYP" varchar(14) NOT NULL default ' ',
  "AAAAJQOV" varchar(14) NOT NULL default ' ',
  "AAAAVBIK" VARBINARY(255),
  "AAABKMUF" VARBINARY(255),
  "AAAAJQOY" VARBINARY(255),
  "AAABKMUE" VARBINARY(255),
  "AAAAJQPA" VARBINARY(255),
  "AAABJCAA" varchar(1) NOT NULL default ' ',
  "AAABIGOB" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAAAJQPE" VARBINARY(255),
  "AAAAJQPF" VARBINARY(255),
  "AAABIKRS" VARBINARY(255),
  "AAABIKRT" VARBINARY(255),
  "AAABIKRU" VARBINARY(255),
  "AAABIKRV" VARBINARY(255),
  "AAABDQKU" varchar(1) NOT NULL default ' ',
  "AAAAJQPL" VARBINARY(255),
  "AAABIIZZ" VARBINARY(255),
  "AAAAJQPN" VARBINARY(255),
  "AAAAJQPO" VARBINARY(255),
  "AAAAJQPP" VARBINARY(255),
  "AAAAJQPQ" VARBINARY(255),
  "AAABLSDL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJQQZ","AAAAJQRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQPU" (
  "AAAAJQQZ" varchar(30) NOT NULL default ' ',
  "AAAAJQRA" varchar(1) NOT NULL default ' ',
  "AAAAJQRB" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABILRT" VARBINARY(255),
  "AAABJHGJ" VARBINARY(255),
  "AAABJHGK" VARBINARY(255),
  "AAABIKRS" VARBINARY(255),
  "AAABIKRT" VARBINARY(255),
  "AAAAJQQE" VARBINARY(255),
  "AAABLRJL" VARBINARY(255),
  "AAABHAGF" VARBINARY(255),
  "AAAAJQQH" VARBINARY(255),
  "AAAAJQQI" VARBINARY(255),
  "AAAAJQQJ" VARBINARY(255),
  "AAABMNMM" VARBINARY(255),
  "AAABHWBK" VARBINARY(255),
  "AAAAJQQM" VARBINARY(255),
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAJQQP" VARBINARY(255),
  "AAABDZZY" varchar(4) NOT NULL default ' ',
  "AAAAJQQR" varchar(1) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABHCYR" VARBINARY(255),
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAABIHRK" VARBINARY(255),
  PRIMARY KEY ("AAAAJQQZ","AAAAJQRA","AAAAJQRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQRC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJBGU" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAAAJQRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTXO" (
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABANOI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQRO" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABKNBI" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABHUHO" varchar(1) NOT NULL default ' ',
  "AAABLEJX" varchar(8) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAAAJQRV" varchar(1) NOT NULL default '0',
  "AAAAJQRW" varchar(1) NOT NULL default '0',
  "AAAAJQRX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQRZ" (
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABIGDH" VARBINARY(255),
  "AAABDOOL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQSE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABLFZO" varchar(30) NOT NULL default ' ',
  "AAABIFON" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQSL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(1) NOT NULL default '0',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQSR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(1) NOT NULL default '0',
  "AAABLOZE" varchar(1) NOT NULL default '0',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQSZ" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABAZVX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLTXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABFQZT" varchar(20) NOT NULL default ' ',
  "AAABFLMZ" varchar(40) NOT NULL default ' ',
  "AAABMECK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAJQTY" varchar(3) NOT NULL default '000',
  "AAAAJQTZ" varchar(1) NOT NULL default ' ',
  "AAAAJQUA" varchar(3) NOT NULL default '000',
  "AAAAJQUB" varchar(1) NOT NULL default ' ',
  "AAAAJQUC" decimal(17,2) NOT NULL default '0.00',
  "AAAAJQUD" varchar(5) NOT NULL default ' ',
  "AAAAJQUE" decimal(17,2) NOT NULL default '0.00',
  "AAAAJQUF" varchar(5) NOT NULL default ' ',
  "AAAAJQUG" varchar(1) NOT NULL default ' ',
  "AAAAJQUH" varchar(1) NOT NULL default ' ',
  "AAAAJQUI" varchar(1) NOT NULL default ' ',
  "AAAAJQUJ" varchar(1) NOT NULL default ' ',
  "AAAAJQUK" varchar(1) NOT NULL default ' ',
  "AAAAJQUL" varchar(1) NOT NULL default ' ',
  "AAAAJQUM" varchar(1) NOT NULL default ' ',
  "AAAAJQUN" varchar(1) NOT NULL default ' ',
  "AAAAJQUO" varchar(1) NOT NULL default ' ',
  "AAAAJQUP" varchar(1) NOT NULL default ' ',
  "AAAAJQUQ" varchar(1) NOT NULL default ' ',
  "AAAAJQUR" varchar(1) NOT NULL default ' ',
  "AAAAJQUS" varchar(1) NOT NULL default ' ',
  "AAAAJQUT" varchar(1) NOT NULL default ' ',
  "AAAAJQUU" varchar(1) NOT NULL default ' ',
  "AAAAJQUV" varchar(1) NOT NULL default ' ',
  "AAAAJQUW" varchar(1) NOT NULL default ' ',
  "AAAAJQUX" varchar(1) NOT NULL default ' ',
  "AAAAJQUY" varchar(1) NOT NULL default ' ',
  "AAAAJQUZ" varchar(1) NOT NULL default ' ',
  "AAAAJQVA" varchar(1) NOT NULL default ' ',
  "AAAAJQVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABBNIW" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH","AAABMDBB","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJQXC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAJQXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJQXC" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJQWP" varchar(20) NOT NULL default ' ',
  "AAAAOXKO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJQXC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJQXC" varchar(3) NOT NULL default '000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKRFR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJQXC","AAABMOAH","AAABMDEN","AAABMREX","AAABKRFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMCMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAAAJRCI" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAJULX" varchar(1) NOT NULL default ' ',
  "AAABLQCR" varchar(27) NOT NULL default ' ',
  "AAAAPVRL" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJRCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMCRA","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAAAJQYZ" ("AAABLQCR","AAAAPVRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMCRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJQZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABFJGM" varchar(8) NOT NULL default '00000000',
  "AAAAMIPQ" varchar(6) NOT NULL default '000000',
  "AAAAJQZQ" varchar(8) NOT NULL default '00000000',
  "AAABHJLP" varchar(8) NOT NULL default '00000000',
  "AAABHJLO" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLXUD" varchar(8) NOT NULL default '00000000',
  "AAAAJQZV" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJQZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMCRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAAAJRCI" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAJULX" varchar(1) NOT NULL default ' ',
  "AAABLQCR" varchar(27) NOT NULL default ' ',
  "AAAAPVRL" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJRCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMCRA","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAAAJRBK" ("AAABLQCR","AAAAPVRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAAAJRCI" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAJULX" varchar(1) NOT NULL default ' ',
  "AAABLQCR" varchar(27) NOT NULL default ' ',
  "AAAAPVRL" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJRCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU","AAABMCRA","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAAAJRCW" ("AAABLQCR","AAAAPVRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRHM" varchar(4) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAAJRER" varchar(20) NOT NULL default ' ',
  "AAAAJRES" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAJRDI" decimal(13,2) NOT NULL default '0.00',
  "AAABCIYI" decimal(13,2) NOT NULL default '0.00',
  "AAAAJRDK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAJRHM","AAABLTXE","AAABMPWL","AAABLARI","AAAAJRER","AAAAJRES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRHM" varchar(4) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAAAJRHF" varchar(4) NOT NULL default ' ',
  "AAAAJRDX" varchar(1) NOT NULL default ' ',
  "AAAAJRDY" varchar(30) NOT NULL default ' ',
  "AAAAJRDZ" varchar(3) NOT NULL default '000',
  "AAAAJREA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRHM","AAABLTXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJREE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRHM" varchar(4) NOT NULL default ' ',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAAJRER" varchar(20) NOT NULL default ' ',
  "AAAAJRES" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABIKEM" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAJRHM","AAABLTXE","AAABLARI","AAAAJRER","AAAAJRES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAAJRHC" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAJRHE" varchar(4) NOT NULL default ' ',
  "AAAAJRFL" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAAAJRHM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAAAJRHC","AAABLCPX","AAAAJRHE","AAAAJRFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAAJRHC" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAJRHE" varchar(4) NOT NULL default ' ',
  "AAAAJRGV" varchar(30) NOT NULL default ' ',
  "AAAAJRGU" varchar(30) NOT NULL default ' ',
  "AAAAJRFW" varchar(30) NOT NULL default ' ',
  "AAAAJRGX" varchar(30) NOT NULL default ' ',
  "AAAAJRFY" varchar(30) NOT NULL default ' ',
  "AAAAJRFZ" varchar(30) NOT NULL default ' ',
  "AAAAJRGA" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAAAJRHC","AAABLCPX","AAAAJRHE","AAAAJRGV","AAAAJRGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAAAJRHC" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAJRHE" varchar(4) NOT NULL default ' ',
  "AAAAJRHF" varchar(4) NOT NULL default ' ',
  "AAAAJRHG" varchar(30) NOT NULL default ' ',
  "AAAAJRGU" varchar(30) NOT NULL default ' ',
  "AAAAJRGV" varchar(30) NOT NULL default ' ',
  "AAABLDUZ" varchar(4) NOT NULL default ' ',
  "AAAAJRGX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO","AAAAJRHC","AAABLCPX","AAAAJRHE","AAAAJRHF","AAAAJRHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRHM" varchar(4) NOT NULL default ' ',
  "AAAATGRY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRIA" varchar(8) NOT NULL default ' ',
  "AAAAJRHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRIA" varchar(8) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRIA","AAABLEHK","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRLQ" varchar(4) NOT NULL default ' ',
  "AAAAJRKO" varchar(2) NOT NULL default ' ',
  "AAAAJRKW" varchar(8) NOT NULL default '00000000',
  "AAAAJRKX" varchar(8) NOT NULL default '00000000',
  "AAAAJRIK" varchar(8) NOT NULL default '00000000',
  "AAAAJRIL" varchar(8) NOT NULL default '00000000',
  "AAAAJRIM" varchar(8) NOT NULL default '00000000',
  "AAAAJRIN" varchar(1) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJRLQ","AAAAJRKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRLQ" varchar(4) NOT NULL default ' ',
  "AAAAJRKO" varchar(2) NOT NULL default ' ',
  "AAAAJRIZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAAAJRLQ","AAAAJRKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAJRKU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAJRKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAJRKU" varchar(2) NOT NULL default ' ',
  "AAAAJRJR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAJRKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRLQ" varchar(4) NOT NULL default ' ',
  "AAAAJRKO" varchar(2) NOT NULL default ' ',
  "AAABKGOR" varchar(8) NOT NULL default '00000000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAAAJRKD" varchar(10) NOT NULL default ' ',
  "AAABBHAZ" varchar(10) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAAAJRKG" varchar(8) NOT NULL default '00000000',
  "AAAAJRMG" varchar(4) NOT NULL default '0000',
  "AAAAJRKI" varchar(12) NOT NULL default ' ',
  "AAAAJRKJ" varchar(8) NOT NULL default '00000000',
  "AAAAJRKK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJRLQ","AAAAJRKO","AAABKGOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRLQ" varchar(4) NOT NULL default ' ',
  "AAAAJRKU" varchar(2) NOT NULL default ' ',
  "AAAAJRLG" varchar(10) NOT NULL default ' ',
  "AAAAJRKW" varchar(8) NOT NULL default '00000000',
  "AAAAJRKX" varchar(8) NOT NULL default '00000000',
  "AAAAPOSO" varchar(8) NOT NULL default '00000000',
  "AAABBOFU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJRLQ"),
  KEY "AAAAJRLD" ("AAABMRIL","AAABMREX","AAAAJRLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRLQ" varchar(4) NOT NULL default ' ',
  "AAAAJRLM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAAAJRLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRMG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJRMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJRMG" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRMD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAJRMG","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRMI" (
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABLHAH" varchar(200) NOT NULL default ' ',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAJRMS" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRMX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRPU" varchar(4) NOT NULL default '0000',
  "AAAAJRPV" varchar(1) NOT NULL default ' ',
  "AAAAJRNT" varchar(3) NOT NULL default '000',
  "AAAAJRPW" varchar(14) NOT NULL default '00000000000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABJGOK" varchar(8) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRPU","AAAAJRPV","AAAAJRNT","AAAAJRPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAAJRPU" varchar(4) NOT NULL default '0000',
  "AAAAJRPV" varchar(1) NOT NULL default ' ',
  "AAAAJRPW" varchar(14) NOT NULL default '00000000000000',
  "AAAAJRPY" varchar(14) NOT NULL default '00000000000000',
  "AAAAVRZW" varchar(4) NOT NULL default ' ',
  "AAAAWIER" varchar(8) NOT NULL default ' ',
  "AAAAJROG" varchar(40) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAJROI" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLQTM" varchar(8) NOT NULL default '00000000',
  "AAABLZMW" varchar(3) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWJLN" varchar(3) NOT NULL default '000',
  "AAAAVSEX" varchar(8) NOT NULL default '00000000',
  "AAAAVSCR" varchar(1) NOT NULL default ' ',
  "AAAAVRYO" varchar(8) NOT NULL default '00000000',
  "AAAAWJKS" varchar(8) NOT NULL default ' ',
  "AAAAWJKJ" varchar(4) NOT NULL default ' ',
  "AAAAJROU" decimal(16,14) NOT NULL default '0.00000000000000',
  "AAAAJROV" decimal(16,14) NOT NULL default '0.00000000000000',
  "AAAAJROW" varchar(5) NOT NULL default ' ',
  "AAAAJROX" varchar(10) NOT NULL default ' ',
  "AAAAJROY" varchar(30) NOT NULL default ' ',
  "AAAAWJLK" decimal(4,3) NOT NULL default '0.000',
  "AAAAJRPA" decimal(4,3) NOT NULL default '0.000',
  "AAAAJRPB" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPC" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPD" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPE" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPF" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPG" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPH" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPI" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPJ" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPK" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPL" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPM" decimal(16,2) NOT NULL default '0.00',
  "AAAAJRPN" varchar(1) NOT NULL default ' ',
  "AAAAJRPO" varchar(1) NOT NULL default ' ',
  "AAAAJRPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAAJRPU","AAAAJRPV","AAAAJRPW"),
  KEY "AAAAJRPX" ("AAAAJRPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAJRQD" varchar(1) NOT NULL default ' ',
  "AAAAJRQE" varchar(3) NOT NULL default ' ',
  "AAAAJRQF" varchar(3) NOT NULL default ' ',
  "AAAAJRQG" varchar(3) NOT NULL default ' ',
  "AAAAJRQH" varchar(3) NOT NULL default ' ',
  "AAAAJRQI" varchar(3) NOT NULL default ' ',
  "AAAAJRQJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJRQQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJRQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJRLR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMPWP","AAABJRLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJRLR" varchar(2) NOT NULL default ' ',
  "AAAAJRRG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABMPWP","AAABJRLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRRM" (
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAAAJRRO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRRQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOEA" varchar(22) NOT NULL default ' ',
  "AAABDOED" varchar(22) NOT NULL default ' ',
  "AAABJJLO" varchar(22) NOT NULL default ' ',
  "AAABJJLF" varchar(4) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABDOEA","AAABDOED"),
  KEY "AAAAJRSA" ("AAABJJLO"),
  KEY "AAAAJRSC" ("AAABLUXQ","AAABJJLF"),
  KEY "AAAAJRSF" ("AAABLUXQ","AAABDOED"),
  KEY "AAAAJRSI" ("AAABLUXQ","AAABDODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRSL" (
  "AAAAJRST" varchar(30) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(3) NOT NULL default '000',
  "AAABLHAH" varchar(128) NOT NULL default ' ',
  "AAABHCQK" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJRST","AAABLMHB","AAABLSCQ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRSX" (
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAAAVOID" varchar(27) NOT NULL default ' ',
  "AAAAVOIE" varchar(4) NOT NULL default '0000',
  "AAAAVOIF" varchar(1) NOT NULL default ' ',
  "AAABJESQ" varchar(1) NOT NULL default ' ',
  "AAABJESR" varchar(10) NOT NULL default '0000000000',
  "AAABJESS" varchar(10) NOT NULL default '0000000000',
  "AAABJESP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAROVJ","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAAAJRTO" varchar(2) NOT NULL default '00',
  "AAAAYFKL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGNXB" varchar(1) NOT NULL default ' ',
  "AAAAWIFB" varchar(4) NOT NULL default ' ',
  "AAAAJRTY" varchar(1) NOT NULL default ' ',
  "AAAAMJSD" varchar(1) NOT NULL default ' ',
  "AAAAJRUA" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGNXB","AAAAWIFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAAJRUL" varchar(4) NOT NULL default '0000',
  "AAAAJRUM" decimal(13,2) NOT NULL default '0.00',
  "AAAAZUWK" decimal(13,2) NOT NULL default '0.00',
  "AAAAJRUO" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAJRVE" varchar(4) NOT NULL default '0000',
  "AAAAJRUZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAJRVA" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAAAJRVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  "AAABCBSU" varchar(35) NOT NULL default ' ',
  "AAAAJRVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAAAJRVZ" varchar(4) NOT NULL default '0000',
  "AAAAJRVU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMQVT","AAABMQVU","AAAAJRVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRWA" (
  "AAAAJRWR" varchar(15) NOT NULL default ' ',
  "AAAAYFKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJRWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJRWR" varchar(15) NOT NULL default ' ',
  "AAAAJRWS" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  "AAABFWCT" tinyint(3) unsigned NOT NULL default '0',
  "AAAAYEIP" varchar(25) NOT NULL default ' ',
  "AAAAJRWO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJRWR","AAAAJRWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRWT" (
  "AAABENTQ" varchar(2) NOT NULL default ' ',
  "AAAAJRWV" varchar(1) NOT NULL default ' ',
  "AAAAJRWW" varchar(1) NOT NULL default ' ',
  "AAAAJRWX" varchar(1) NOT NULL default ' ',
  "AAAAJRWY" varchar(1) NOT NULL default ' ',
  "AAAAJRWZ" varchar(1) NOT NULL default ' ',
  "AAAAJRXA" varchar(1) NOT NULL default ' ',
  "AAAAJRXB" varchar(1) NOT NULL default ' ',
  "AAAAJRXC" varchar(1) NOT NULL default ' ',
  "AAAAJRXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABENTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAAAJRXK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMCMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAJRXT" varchar(1) NOT NULL default ' ',
  "AAAAJRXU" varchar(1) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJKXJ","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXID","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRYS" (
  "AAAAJRZD" varchar(8) NOT NULL default ' ',
  "AAAAWGWC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJRZD","AAAAWGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRYX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJRZD" varchar(8) NOT NULL default ' ',
  "AAAAWGWC" varchar(10) NOT NULL default ' ',
  "AAAAJRZB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAJRZD","AAAAWGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMKYH" varchar(14) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABJHCL" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  "AAABJHCF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABMKYH","AAABKAFO","AAABJHCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJRZV" (
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAJSAF" varchar(4) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH","AAABMAMP","AAAAJSAF","AAABLTNT","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSAI" (
  "AAABEDJP" varchar(60) NOT NULL default ' ',
  "AAABLZLD" varchar(60) NOT NULL default ' ',
  "AAAAJSGU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDJP","AAABLZLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSAO" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAAAJSAW" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAAAJSAS" varchar(1) NOT NULL default ' ',
  "AAABGPVW" varchar(4) NOT NULL default ' ',
  "AAAAJSAU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAAAJSAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSAX" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAJSCQ" varchar(4) NOT NULL default '0000',
  "AAAAJSCR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLEIH","AAABKAOU","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSBI" (
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAJSFT" varchar(6) NOT NULL default '000000',
  "AAAAJSHU" varchar(16) NOT NULL default '0000000000000000',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTVR"(255),"AAAAJSFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSBQ" (
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAROVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSBT" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAAAVOIE" varchar(4) NOT NULL default '0000',
  "AAABJESR" varchar(10) NOT NULL default '0000000000',
  "AAABJESS" varchar(10) NOT NULL default '0000000000',
  "AAABJESQ" varchar(1) NOT NULL default ' ',
  "AAAAVOIF" varchar(1) NOT NULL default ' ',
  "AAABJESP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLEIH","AAAAROVJ","AAABHJGM","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSCJ" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAJSCQ" varchar(4) NOT NULL default '0000',
  "AAAAJSCR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLEIH","AAAAROVJ","AAABHJGM","AAABKAOU","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSCX" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABKAOU" varchar(27) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAALANI" varchar(4) NOT NULL default '0000',
  "AAABJSDF" varchar(4) NOT NULL default '0000',
  "AAABCWDR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABKAOU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSDH" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABLMHB","AAABLMHG","AAABEDRM","AAABCKEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSDS" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABLUXQ","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSDZ" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJSEC" varchar(32) NOT NULL default ' ',
  "AAAAJSED" varchar(10) NOT NULL default ' ',
  "AAAAJSEE" varchar(2) NOT NULL default ' ',
  "AAABJFIE" varchar(50) NOT NULL default ' ',
  "AAAAJSEG" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSEJ" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAAAJSHL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABLUXQ","AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSER" (
  "AAAAJSGU" varchar(60) NOT NULL default ' ',
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAAJSGY" varchar(2) NOT NULL default ' ',
  "AAAAJSGZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJSGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSEZ" (
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAJSFT" varchar(6) NOT NULL default '000000',
  "AAAAJSHU" varchar(16) NOT NULL default '0000000000000000',
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABFWAW" varchar(24) NOT NULL default ' ',
  "AAABMOHM" varchar(8) NOT NULL default ' ',
  "AAAAJSHL" varchar(32) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAAAJSHS" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"(255),"AAAAJSFT","AAAAJSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSFV" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABBJJS" varchar(8) NOT NULL default '00000000',
  "AAAATVGC" varchar(6) NOT NULL default '000000',
  "AAAARKZJ" varchar(1) NOT NULL default ' ',
  "AAAAJSGC" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJSGE" varchar(1) NOT NULL default ' ',
  "AAAAJSGF" varchar(32) NOT NULL default ' ',
  "AAABLUHL" varchar(6) NOT NULL default '000000',
  "AAABGPVW" varchar(4) NOT NULL default ' ',
  "AAAAJSGI" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAAAROVJ","AAABHJGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSGO" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAAJSGY" varchar(2) NOT NULL default ' ',
  "AAAAJSGZ" varchar(2) NOT NULL default ' ',
  "AAAAJSGU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIH","AAABLUXQ","AAABKWVJ","AAAAJSGY","AAAAJSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSHA" (
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAJSHU" varchar(16) NOT NULL default '0000000000000000',
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABFWAW" varchar(24) NOT NULL default ' ',
  "AAABMOHM" varchar(8) NOT NULL default ' ',
  "AAAAJSHL" varchar(32) NOT NULL default ' ',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABCKEF" varchar(2) NOT NULL default ' ',
  "AAAAJSHS" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"(255),"AAAAJSHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSHV" (
  "AAABLEIH" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJSHY" varchar(1) NOT NULL default ' ',
  "AAAAJSHZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLEIH","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJSIE" varchar(3) NOT NULL default ' ',
  "AAAAJSIF" varchar(1) NOT NULL default ' ',
  "AAAAJSIG" varchar(1) NOT NULL default ' ',
  "AAAAJSIH" varchar(3) NOT NULL default ' ',
  "AAAAJSII" varchar(1) NOT NULL default ' ',
  "AAAAJSIJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJSIR" varchar(2) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAJSIP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJSIR","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENGK" varchar(2) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAAJSJJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENGK","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENGK" varchar(2) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAAAJSJJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENGK","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAAAJSJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAAAYLBQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYLBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJSKP" varchar(12) NOT NULL default '000000000000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJSKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJSKP" varchar(12) NOT NULL default '000000000000',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJSKP","AAAAYLBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default '000',
  "AAAAJSLQ" varchar(1) NOT NULL default ' ',
  "AAAANEEA" varchar(40) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAAAJSLU" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKLZ","AAAAYLBV","AAABKRXK"),
  KEY "AAAAJSLH" ("AAABMRIL","AAAANEEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default '000',
  "AAAAJSLQ" varchar(1) NOT NULL default ' ',
  "AAAANEEA" varchar(40) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAAAJSLU" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABLKLZ","AAAAYLBV","AAABKRXK"),
  KEY "AAAAJSMC" ("AAABMRIL","AAABKSUH","AAAANEEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  "AAAAMIDI" varchar(30) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAMIDK" varchar(30) NOT NULL default ' ',
  "AAAAJSQP" varchar(4) NOT NULL default ' ',
  "AAABJEYP" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAAASHIU" varchar(18) NOT NULL default ' ',
  "AAAAJSQT" varchar(1) NOT NULL default ' ',
  "AAAAMIDL" varchar(30) NOT NULL default ' ',
  "AAAAJSQV" varchar(1) NOT NULL default ' ',
  "AAAAMIDM" varchar(1) NOT NULL default ' ',
  "AAAAJXKG" varchar(1) NOT NULL default ' ',
  "AAABFTLZ" varchar(1) NOT NULL default ' ',
  "AAAAJXKI" varchar(1) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAJXKK" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAJXKM" varchar(1) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAAAMIDO" varchar(8) NOT NULL default '00000000',
  "AAAAJXKP" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAJXKR" varchar(1) NOT NULL default ' ',
  "AAABKNPE" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default ' ',
  "AAAAJSRM" varchar(1) NOT NULL default ' ',
  "AAAAJSRN" varchar(10) NOT NULL default ' ',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  "AAABMHEL" varchar(25) NOT NULL default ' ',
  "AAABLIAR" varchar(18) NOT NULL default ' ',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAASHGR" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAMICW" varchar(8) NOT NULL default '00000000',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAAAJSRX" varchar(3) NOT NULL default ' ',
  "AAAASMYV" varchar(4) NOT NULL default ' ',
  "AAAAJSRZ" varchar(2) NOT NULL default ' ',
  "AAAAVRZR" varchar(20) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAAAMICZ" varchar(30) NOT NULL default ' ',
  "AAABLSTE" varchar(22) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKLZ"),
  KEY "AAAAJSOM" ("AAAAMIDI"),
  KEY "AAAAJSOO" ("AAABKNPE"),
  KEY "AAAAJSOQ" ("AAAASHIU"),
  KEY "AAAAJSOS" ("AAABMRIL","AAABLXTG"),
  KEY "AAAAJSOV" ("AAAAMIDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAATI" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAQPGN" varchar(1) NOT NULL default ' ',
  "AAAAJXLR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKLZ","AAABMRHS"),
  KEY "AAAAJSPI" ("AAAAJXLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAATI" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAQPGN" varchar(1) NOT NULL default ' ',
  "AAAAJXLR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABLKLZ","AAABMRHS"),
  KEY "AAAAJSPX" ("AAABKSUH","AAAAJXLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  "AAAAMIDI" varchar(30) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAMIDK" varchar(30) NOT NULL default ' ',
  "AAAAJSQP" varchar(4) NOT NULL default ' ',
  "AAABJEYP" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAAASHIU" varchar(18) NOT NULL default ' ',
  "AAAAJSQT" varchar(1) NOT NULL default ' ',
  "AAAAMIDL" varchar(30) NOT NULL default ' ',
  "AAAAJSQV" varchar(1) NOT NULL default ' ',
  "AAAAMIDM" varchar(1) NOT NULL default ' ',
  "AAAAJXKG" varchar(1) NOT NULL default ' ',
  "AAABFTLZ" varchar(1) NOT NULL default ' ',
  "AAAAJXKI" varchar(1) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAJXKK" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAJXKM" varchar(1) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAAAMIDO" varchar(8) NOT NULL default '00000000',
  "AAAAJXKP" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAJXKR" varchar(1) NOT NULL default ' ',
  "AAABKNPE" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default ' ',
  "AAAAJSRM" varchar(1) NOT NULL default ' ',
  "AAAAJSRN" varchar(10) NOT NULL default ' ',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  "AAABMHEL" varchar(25) NOT NULL default ' ',
  "AAABLIAR" varchar(18) NOT NULL default ' ',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAASHGR" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAMICW" varchar(8) NOT NULL default '00000000',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAAAJSRX" varchar(3) NOT NULL default ' ',
  "AAAASMYV" varchar(4) NOT NULL default ' ',
  "AAAAJSRZ" varchar(2) NOT NULL default ' ',
  "AAAAVRZR" varchar(20) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAAAMICZ" varchar(30) NOT NULL default ' ',
  "AAABLSTE" varchar(22) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABLKLZ"),
  KEY "AAAAJSSI" ("AAABKSUH","AAAAMIDI"),
  KEY "AAAAJSSL" ("AAABKSUH","AAAASHIU"),
  KEY "AAAAJSSO" ("AAABMRIL","AAABKSUH","AAABLXTG"),
  KEY "AAAAJSSS" ("AAABKSUH","AAAAMIDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAJVAY" varchar(2) NOT NULL default '00',
  "AAAAYZWG" varchar(20) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAAAYLBV" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAAAJSTD" varchar(40) NOT NULL default ' ',
  "AAAAJSTE" varchar(1) NOT NULL default ' ',
  "AAABCRGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAJSTU" varchar(2) NOT NULL default ' ',
  "AAAAJSTO" varchar(2) NOT NULL default ' ',
  "AAAAJSTP" varchar(1) NOT NULL default ' ',
  "AAABDBHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCMO","AAAAJSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSUD" (
  "AAAAPQWN" varchar(30) NOT NULL default ' ',
  "AAAAPOEO" varchar(4) NOT NULL default ' ',
  "AAAAJSVE" varchar(116) NOT NULL default ' ',
  "AAAAJSUU" varchar(3) NOT NULL default ' ',
  "AAAAJSUV" varchar(3) NOT NULL default ' ',
  "AAABCCLQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPQWN","AAAAPOEO","AAAAJSVE"),
  KEY "AAAAJSUN" ("AAAAPOEO","AAAAJSVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSUQ" (
  "AAAAPQWN" varchar(30) NOT NULL default ' ',
  "AAAAPOEO" varchar(4) NOT NULL default ' ',
  "AAAAJSVE" varchar(40) NOT NULL default ' ',
  "AAAAJSUU" varchar(3) NOT NULL default ' ',
  "AAAAJSUV" varchar(3) NOT NULL default ' ',
  "AAABCCLQ" varchar(1) NOT NULL default ' ',
  "AAAAJSUX" varchar(1) NOT NULL default ' ',
  "AAAAJSUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPQWN","AAAAPOEO","AAAAJSVE"),
  KEY "AAAAJSVC" ("AAAAPOEO","AAAAJSVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSVF" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  "AAABEQXQ" varchar(10) NOT NULL default '0000000000',
  "AAABEQXR" varchar(10) NOT NULL default '0000000000',
  "AAABGHOW" varchar(2) NOT NULL default '00',
  "AAABGHOY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSVM" (
  "AAABEQYB" varchar(2) NOT NULL default ' ',
  "AAABEQXX" varchar(10) NOT NULL default '0000000000',
  "AAABEQXY" varchar(10) NOT NULL default '0000000000',
  "AAABEQZP" varchar(4) NOT NULL default '0000',
  "AAABEQZQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABEQYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSVT" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSVW" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  "AAABFPKP" varchar(10) NOT NULL default '0000000000',
  "AAABGRSI" varchar(10) NOT NULL default '0000000000',
  "AAABEQZP" varchar(10) NOT NULL default '0000000000',
  "AAABEQZQ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAAAJSZN" varchar(1) NOT NULL default ' ',
  "AAAAJTST" varchar(1) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAAAJSWU" varchar(10) NOT NULL default ' ',
  "AAABBSKV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABJABF","AAAAJSZN","AAAAJTST","AAABGSNE","AAAAMYIG","AAAAJSWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAAJSZN" varchar(1) NOT NULL default ' ',
  "AAAAJSXB" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJTVL","AAABCNNC","AAAAJSZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAAJSXM" varchar(2) NOT NULL default ' ',
  "AAAAJSXN" varchar(10) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAAAJSXQ" varchar(35) NOT NULL default ' ',
  "AAAAJSXR" varchar(3) NOT NULL default ' ',
  "AAAAJSXS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJTVL","AAABCNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAAAJSYA" varchar(3) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAJSYI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJSZN" varchar(1) NOT NULL default ' ',
  "AAAAJTKP" varchar(1) NOT NULL default ' ',
  "AAAAMYJH" varchar(4) NOT NULL default ' ',
  "AAAAJSYS" varchar(4) NOT NULL default ' ',
  "AAAAJSYT" varchar(4) NOT NULL default ' ',
  "AAAAJSYU" varchar(4) NOT NULL default ' ',
  "AAAAJSYV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJSZN","AAAAJTKP","AAAAMYJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJSZN" varchar(1) NOT NULL default ' ',
  "AAAAJTST" varchar(1) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAABCNUY" varchar(4) NOT NULL default ' ',
  "AAABCNUZ" varchar(4) NOT NULL default ' ',
  "AAABHFHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJSZN","AAAAJTST","AAABGSNE","AAAAMYIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJSZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTSP" varchar(35) NOT NULL default ' ',
  "AAAAJSZY" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTBO" varchar(10) NOT NULL default ' ',
  "AAAAJTAH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTCU","AAAAJTBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTBO" varchar(10) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTCU","AAAAJTBO","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTBO" varchar(10) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  "AAABCNYI" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTCU","AAAAJTBO","AAABHFHO","AAABCNYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTBO" varchar(10) NOT NULL default ' ',
  "AAAAJTBK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAJTCU","AAAAJTBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTBP" (
  "AAAAJTBU" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJTBS" varchar(10) NOT NULL default ' ',
  "AAAAJTBT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJTBU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTBZ" varchar(20) NOT NULL default ' ',
  "AAAAJTCA" varchar(20) NOT NULL default ' ',
  "AAAAJTCB" varchar(20) NOT NULL default ' ',
  "AAAAJTCC" varchar(20) NOT NULL default ' ',
  "AAAAJTCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAABHFHO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTCU","AAABHFHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJTCU" varchar(10) NOT NULL default ' ',
  "AAAAJTCR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAJTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJUHE","AAAAJUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAAAJTTX" varchar(2) NOT NULL default '00',
  "AAAAJTTQ" varchar(2) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAAAMYKS" varchar(15) NOT NULL default ' ',
  "AAABGZJM" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAAAJTTX"),
  KEY "AAAAJTDQ" ("AAABMRIL","AAAAJTTQ","AAABEDJP","AAABENNR","AAAAMYKS","AAABGZJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABGXGV" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAABMPLS","AAABCNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAAAJTUO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAAJTVN" varchar(1) NOT NULL default ' ',
  "AAAAJTVO" varchar(2) NOT NULL default ' ',
  "AAAAJTVP" varchar(8) NOT NULL default ' ',
  "AAAAJTVQ" varchar(8) NOT NULL default ' ',
  "AAAAJTVR" varchar(8) NOT NULL default ' ',
  "AAAAJTVS" varchar(8) NOT NULL default ' ',
  "AAAAJTVB" varchar(10) NOT NULL default ' ',
  "AAAAJTVC" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABENPN" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAJTVH" varchar(1) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAAAJTFJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAABCNNC","AAAAJTVN","AAAAJTVO","AAAAJTVP","AAAAJTVQ","AAAAJTVR","AAAAJTVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAAAMYKS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAAMYKQ" varchar(3) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAAAJTGJ" varchar(1) NOT NULL default ' ',
  "AAAAJTGK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJTVL","AAABCNNC","AAAAMYKQ","AAAAMYIG","AAABJRIC","AAABMCZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  "AAAAJTGZ" varchar(6) NOT NULL default '000000',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAAJTHC" varchar(2) NOT NULL default ' ',
  "AAAAMYHF" varchar(15) NOT NULL default ' ',
  "AAAAMYHG" varchar(6) NOT NULL default ' ',
  "AAAAMYHH" varchar(15) NOT NULL default ' ',
  "AAAAMYHI" varchar(6) NOT NULL default ' ',
  "AAAAJTHH" varchar(2) NOT NULL default ' ',
  "AAAAMYHJ" varchar(60) NOT NULL default ' ',
  "AAAAMYHK" varchar(3) NOT NULL default ' ',
  "AAAAMYHL" varchar(60) NOT NULL default ' ',
  "AAAAMYHM" varchar(3) NOT NULL default ' ',
  "AAAAJTWJ" varchar(35) NOT NULL default ' ',
  "AAAAMYHN" varchar(34) NOT NULL default ' ',
  "AAAAJTHO" varchar(5) NOT NULL default ' ',
  "AAAAJTHP" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTHQ" varchar(8) NOT NULL default '00000000',
  "AAAAJTHR" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTHS" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTHT" decimal(9,5) NOT NULL default '0.00000',
  "AAAAMYHO" varchar(15) NOT NULL default ' ',
  "AAAAMYHP" varchar(6) NOT NULL default ' ',
  "AAAAMYHQ" varchar(15) NOT NULL default ' ',
  "AAAAMYHR" varchar(6) NOT NULL default ' ',
  "AAAAJTHY" varchar(2) NOT NULL default ' ',
  "AAAAMYHS" varchar(60) NOT NULL default ' ',
  "AAAAMYHT" varchar(3) NOT NULL default ' ',
  "AAAAMYHU" varchar(60) NOT NULL default ' ',
  "AAAAMYHV" varchar(3) NOT NULL default ' ',
  "AAAAJTID" varchar(35) NOT NULL default ' ',
  "AAAAMYHW" varchar(34) NOT NULL default ' ',
  "AAAAJTIF" varchar(5) NOT NULL default ' ',
  "AAAAJTIG" varchar(8) NOT NULL default '00000000',
  "AAAAJTIH" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTII" varchar(15) NOT NULL default ' ',
  "AAAAMYHX" varchar(15) NOT NULL default ' ',
  "AAAAMYHY" varchar(6) NOT NULL default ' ',
  "AAAAMYHZ" varchar(15) NOT NULL default ' ',
  "AAAAMYIA" varchar(6) NOT NULL default ' ',
  "AAAAJTIN" varchar(2) NOT NULL default ' ',
  "AAAAMYIB" varchar(60) NOT NULL default ' ',
  "AAAAMYIC" varchar(3) NOT NULL default ' ',
  "AAAAJTIQ" varchar(20) NOT NULL default ' ',
  "AAAAMYID" varchar(60) NOT NULL default ' ',
  "AAAAMYIE" varchar(3) NOT NULL default ' ',
  "AAAAJTIT" varchar(35) NOT NULL default ' ',
  "AAAAMYIF" varchar(34) NOT NULL default ' ',
  "AAAAJTIV" varchar(5) NOT NULL default ' ',
  "AAAAQHQH" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTIX" varchar(8) NOT NULL default '00000000',
  "AAAAJTIY" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTIZ" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTJA" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTJB" varchar(4) NOT NULL default ' ',
  "AAAAJTJC" varchar(15) NOT NULL default ' ',
  "AAAAJTJD" varchar(6) NOT NULL default ' ',
  "AAAAJTJE" varchar(15) NOT NULL default ' ',
  "AAAAJTJF" varchar(6) NOT NULL default ' ',
  "AAAAJTJG" varchar(2) NOT NULL default ' ',
  "AAAAJTJH" varchar(60) NOT NULL default ' ',
  "AAAAJTJI" varchar(3) NOT NULL default ' ',
  "AAAAJTJJ" varchar(60) NOT NULL default ' ',
  "AAAAJTJK" varchar(3) NOT NULL default ' ',
  "AAAAJTJL" varchar(35) NOT NULL default ' ',
  "AAAAJTJM" varchar(34) NOT NULL default ' ',
  "AAAAJTJN" varchar(5) NOT NULL default ' ',
  "AAAAJTJO" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTJP" varchar(8) NOT NULL default '00000000',
  "AAAAJTJQ" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTJR" decimal(17,2) NOT NULL default '0.00',
  "AAAAJTJS" decimal(9,5) NOT NULL default '0.00000',
  "AAAAJTJT" varchar(3) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAAAJTJV" varchar(1) NOT NULL default ' ',
  "AAAAMYIH" decimal(17,2) NOT NULL default '0.00',
  "AAAAMYII" varchar(5) NOT NULL default ' ',
  "AAAAMYIP" varchar(8) NOT NULL default '00000000',
  "AAAAJTJZ" varchar(8) NOT NULL default '00000000',
  "AAAAMYIQ" varchar(8) NOT NULL default ' ',
  "AAABETCO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAAAMYIU" varchar(8) NOT NULL default ' ',
  "AAABCWPN" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAAAMYIY" varchar(12) NOT NULL default ' ',
  "AAAAXUBJ" varchar(8) NOT NULL default '00000000',
  "AAAAMYJA" varchar(6) NOT NULL default '000000',
  "AAAAMYJC" varchar(1) NOT NULL default ' ',
  "AAAAMYJE" varchar(1) NOT NULL default ' ',
  "AAAAMYJG" varchar(1) NOT NULL default ' ',
  "AAAAMYJH" varchar(4) NOT NULL default ' ',
  "AAAAJTKP" varchar(1) NOT NULL default ' ',
  "AAAAJTKQ" varchar(1) NOT NULL default ' ',
  "AAAAJTKR" varchar(1) NOT NULL default ' ',
  "AAAAJTKS" varchar(16) NOT NULL default '0000000000000000',
  "AAAAJTKT" varchar(16) NOT NULL default '0000000000000000',
  "AAAAJTWK" varchar(1) NOT NULL default ' ',
  "AAAAJTWL" varchar(2) NOT NULL default ' ',
  "AAAAJTKW" varchar(1) NOT NULL default ' ',
  "AAAAJTKX" varchar(2) NOT NULL default ' ',
  "AAAAJTKY" varchar(10) NOT NULL default ' ',
  "AAAAJTKZ" varchar(30) NOT NULL default ' ',
  "AAAAJTLA" varchar(30) NOT NULL default ' ',
  "AAAAJTLB" varchar(16) NOT NULL default ' ',
  "AAAAJTLC" varchar(8) NOT NULL default ' ',
  "AAAAJTLD" varchar(8) NOT NULL default ' ',
  "AAAAJTLE" varchar(8) NOT NULL default ' ',
  "AAAAJTLF" varchar(8) NOT NULL default ' ',
  "AAAAJTLG" varchar(8) NOT NULL default '00000000',
  "AAAAJTLH" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  UNIQUE KEY "AAAAJTLK" ("AAABMRIL","AAABMKAI","AAAAJUHE","AAAAJUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAYGNB" varchar(4) NOT NULL default ' ',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABLVHQ" varchar(35) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(16) NOT NULL default ' ',
  "AAAAYENH" varchar(31) NOT NULL default ' ',
  "AAABEGXK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAAAYGNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJUHE","AAAAJUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAJTNL" varchar(6) NOT NULL default ' ',
  "AAAAJTNM" varchar(6) NOT NULL default ' ',
  "AAABBRKE" varchar(6) NOT NULL default '000000',
  "AAAAMYJQ" varchar(6) NOT NULL default '000000',
  "AAAAMYJR" varchar(1) NOT NULL default ' ',
  "AAABMRCK" longtext,
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAAAJTNL","AAAAJTNM","AAABBRKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAJTOJ" varchar(6) NOT NULL default ' ',
  "AAAAJTOK" varchar(15) NOT NULL default ' ',
  "AAAAJTOL" varchar(15) NOT NULL default ' ',
  "AAAAJTOM" varchar(4) NOT NULL default '0000',
  "AAAAJTON" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  "AAAAMYKB" varchar(6) NOT NULL default ' ',
  "AAABJGGL" varchar(64) NOT NULL default ' ',
  "AAABETCO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABHESL" varchar(6) NOT NULL default '000000',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  "AAABJCFN" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAAAJTOJ","AAAAJTOK","AAAAJTOL","AAAAJTOM","AAAAJTON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBZMT" varchar(24) NOT NULL default ' ',
  "AAABMRFO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBZMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABLYRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAJTPH" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABLYRW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAABHZOI" varchar(12) NOT NULL default ' ',
  "AAAAJUEK" varchar(12) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAAALPVJ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABLTXI","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABFZBW" varchar(12) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  "AAABLLNH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTSP" varchar(35) NOT NULL default ' ',
  "AAAAVSVS" varchar(1) NOT NULL default ' ',
  "AAAAJTSR" varchar(1) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAJTST" varchar(1) NOT NULL default ' ',
  "AAAAVSVT" varchar(4) NOT NULL default '0000',
  "AAAAVSVU" varchar(2) NOT NULL default '00',
  "AAAAJTSW" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJTSX" varchar(5) NOT NULL default ' ',
  "AAAAJTRL" varchar(5) NOT NULL default ' ',
  "AAAAJTRM" decimal(23,2) NOT NULL default '0.00',
  "AAAAJTRN" decimal(23,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  KEY "AAAAJTRQ" ("AAABMKAI","AAAAJTSP","AAAAVSVS","AAAAJTSR","AAABGSNE","AAAAJTST","AAAAVSVT","AAAAVSVU","AAAAJTSW","AAAAJTSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJTSP" varchar(35) NOT NULL default ' ',
  "AAAAVSVS" varchar(1) NOT NULL default ' ',
  "AAAAJTSR" varchar(1) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAJTST" varchar(1) NOT NULL default ' ',
  "AAAAVSVT" varchar(4) NOT NULL default '0000',
  "AAAAVSVU" varchar(2) NOT NULL default '00',
  "AAAAJTSW" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJTSX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJTSP","AAAAVSVS","AAAAJTSR","AAABGSNE","AAAAJTST","AAAAVSVT","AAAAVSVU","AAAAJTSW","AAAAJTSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJTTK" varchar(10) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAJTTD" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABFKWB" varchar(2) NOT NULL default ' ',
  "AAABFKWC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAJTTK","AAABMCRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAAAJTTX" varchar(50) NOT NULL default ' ',
  "AAAAJTTQ" varchar(2) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAAAMYKS" varchar(15) NOT NULL default ' ',
  "AAABGZJM" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAAAJTTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABGXGV" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAABMPLS","AAABCNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAAAJTUO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJTVL" varchar(4) NOT NULL default ' ',
  "AAABCNNC" varchar(5) NOT NULL default ' ',
  "AAAAJTVN" varchar(1) NOT NULL default ' ',
  "AAAAJTVO" varchar(2) NOT NULL default ' ',
  "AAAAJTVP" varchar(8) NOT NULL default ' ',
  "AAAAJTVQ" varchar(8) NOT NULL default ' ',
  "AAAAJTVR" varchar(8) NOT NULL default ' ',
  "AAAAJTVS" varchar(8) NOT NULL default ' ',
  "AAAAJTVB" varchar(10) NOT NULL default ' ',
  "AAAAJTVC" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABENPN" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAJTVH" varchar(1) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJTVL","AAABCNNC","AAAAJTVN","AAAAJTVO","AAAAJTVP","AAAAJTVQ","AAAAJTVR","AAAAJTVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAJTWJ" varchar(35) NOT NULL default ' ',
  "AAAAJTWK" varchar(1) NOT NULL default ' ',
  "AAAAJTWL" varchar(2) NOT NULL default ' ',
  "AAAAMYIE" varchar(3) NOT NULL default ' ',
  "AAAAMYII" varchar(5) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAAAJTWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABKWVJ","AAABGSNE","AAAAJTWJ","AAAAJTWK","AAAAJTWL","AAAAMYIE","AAAAMYII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKLVZ" varchar(12) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKLVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTWV" (
  "AAAAJTWY" varchar(9) NOT NULL default ' ',
  "AAAAJTWZ" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJTWY","AAAAJTWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABLPQY","AAABGTBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTXK" (
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(61) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(150) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTXT" (
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTXZ" (
  "AAABMREF" varchar(60) NOT NULL default ' ',
  "AAABBYDZ" VARBINARY(255),
  "AAAAJTYC" VARBINARY(255),
  "AAAAJTYD" VARBINARY(255),
  "AAAAJTYE" VARBINARY(255),
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGKTL" varchar(1) NOT NULL default ' ',
  "AAABEDJP" varchar(1) NOT NULL default ' ',
  "AAABLZLD" varchar(1) NOT NULL default ' ',
  "AAABKWUO" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABGTBZ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTYU" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLRJL" varchar(3) NOT NULL default ' ',
  "AAABIGGO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTZA" (
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABAHSM" varchar(15) NOT NULL default ' ',
  "AAAAJTZD" varchar(30) NOT NULL default ' ',
  "AAABMKFC" varchar(15) NOT NULL default ' ',
  "AAAAJTZF" varchar(30) NOT NULL default ' ',
  "AAABLDUY" varchar(15) NOT NULL default ' ',
  "AAABFNLG" varchar(30) NOT NULL default ' ',
  "AAABHGWF" varchar(15) NOT NULL default ' ',
  "AAABHHTK" varchar(30) NOT NULL default ' ',
  "AAABGKTL" varchar(1) NOT NULL default ' ',
  "AAABEDJP" varchar(1) NOT NULL default ' ',
  "AAABLZLD" varchar(1) NOT NULL default ' ',
  "AAABKWUO" varchar(1) NOT NULL default ' ',
  "AAAAJUFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTZQ" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJTZW" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWLCF" varchar(20) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWLCF","AAABLEKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUAJ" (
  "AAABMLNA" int(11) NOT NULL default '0',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABAGQW" varchar(1) NOT NULL default ' ',
  "AAABMQXO" varchar(1) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAABLTXS" varchar(1) NOT NULL default ' ',
  "AAABFPPX" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUBZ" varchar(1) NOT NULL default '0',
  "AAAAJUCA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJUBZ","AAAAJUCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABENQM" varchar(16) NOT NULL default ' ',
  "AAAAJUBJ" varchar(8) NOT NULL default ' ',
  "AAAAJUBK" varchar(8) NOT NULL default ' ',
  "AAAAJUBL" varchar(8) NOT NULL default ' ',
  "AAAAJUBM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABENQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUBZ" varchar(1) NOT NULL default '0',
  "AAAAJUCA" varchar(8) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAAAJUBZ","AAAAJUCA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUCC" (
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUCJ" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAJUCL" varchar(30) NOT NULL default ' ',
  "AAAAJUCM" varchar(30) NOT NULL default ' ',
  "AAAAJUCN" varchar(4) NOT NULL default ' ',
  "AAABJSDF" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAAJUCQ" varchar(4) NOT NULL default ' ',
  "AAAAJUCR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUCT" (
  "AAABCJNN" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABCJNN","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUDA" (
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUDE" (
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABJCXS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUDK" (
  "AAABLMOZ" varchar(20) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(150) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAAAMMOR" varchar(12) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAABHZOI" varchar(12) NOT NULL default ' ',
  "AAAAJUEK" varchar(12) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABMNYT","AAAAMMOR","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUEX" (
  "AAAAJUFE" varchar(2) NOT NULL default ' ',
  "AAAAJUFF" varchar(2) NOT NULL default '00',
  "AAAAJUFA" varchar(6) NOT NULL default ' ',
  "AAAAJUFB" varchar(15) NOT NULL default ' ',
  "AAAAJUFC" varchar(15) NOT NULL default ' ',
  "AAAAJUFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJUFE","AAAAJUFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  "AAAAJUFJ" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAAAJUFL" varchar(1) NOT NULL default ' ',
  "AAAAJUFM" varchar(1) NOT NULL default ' ',
  "AAAAJUFN" varchar(1) NOT NULL default ' ',
  "AAAAJUFO" varchar(1) NOT NULL default ' ',
  "AAAAJUFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTBZ","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABGTBZ" varchar(6) NOT NULL default ' ',
  "AAAAJUGE" varchar(1) NOT NULL default ' ',
  "AAAAJUGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABGTBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAAAMMOR" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABMNYT","AAAAMMOR","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAAAMMOR" varchar(12) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAAAJUHE" varchar(10) NOT NULL default '0000000000',
  "AAAAJUHF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABMNYT","AAAAMMOR","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKAI" varchar(4) NOT NULL default ' ',
  "AAABGTBZ" varchar(8) NOT NULL default ' ',
  "AAABCNYZ" varchar(3) NOT NULL default ' ',
  "AAABGSNE" varchar(1) NOT NULL default ' ',
  "AAAAMYIG" varchar(6) NOT NULL default ' ',
  "AAAAJUHS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKAI","AAABGTBZ","AAABCNYZ","AAABGSNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLJZE" varchar(6) NOT NULL default '000000',
  "AAAAJUID" varchar(1) NOT NULL default ' ',
  "AAABLJYP" varchar(12) NOT NULL default ' ',
  "AAABLJYQ" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABLJYS" varchar(8) NOT NULL default '00000000',
  "AAABLJYT" varchar(8) NOT NULL default '00000000',
  "AAABLJYU" varchar(6) NOT NULL default '000000',
  "AAABLJYV" varchar(6) NOT NULL default '000000',
  "AAABLJYW" varchar(12) NOT NULL default ' ',
  "AAABLJYX" varchar(8) NOT NULL default '00000000',
  "AAABLJYY" varchar(6) NOT NULL default '000000',
  "AAABLJZI" varchar(1) NOT NULL default ' ',
  "AAAAJUIP" varchar(1) NOT NULL default ' ',
  "AAABLJZA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMLPO","AAABLJZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABMAKT" varchar(12) NOT NULL default ' ',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLZDH" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAQPGY" varchar(1) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLTYR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMALG","AAABMLPO"),
  KEY "AAAAJUJQ" ("AAABMRIL","AAABLXWK"),
  KEY "AAAAJUJT" ("AAABMAKT","AAABMALG","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAAAWSCG" varchar(1) NOT NULL default ' ',
  "AAAAWSCH" varchar(1) NOT NULL default ' ',
  "AAAAWSCI" varchar(1) NOT NULL default ' ',
  "AAAAJUKJ" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAAAJULF" varchar(1) NOT NULL default ' ',
  "AAAAJUKQ" varchar(1) NOT NULL default ' ',
  "AAAAWSCJ" varchar(1) NOT NULL default ' ',
  "AAAAJUKS" varchar(1) NOT NULL default ' ',
  "AAAAJUKT" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMLPO"),
  KEY "AAAAJUKZ" ("AAABMCRM"),
  KEY "AAAAJULB" ("AAABMQVW"),
  KEY "AAAAJULD" ("AAABLLEF","AAAAJULF"),
  KEY "AAAAJULG" ("AAABLKLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJULI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABJFDW" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAAAWKZZ" varchar(8) NOT NULL default '00000000',
  "AAAAWKZX" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAAAJULX" varchar(1) NOT NULL default ' ',
  "AAABJXFC" varchar(1) NOT NULL default ' ',
  "AAAAWKZW" varchar(16) NOT NULL default ' ',
  "AAAAJUMA" varchar(1) NOT NULL default ' ',
  "AAAAJUMB" varchar(1) NOT NULL default ' ',
  "AAAAJUMC" varchar(2) NOT NULL default ' ',
  "AAAAJUMD" varchar(10) NOT NULL default ' ',
  "AAAASTFO" varchar(16) NOT NULL default ' ',
  "AAAAJUMF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCM","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNPE" varchar(12) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAAAWSIV" varchar(1) NOT NULL default ' ',
  "AAAAJXKT" varchar(1) NOT NULL default ' ',
  "AAAAMIDR" varchar(30) NOT NULL default ' ',
  "AAAAJUPC" varchar(1) NOT NULL default ' ',
  "AAAASMYP" varchar(4) NOT NULL default ' ',
  "AAAAJUPE" varchar(1) NOT NULL default ' ',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAAAJUPG" varchar(1) NOT NULL default ' ',
  "AAAAMIDU" varchar(8) NOT NULL default ' ',
  "AAAAJUPI" varchar(1) NOT NULL default ' ',
  "AAABGIZI" varchar(3) NOT NULL default ' ',
  "AAAAJUPK" varchar(1) NOT NULL default ' ',
  "AAAAJUPL" varchar(2) NOT NULL default ' ',
  "AAAAJUPM" varchar(8) NOT NULL default '00000000',
  "AAAAJUPN" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAJUPP" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAJUPR" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAAAJUPT" varchar(1) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAAAJUPV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJUPX" varchar(1) NOT NULL default ' ',
  "AAAAXYMM" varchar(12) NOT NULL default ' ',
  "AAAAJUPZ" varchar(1) NOT NULL default ' ',
  "AAAAMIEB" varchar(4) NOT NULL default ' ',
  "AAAAJUQB" varchar(1) NOT NULL default ' ',
  "AAAAMIEC" varchar(12) NOT NULL default ' ',
  "AAAAJUQD" varchar(1) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAAAJUQF" varchar(1) NOT NULL default ' ',
  "AAAAJUQG" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAJUQI" varchar(1) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAAAJUQK" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAAAJUQM" varchar(1) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAAAJUQO" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNPE"),
  KEY "AAAAJUOI" ("AAABLXWK","AAABKNGX"),
  KEY "AAAAJUOL" ("AAABMCRK","AAABKNGX"),
  KEY "AAAAJUOO" ("AAABLKLZ","AAABKNGX"),
  KEY "AAAAJUOR" ("AAAAXYMM","AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABKNPE" varchar(12) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAAAWSIV" varchar(1) NOT NULL default ' ',
  "AAAAJXKT" varchar(1) NOT NULL default ' ',
  "AAAAMIDR" varchar(30) NOT NULL default ' ',
  "AAAAJUPC" varchar(1) NOT NULL default ' ',
  "AAAASMYP" varchar(4) NOT NULL default ' ',
  "AAAAJUPE" varchar(1) NOT NULL default ' ',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAAAJUPG" varchar(1) NOT NULL default ' ',
  "AAAAMIDU" varchar(8) NOT NULL default ' ',
  "AAAAJUPI" varchar(1) NOT NULL default ' ',
  "AAABGIZI" varchar(3) NOT NULL default ' ',
  "AAAAJUPK" varchar(1) NOT NULL default ' ',
  "AAAAJUPL" varchar(2) NOT NULL default ' ',
  "AAAAJUPM" varchar(8) NOT NULL default '00000000',
  "AAAAJUPN" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAJUPP" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAJUPR" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAAAJUPT" varchar(1) NOT NULL default ' ',
  "AAABDIIT" varchar(8) NOT NULL default '00000000',
  "AAAAJUPV" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAJUPX" varchar(1) NOT NULL default ' ',
  "AAAAXYMM" varchar(12) NOT NULL default ' ',
  "AAAAJUPZ" varchar(1) NOT NULL default ' ',
  "AAAAMIEB" varchar(4) NOT NULL default ' ',
  "AAAAJUQB" varchar(1) NOT NULL default ' ',
  "AAAAMIEC" varchar(12) NOT NULL default ' ',
  "AAAAJUQD" varchar(1) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAAAJUQF" varchar(1) NOT NULL default ' ',
  "AAAAJUQG" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAAAJUQI" varchar(1) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAAAJUQK" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAAAJUQM" varchar(1) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAAAJUQO" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABKNPE"),
  KEY "AAAAJUQV" ("AAABKSUH","AAABLXWK","AAABKNGX"),
  KEY "AAAAJUQZ" ("AAABKSUH","AAABMCRK","AAABKNGX"),
  KEY "AAAAJURD" ("AAABKSUH","AAABLKLZ","AAABKNGX"),
  KEY "AAAAJURH" ("AAABKSUH","AAAAXYMM","AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJURL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABCDHU" varchar(2) NOT NULL default ' ',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAJUUD" varchar(4) NOT NULL default ' ',
  "AAABCSAA" varchar(4) NOT NULL default ' ',
  "AAABCRZZ" varchar(4) NOT NULL default ' ',
  "AAAAJURZ" varchar(6) NOT NULL default ' ',
  "AAAAJUSA" varchar(6) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABJSIQ" varchar(8) NOT NULL default '00000000',
  "AAABGAUK" varchar(1) NOT NULL default ' ',
  "AAAAJUUC" varchar(10) NOT NULL default ' ',
  "AAAAJUSI" varchar(10) NOT NULL default ' ',
  "AAAAJUSJ" varchar(4) NOT NULL default ' ',
  "AAAAJUSK" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABJDPM" varchar(7) NOT NULL default ' ',
  "AAAAJUSN" varchar(1) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABIZQZ" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRIO" varchar(10) NOT NULL default ' ',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAJUSW" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABLCND" varchar(20) NOT NULL default ' ',
  "AAABLCNE" varchar(20) NOT NULL default ' ',
  "AAABLCNF" varchar(10) NOT NULL default ' ',
  "AAABLCNG" varchar(10) NOT NULL default ' ',
  "AAABLCNH" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNI" varchar(3) NOT NULL default ' ',
  "AAABLCNJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNK" varchar(3) NOT NULL default ' ',
  "AAABLCNL" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNM" varchar(5) NOT NULL default ' ',
  "AAABLCNN" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNO" varchar(5) NOT NULL default ' ',
  "AAABLCNP" varchar(8) NOT NULL default '00000000',
  "AAABLCNQ" varchar(8) NOT NULL default '00000000',
  "AAABLCNR" varchar(1) NOT NULL default ' ',
  "AAABLCNS" varchar(1) NOT NULL default ' ',
  "AAABILSX" varchar(6) NOT NULL default ' ',
  "AAABILTM" varchar(6) NOT NULL default ' ',
  "AAAAJUTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD"),
  KEY "AAAAJUTT" ("AAABKRJY","AAABKRJU","AAABMRIL"),
  KEY "AAAAJUTX" ("AAABLXTG","AAABMRIL"),
  KEY "AAAAJUUA" ("AAABCDHU","AAAAJUUC","AAAAJUUD","AAABCRZZ","AAABKOVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAAAJUUV" varchar(10) NOT NULL default ' ',
  "AAAAJUUW" varchar(4) NOT NULL default ' ',
  "AAAAJUVR" varchar(10) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABKRIG" varchar(2) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABKRIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJUVR" varchar(10) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJUVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJUVZ" varchar(2) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJUVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABKOVI" varchar(5) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABKOVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJUWP" varchar(9) NOT NULL default ' ',
  "AAAAJUWM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJUWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAJVAK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLTD"),
  KEY "AAAAJUWZ" ("AAAAJVAK","AAABMLTD","AAABMRIL","AAABMRHS"),
  KEY "AAAAJUXE" ("AAABMLTD","AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAJUXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABLXTG"),
  KEY "AAAAJUXQ" ("AAABLXTG","AAABMRIL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJUYE" varchar(12) NOT NULL default ' ',
  "AAAAPOGD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJUYE","AAAAPOGD"),
  KEY "AAAAJUYD" ("AAAAJUYE","AAABMLTD","AAAAPOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJVAY" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAJUYQ" varchar(1) NOT NULL default ' ',
  "AAABEMCA" varchar(4) NOT NULL default ' ',
  "AAABDECB" varchar(8) NOT NULL default '00000000',
  "AAAALVQA" varchar(8) NOT NULL default '00000000',
  "AAAAJUYU" decimal(5,2) NOT NULL default '0.00',
  "AAAAJUYV" decimal(15,2) NOT NULL default '0.00',
  "AAAAJUYW" decimal(15,2) NOT NULL default '0.00',
  "AAAAJUYX" decimal(7,3) NOT NULL default '0.000',
  "AAAAJUYY" decimal(7,3) NOT NULL default '0.000',
  "AAABCDAX" varchar(8) NOT NULL default '00000000',
  "AAABAEGB" varchar(1) NOT NULL default ' ',
  "AAABGCKQ" varchar(3) NOT NULL default ' ',
  "AAAAJUZC" varchar(4) NOT NULL default ' ',
  "AAAAJUZD" varchar(4) NOT NULL default ' ',
  "AAAAJUZE" varchar(8) NOT NULL default '00000000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  "AAAAJUZH" decimal(3,0) NOT NULL default '0',
  "AAAAJUZI" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJVAY"),
  UNIQUE KEY "AAAAJUZM" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJUZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJVAY" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAAAJVAY","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAAAJVAY" varchar(4) NOT NULL default ' ',
  "AAAAJVAI" varchar(50) NOT NULL default ' ',
  "AAAAMUKE" varchar(50) NOT NULL default ' ',
  "AAAAJVAK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLTD","AAAAJVAY"),
  KEY "AAAAJVAP" ("AAABMLTD","AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMLTD" varchar(12) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAAAJVAY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMLTD","AAABFUWK"),
  KEY "AAAAJVBD" ("AAABMLTD","AAABMRIL","AAABMDBB","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVBI" (
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAAAJVBK" varchar(4) NOT NULL default ' ',
  "AAAAJVBL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHATI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVBN" (
  "AAABGQUK" varchar(20) NOT NULL default ' ',
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  "AAAAJVBR" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJVBS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGQUK","AAABHATI","AAABFUCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVBW" (
  "AAABCDBN" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(132) NOT NULL default ' ',
  "AAABCYGL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCDBN","AAABHATI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVCE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  "AAAAJVMF" varchar(8) NOT NULL default ' ',
  "AAAAJVMG" varchar(4) NOT NULL default '0000',
  "AAAAJVMH" varchar(24) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABETFA" varchar(8) NOT NULL default '00000000',
  "AAABHEYG" varchar(24) NOT NULL default ' ',
  "AAAAJVGP" varchar(26) NOT NULL default ' ',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAAAJVHQ" varchar(10) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAJVFR" varchar(30) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  "AAAAJVCX" varchar(3) NOT NULL default ' ',
  "AAAAJVCY" varchar(8) NOT NULL default '00000000',
  "AAAAJVCZ" varchar(35) NOT NULL default ' ',
  "AAAAJVDA" varchar(25) NOT NULL default ' ',
  "AAAAJVDB" varchar(1) NOT NULL default ' ',
  "AAAAJVDC" varchar(10) NOT NULL default ' ',
  "AAAAJVDD" varchar(2) NOT NULL default ' ',
  "AAAAJVDE" varchar(5) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAAJVDG" varchar(5) NOT NULL default ' ',
  "AAAAJVDH" varchar(5) NOT NULL default ' ',
  "AAABMIRY" varchar(2) NOT NULL default ' ',
  "AAABEIJR" varchar(2) NOT NULL default ' ',
  "AAAAJVDK" varchar(20) NOT NULL default ' ',
  "AAAAJVDL" varchar(20) NOT NULL default ' ',
  "AAAAJVDM" varchar(10) NOT NULL default ' ',
  "AAAAJVDN" varchar(10) NOT NULL default ' ',
  "AAAAJVDO" varchar(1) NOT NULL default ' ',
  "AAAAJVDP" varchar(1) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAAAJVDU" varchar(4) NOT NULL default '0000',
  "AAAAJVDV" varchar(1) NOT NULL default '0',
  "AAAAJVDW" varchar(1) NOT NULL default '0',
  "AAAAJVDX" varchar(6) NOT NULL default ' ',
  "AAAAJVDY" varchar(6) NOT NULL default ' ',
  "AAAAJVDZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCCXM","AAAAJVMF","AAAAJVMG","AAAAJVMH","AAABKRIM","AAABETFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLSAK","AAABLSAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVEQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAAAJVEV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLSAK","AAABLSAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVFA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVFR" varchar(30) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKRIM","AAAAJVFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVFI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVFR" varchar(30) NOT NULL default ' ',
  "AAAAJVFN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABKRIM","AAAAJVFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVFS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAAAJVGP" varchar(26) NOT NULL default ' ',
  "AAAAJVGE" varchar(26) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAAAJVGP"),
  KEY "AAAAJVGC" ("AAABLUXQ","AAAAJVGE","AAABININ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVGG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAAAJVGP" varchar(26) NOT NULL default ' ',
  "AAAAJVGL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABININ","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVGQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKRIM","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAAAJVHQ" varchar(10) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLSAK","AAAAJVHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVHH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAAAJVHQ" varchar(10) NOT NULL default ' ',
  "AAAAJVHM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLSAK","AAAAJVHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVHR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJVMF" varchar(8) NOT NULL default ' ',
  "AAAAJVMG" varchar(4) NOT NULL default '0000',
  "AAAAJVMH" varchar(24) NOT NULL default ' ',
  "AAABHIKN" varchar(8) NOT NULL default '00000000',
  "AAAAJVHY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAJVMF","AAAAJVMG","AAAAJVMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVIE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAJVIJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABKRIM","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVIO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAABHEYG" varchar(24) NOT NULL default ' ',
  "AAAAJVJA" varchar(24) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABHEYG"),
  KEY "AAAAJVIY" ("AAABLUXQ","AAAAJVJA","AAABININ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVJC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(6) NOT NULL default ' ',
  "AAABHEYG" varchar(24) NOT NULL default ' ',
  "AAAAJVJH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABININ","AAABHEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVJM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  "AAAAJVMF" varchar(8) NOT NULL default ' ',
  "AAAAJVMG" varchar(4) NOT NULL default '0000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAPQWX" varchar(1) NOT NULL default ' ',
  "AAABLVBE" varchar(1) NOT NULL default ' ',
  "AAABETFA" varchar(8) NOT NULL default '00000000',
  "AAAAJVJV" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAKARO" varchar(8) NOT NULL default '00000000',
  "AAAAJVKA" varchar(12) NOT NULL default ' ',
  "AAAAJVKB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABCCXM","AAAAJVMF","AAAAJVMG","AAABKRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVKH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  "AAAAJVMF" varchar(8) NOT NULL default ' ',
  "AAAAJVMG" varchar(4) NOT NULL default '0000',
  "AAAAJVMH" varchar(24) NOT NULL default ' ',
  "AAABHIKN" varchar(8) NOT NULL default '00000000',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABLFYJ" varchar(6) NOT NULL default '000000',
  "AAABHGYA" varchar(6) NOT NULL default '000000',
  "AAABFQTJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABCCXM","AAAAJVMF","AAAAJVMG","AAAAJVMH"),
  KEY "AAAAJVKZ" ("AAABLUXQ","AAABLUSV","AAABCCXM","AAAAJVMF","AAAAJVMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVLF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCCXM" varchar(3) NOT NULL default ' ',
  "AAAAJVMF" varchar(8) NOT NULL default ' ',
  "AAAAJVMG" varchar(4) NOT NULL default '0000',
  "AAAAJVMH" varchar(24) NOT NULL default ' ',
  "AAABETFA" varchar(8) NOT NULL default '00000000',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABAARF" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAJVMM" varchar(4) NOT NULL default ' ',
  "AAAAJVMN" varchar(10) NOT NULL default ' ',
  "AAAAJVMO" varchar(1) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAJVLU" decimal(15,2) NOT NULL default '0.00',
  "AAAAJVLV" decimal(15,2) NOT NULL default '0.00',
  "AAAAJVLW" varchar(5) NOT NULL default ' ',
  "AAAAJVLX" decimal(15,2) NOT NULL default '0.00',
  "AAAAJVLY" decimal(15,2) NOT NULL default '0.00',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAKARO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABCCXM","AAAAJVMF","AAAAJVMG","AAAAJVMH","AAABETFA","AAABKGOS","AAABAARF","AAABLTXI","AAAAJVMM","AAAAJVMN","AAAAJVMO","AAABIFOV","AAABKRIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVMR" (
  "AAAAJVNA" varchar(64) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAJVMU" varchar(1) NOT NULL default ' ',
  "AAABLENR" varchar(255) NOT NULL default ' ',
  "AAAARFCO" varchar(12) NOT NULL default ' ',
  "AAAAQWAQ" varchar(8) NOT NULL default '00000000',
  "AAAAQWAR" varchar(12) NOT NULL default ' ',
  "AAAAQWAS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAJVNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJVNX" varchar(12) NOT NULL default ' ',
  "AAAAJVNO" varchar(20) NOT NULL default ' ',
  "AAAAJVNF" varchar(40) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJVNX"),
  KEY "AAAAJVNN" ("AAAAJVNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJVNX" varchar(12) NOT NULL default ' ',
  "AAABMLTD" int(11) NOT NULL default '0',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAABMLFX" varchar(20) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJVNX","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAABMMVO" int(11) NOT NULL default '0',
  "AAAAOPUN" varchar(1) NOT NULL default ' ',
  "AAAAMWQI" varchar(8) NOT NULL default '00000000',
  "AAAAMQZE" varchar(6) NOT NULL default '000000',
  "AAAAJVOW" varchar(11) NOT NULL default '00000000000',
  "AAAAJVOH" varchar(8) NOT NULL default '00000000',
  "AAAAJVOI" varchar(6) NOT NULL default '000000',
  "AAAAJVOV" varchar(11) NOT NULL default '00000000000',
  "AAABIFOV" varchar(12) NOT NULL default ' ',
  "AAAAJVOL" varchar(8) NOT NULL default '00000000',
  "AAAAJVOM" varchar(12) NOT NULL default ' ',
  "AAABDROJ" varchar(1) NOT NULL default ' ',
  "AAABFQWU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCP","AAABMMVO"),
  KEY "AAAAJVOS" ("AAABMRIL","AAABIFOV","AAAAJVOV","AAAAJVOW","AAAAOPUN","AAABMLCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAAAJVPE" varchar(4) NOT NULL default '0000',
  "AAAAJVPF" varchar(4) NOT NULL default '0000',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABKOES" varchar(1) NOT NULL default ' ',
  "AAABKRIG" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKRFN" varchar(8) NOT NULL default '00000000',
  "AAABKRKS" varchar(25) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAJVPQ" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABGIZI" varchar(3) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABLXDX" varchar(1) NOT NULL default ' ',
  "AAABCDTH" varchar(1) NOT NULL default ' ',
  "AAABMAKT" varchar(8) NOT NULL default '00000000',
  "AAAAJVRP" varchar(8) NOT NULL default '00000000',
  "AAABCCSB" varchar(1) NOT NULL default ' ',
  "AAAAJVQE" varchar(10) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABKOVG" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAJVQI" varchar(5) NOT NULL default ' ',
  "AAABLCND" varchar(20) NOT NULL default ' ',
  "AAABLCNE" varchar(20) NOT NULL default ' ',
  "AAABLCNF" varchar(10) NOT NULL default ' ',
  "AAABLCNG" varchar(10) NOT NULL default ' ',
  "AAABLCNH" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNI" varchar(3) NOT NULL default ' ',
  "AAABLCNJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNK" varchar(3) NOT NULL default ' ',
  "AAABLCNL" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNM" varchar(5) NOT NULL default ' ',
  "AAABLCNN" decimal(13,3) NOT NULL default '0.000',
  "AAABLCNO" varchar(5) NOT NULL default ' ',
  "AAABLCNP" varchar(8) NOT NULL default '00000000',
  "AAABLCNQ" varchar(8) NOT NULL default '00000000',
  "AAABLCNR" varchar(1) NOT NULL default ' ',
  "AAABLCNS" varchar(1) NOT NULL default ' ',
  "AAABILSX" varchar(6) NOT NULL default ' ',
  "AAABILTM" varchar(6) NOT NULL default ' ',
  "AAAAJVRB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD"),
  UNIQUE KEY "AAAAJVRE" ("AAABMRIL","AAABKRJY","AAABCCXV","AAABMDBB"),
  UNIQUE KEY "AAAAJVRJ" ("AAABMRIL","AAABLXTG"),
  KEY "AAAAJVRM" ("AAABMRIL","AAABMAKT","AAAAJVRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAAAJVUH" varchar(22) NOT NULL default ' ',
  "AAAAUNUN" varchar(20) NOT NULL default ' ',
  "AAAAJVSM" varchar(1) NOT NULL default ' ',
  "AAABBACF" varchar(40) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  "AAAAWSPH" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAAAJVSZ" varchar(18) NOT NULL default ' ',
  "AAAAJWSJ" varchar(12) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAAAJVTC" varchar(1) NOT NULL default ' ',
  "AAABIZSL" smallint(6) NOT NULL default '0',
  "AAABIIXH" smallint(6) NOT NULL default '0',
  "AAAAJVTF" double NOT NULL default '0',
  "AAAAJVTG" varchar(1) NOT NULL default ' ',
  "AAAAJVTH" varchar(1) NOT NULL default ' ',
  "AAAAJVTI" varchar(40) NOT NULL default ' ',
  "AAAAJVTJ" varchar(1) NOT NULL default ' ',
  "AAAAJVTK" double NOT NULL default '0',
  "AAAAJVTL" varchar(1) NOT NULL default ' ',
  "AAAAJVTM" double NOT NULL default '0',
  "AAAAJVTN" varchar(1) NOT NULL default ' ',
  "AAAAJVTO" varchar(3) NOT NULL default ' ',
  "AAABBDMU" varchar(1) NOT NULL default ' ',
  "AAAAJVTQ" varchar(1) NOT NULL default ' ',
  "AAAAJVTR" varchar(1) NOT NULL default ' ',
  "AAABIFOV" varchar(12) NOT NULL default ' ',
  "AAAAJVTT" double NOT NULL default '0',
  "AAAAJVTU" varchar(1) NOT NULL default ' ',
  "AAABHEHA" double NOT NULL default '0',
  "AAAAJVTW" varchar(1) NOT NULL default ' ',
  "AAAAWSPN" varchar(1) NOT NULL default ' ',
  "AAAAJVXN" varchar(8) NOT NULL default ' ',
  "AAAAJVTZ" varchar(1) NOT NULL default ' ',
  "AAAAJVUA" varchar(1) NOT NULL default ' ',
  "AAAAJVUB" varchar(1) NOT NULL default ' ',
  "AAAAJVUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCP"),
  KEY "AAAAJVUF" ("AAABMRIL","AAAAJVUH","AAAAUNUN"),
  KEY "AAAAJVUJ" ("AAABMRIL","AAABLUSC"),
  KEY "AAAAJVUM" ("AAABMRIL","AAAAUNUN","AAABFCUI"),
  KEY "AAAAJVUQ" ("AAABMRIL","AAAAJWSJ"),
  KEY "AAAAJVUT" ("AAABMRIL","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRFF" varchar(40) NOT NULL default ' ',
  "AAAANLCH" varchar(40) NOT NULL default ' ',
  "AAAAJVZV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAAAJVVS" varchar(24) NOT NULL default ' ',
  "AAAAJVVN" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABCCXV","AAABMDBB","AAABKRJY","AAAAJVVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWZEB" varchar(1) NOT NULL default ' ',
  "AAAAWZEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQJAG" varchar(20) NOT NULL default ' ',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  "AAAAJVYD" varchar(11) NOT NULL default '00000000000',
  "AAAAJVWF" varchar(1) NOT NULL default ' ',
  "AAAAZKIX" varchar(40) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAJVWJ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABJRYM" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABDROJ" varchar(1) NOT NULL default ' ',
  "AAABKNTE" varchar(12) NOT NULL default '000000000000',
  "AAABCVFP" varchar(8) NOT NULL default '00000000',
  "AAAASRIR" varchar(4) NOT NULL default '0000',
  "AAAAQKKU" varchar(8) NOT NULL default '00000000',
  "AAAAJVZN" varchar(20) NOT NULL default ' ',
  "AAAAJVWW" varchar(2) NOT NULL default ' ',
  "AAAAJVWX" varchar(8) NOT NULL default '00000000',
  "AAAAJVYJ" varchar(22) NOT NULL default ' ',
  "AAAAJVWZ" varchar(1) NOT NULL default ' ',
  "AAAAJVXA" double NOT NULL default '0',
  "AAAAJVXB" varchar(1) NOT NULL default ' ',
  "AAAAJVXC" double NOT NULL default '0',
  "AAAAJVXD" varchar(1) NOT NULL default ' ',
  "AAAAJVXE" varchar(3) NOT NULL default ' ',
  "AAAAJVXF" double NOT NULL default '0',
  "AAAAJVXG" varchar(1) NOT NULL default ' ',
  "AAAAJVXH" double NOT NULL default '0',
  "AAAAJVXI" varchar(1) NOT NULL default ' ',
  "AAAAJVXJ" varchar(1) NOT NULL default ' ',
  "AAAAJVXK" varchar(1) NOT NULL default ' ',
  "AAAAJVXL" varchar(1) NOT NULL default ' ',
  "AAAAWSPN" varchar(1) NOT NULL default ' ',
  "AAAAJVXN" varchar(8) NOT NULL default ' ',
  "AAAAJVXO" varchar(4) NOT NULL default ' ',
  "AAAAUEXS" varchar(6) NOT NULL default ' ',
  "AAAAJVXQ" varchar(1) NOT NULL default ' ',
  "AAAAOPUN" varchar(1) NOT NULL default ' ',
  "AAAAJVXS" varchar(22) NOT NULL default ' ',
  "AAAAQOFS" varchar(6) NOT NULL default '000000',
  "AAABGOQL" varchar(1) NOT NULL default ' ',
  "AAAAOTKE" varchar(1) NOT NULL default ' ',
  "AAABKOTM" varchar(8) NOT NULL default '00000000',
  "AAAAJVXX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAQJAG"),
  KEY "AAAAJVYA" ("AAABMRIL","AAABMLCP","AAAAJVYD"),
  KEY "AAAAJVYE" ("AAABMRIL","AAAAJVZN"),
  KEY "AAAAJVYH" ("AAABMRIL","AAAAJVYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRFN" varchar(8) NOT NULL default '00000000',
  "AAABKRKS" varchar(25) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKRFV" varchar(4) NOT NULL default ' ',
  "AAAAJVYT" varchar(8) NOT NULL default '00000000',
  "AAAAJVYU" varchar(10) NOT NULL default ' ',
  "AAAAJVYV" varchar(1) NOT NULL default ' ',
  "AAABCCSI" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJVYY" varchar(4) NOT NULL default ' ',
  "AAAAJVYZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCCXV","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  "AAAAJVZN" varchar(20) NOT NULL default ' ',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGWVP","AAABGWVQ","AAAAJVZN","AAABMLCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJVZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRFF" varchar(40) NOT NULL default ' ',
  "AAAAJVZV" varchar(1) NOT NULL default ' ',
  "AAAANLCH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCCXV","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABCCTZ" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAJWAH" decimal(5,2) NOT NULL default '0.00',
  "AAAAJWAI" decimal(5,2) NOT NULL default '0.00',
  "AAAAJWAJ" varchar(1) NOT NULL default ' ',
  "AAAAJWAK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABLXTG","AAABCCTZ","AAABMDBB"),
  KEY "AAAAJWAQ" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABLXTG","AAABMDBB","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMLNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAMLNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAABLATT" varchar(10) NOT NULL default ' ',
  "AAAAJWBW" varchar(8) NOT NULL default '00000000',
  "AAABFTKZ" decimal(15,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABKYTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  "AAABLATG" varchar(8) NOT NULL default '00000000',
  "AAABKYSY" decimal(15,6) NOT NULL default '0.000000',
  "AAABLYAG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYTL","AAAAMLNO","AAABLATG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAAAMLNO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYTL","AAAAMLNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWCS" (
  "AAABMLPV" varchar(81) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLFTW" decimal(14,0) NOT NULL default '0',
  "AAAAJWCX" decimal(14,0) NOT NULL default '0',
  "AAAAJWCY" decimal(14,0) NOT NULL default '0',
  "AAAAJWCZ" decimal(14,0) NOT NULL default '0',
  "AAAAJWDA" decimal(14,0) NOT NULL default '0',
  "AAAAJWDB" decimal(14,0) NOT NULL default '0',
  "AAAAJWDC" decimal(14,0) NOT NULL default '0',
  "AAAAJWDD" decimal(14,0) NOT NULL default '0',
  "AAAAJWDE" decimal(14,0) NOT NULL default '0',
  "AAAAJWDF" decimal(14,0) NOT NULL default '0',
  "AAAAJWDG" decimal(14,0) NOT NULL default '0',
  "AAAAJWDH" decimal(14,0) NOT NULL default '0',
  "AAAAJWDI" decimal(14,0) NOT NULL default '0',
  "AAAAJWDJ" decimal(14,0) NOT NULL default '0',
  "AAAAJWDK" decimal(14,0) NOT NULL default '0',
  "AAAAJWDL" decimal(14,0) NOT NULL default '0',
  "AAAAJWDM" decimal(14,0) NOT NULL default '0',
  "AAAAJWDN" decimal(14,0) NOT NULL default '0',
  "AAAAJWDO" decimal(14,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWDQ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  "AAABLJYP" varchar(12) NOT NULL default ' ',
  "AAAAJWDV" varchar(10) NOT NULL default ' ',
  "AAAAJWDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWDY" (
  "AAAAJWET" varchar(1) NOT NULL default ' ',
  "AAAAJWEU" varchar(5) NOT NULL default ' ',
  "AAAAJWEB" varchar(2) NOT NULL default '00',
  "AAAAJWEC" varchar(1) NOT NULL default ' ',
  "AAABJTCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJWET","AAAAJWEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWEG" (
  "AAAAJWET" varchar(1) NOT NULL default ' ',
  "AAAAJWEU" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLEQR" varchar(6) NOT NULL default '000000',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABFWLV" varchar(1) NOT NULL default ' ',
  "AAAAJWEQ" varchar(1) NOT NULL default ' ',
  "AAAAJWER" varchar(1) NOT NULL default ' ',
  "AAAAJWES" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJWET","AAAAJWEU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWEW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAATKME" varchar(30) NOT NULL default ' ',
  "AAAATKMG" varchar(30) NOT NULL default ' ',
  "AAABHLCF" varchar(1) NOT NULL default ' ',
  "AAAAUXJC" varchar(1) NOT NULL default ' ',
  "AAABCKPA" varchar(8) NOT NULL default '00000000',
  "AAAATKMK" varchar(12) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABIKRU" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABFVFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMNML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWFP" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABEWKE" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAAJWHF" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(10) NOT NULL default '0000000000',
  "AAAAJWGS" longblob NOT NULL,
  "AAAAJWGT" varchar(132) NOT NULL default ' ',
  "AAAAJWGU" varchar(132) NOT NULL default ' ',
  "AAAAJWGV" varchar(2) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAJWGX" varchar(3) NOT NULL default '000',
  "AAAAJWGY" varchar(3) NOT NULL default ' ',
  "AAAAJWGZ" longtext,
  "AAAAJWHA" longtext,
  PRIMARY KEY ("AAABLSCE","AAABEWKE","AAABMREL","AAABMIZS","AAAAJWHF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWGL" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABEWKE" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAAJWHF" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(10) NOT NULL default '0000000000',
  "AAAAJWGS" longblob NOT NULL,
  "AAAAJWGT" varchar(132) NOT NULL default ' ',
  "AAAAJWGU" varchar(132) NOT NULL default ' ',
  "AAAAJWGV" varchar(2) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAJWGX" varchar(3) NOT NULL default '000',
  "AAAAJWGY" varchar(3) NOT NULL default ' ',
  "AAAAJWGZ" longtext,
  "AAAAJWHA" longtext,
  PRIMARY KEY ("AAABLSCE","AAABEWKE","AAABMREL","AAABMIZS","AAAAJWHF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJWHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJWHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWIE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMBYX" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWIQ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWJX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWKG" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABFLQL" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAJWKV" varchar(22) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABFLQL","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPTRY" varchar(18) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAAAWSXM" varchar(8) NOT NULL default ' ',
  "AAAAJWLI" varchar(1) NOT NULL default ' ',
  "AAAAJWNB" varchar(30) NOT NULL default ' ',
  "AAAAJWMR" varchar(18) NOT NULL default ' ',
  "AAAAJWNA" varchar(30) NOT NULL default ' ',
  "AAAAJWMQ" varchar(18) NOT NULL default ' ',
  "AAAAJWLN" varchar(4) NOT NULL default '0000',
  "AAAAJWLO" varchar(8) NOT NULL default '00000000',
  "AAAAJWLP" varchar(6) NOT NULL default '000000',
  "AAAAJWLQ" varchar(8) NOT NULL default '00000000',
  "AAAAJWLR" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(22) NOT NULL default ' ',
  "AAAAJWLT" varchar(30) NOT NULL default ' ',
  "AAAAJWLU" varchar(18) NOT NULL default ' ',
  "AAAAWSYA" varchar(6) NOT NULL default ' ',
  "AAAAJWLW" varchar(1) NOT NULL default ' ',
  "AAAAJWLX" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAAAWSPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPTRY"),
  UNIQUE KEY "AAAAJWMH" ("AAABMRIL","AAABMRFO"),
  KEY "AAAAJWMK" ("AAAAJWMR","AAAAJWMQ","AAAAWSXM","AAAAWSYA"),
  KEY "AAAAJWMP" ("AAAAJWMQ","AAAAJWMR","AAAAWSXM","AAAAWSYA"),
  KEY "AAAAJWMU" ("AAAAJWNB","AAAAJWNA","AAAAWSXM","AAAAWSYA"),
  KEY "AAAAJWMZ" ("AAAAJWNA","AAAAJWNB","AAAAWSXM","AAAAWSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPTRY" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAJWNR" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAQPGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPTRY","AAABMRHS"),
  KEY "AAAAJWNO" ("AAABMRIL","AAABMRHS","AAAAJWNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWNS" (
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAAAJWNU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWNW" (
  "AAAAJWOL" varchar(50) NOT NULL default ' ',
  "AAAAJWOM" smallint(6) NOT NULL default '0',
  "AAAAJWNZ" varchar(40) NOT NULL default ' ',
  "AAABDOOL" varchar(40) NOT NULL default ' ',
  "AAAAJWOB" int(11) NOT NULL default '0',
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABHZBX" varchar(20) NOT NULL default ' ',
  "AAABMLDW" int(11) NOT NULL default '0',
  "AAAAJWOF" varchar(120) NOT NULL default ' ',
  "AAAAZVCK" varchar(50) NOT NULL default ' ',
  "AAAAZVCL" varchar(50) NOT NULL default ' ',
  "AAAAZVCM" varchar(50) NOT NULL default ' ',
  "AAAAJWOJ" varchar(50) NOT NULL default ' ',
  "AAAAJWOK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJWOL","AAAAJWOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWON" (
  "AAABIABC" int(11) NOT NULL default '0',
  "AAABMMRH" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABIABC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWOR" (
  "AAAAJWPY" int(11) NOT NULL default '0',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAAAJWRC" varchar(1) NOT NULL default ' ',
  "AAAAJWRB" smallint(6) NOT NULL default '0',
  "AAAAJWPK" varchar(1) NOT NULL default ' ',
  "AAAAJWQK" varchar(10) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAAAJWQO" varchar(2) NOT NULL default ' ',
  "AAAAJWQP" varchar(50) NOT NULL default ' ',
  "AAAAJWPB" varchar(50) NOT NULL default ' ',
  "AAAAJWQQ" varchar(1) NOT NULL default ' ',
  "AAABINIK" varchar(14) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABJVWR" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJWPY","AAABMJWN","AAAAJWRC","AAAAJWRB","AAAAJWPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWPL" (
  "AAAAJWPZ" varchar(14) NOT NULL default ' ',
  "AAAAJWPN" varchar(50) NOT NULL default ' ',
  "AAABGNEW" varchar(4) NOT NULL default ' ',
  "AAAAJWQV" varchar(10) NOT NULL default ' ',
  "AAAAJWQW" varchar(10) NOT NULL default ' ',
  "AAAAJWQX" varchar(10) NOT NULL default ' ',
  "AAAAJWPS" varchar(6) NOT NULL default ' ',
  "AAAAJWPT" varchar(1) NOT NULL default ' ',
  "AAABHEYE" smallint(6) NOT NULL default '0',
  "AAAAJWPV" smallint(6) NOT NULL default '0',
  "AAABLETY" varchar(3) NOT NULL default ' ',
  "AAABJVWR" varchar(80) NOT NULL default ' ',
  "AAAAJWPY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAJWPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWQA" (
  "AAABGNEW" varchar(4) NOT NULL default ' ',
  "AAAAJWQV" varchar(10) NOT NULL default ' ',
  "AAAAJWQW" varchar(10) NOT NULL default ' ',
  "AAAAJWQX" varchar(10) NOT NULL default ' ',
  "AAABINIK" varchar(14) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAAAJWRB" smallint(6) NOT NULL default '0',
  "AAAAJWRC" varchar(1) NOT NULL default ' ',
  "AAAAJWQK" varchar(10) NOT NULL default ' ',
  "AAABKWRQ" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAAAJWQO" varchar(2) NOT NULL default ' ',
  "AAAAJWQP" varchar(50) NOT NULL default ' ',
  "AAAAJWQQ" varchar(1) NOT NULL default ' ',
  "AAAAJWQR" varchar(1) NOT NULL default ' ',
  "AAABLEET" varchar(80) NOT NULL default ' ',
  "AAABBZHR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNEW","AAAAJWQV","AAAAJWQW","AAAAJWQX","AAABINIK","AAABLEES","AAABMJWN","AAAAJWRB","AAAAJWRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWRD" (
  "AAAAJWRS" int(11) NOT NULL default '0',
  "AAAAJWRT" int(11) NOT NULL default '0',
  "AAAAYZCF" int(11) NOT NULL default '0',
  "AAABMREL" varchar(18) NOT NULL default ' ',
  "AAABHVTW" varchar(18) NOT NULL default ' ',
  "AAAAJWRJ" varchar(6) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAABHAHM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJWRM" varchar(1) NOT NULL default ' ',
  "AAABFVIF" varchar(1) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAAAJWRP" int(11) NOT NULL default '0',
  "AAAAJWRQ" int(11) NOT NULL default '0',
  "AAABKAHC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAJWRS","AAAAJWRT","AAAAYZCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJWSB" varchar(4) NOT NULL default ' ',
  "AAAAJWSC" varchar(30) NOT NULL default ' ',
  "AAAAJWRZ" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAAAJWSB","AAAAJWSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAAAJWSJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCP","AAAAJWSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAAAJWTO" varchar(30) NOT NULL default ' ',
  "AAAAJWTP" varchar(50) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAAAJWSU" varchar(4) NOT NULL default '0000',
  "AAABLTQF" varchar(18) NOT NULL default '000000000000000000',
  "AAABIJNF" varchar(18) NOT NULL default '000000000000000000',
  "AAAAJWSX" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTN"),
  KEY "AAAAJWTD" ("AAABMRIL","AAABGIHK","AAABDRQC","AAABLTUX"),
  KEY "AAAAJWTI" ("AAABMRIL","AAABLXUH","AAABGIHK"),
  KEY "AAAAJWTM" ("AAABMRIL","AAAAJWTO","AAAAJWTP","AAABGIHK"),
  KEY "AAAAJWTR" ("AAABMRIL","AAABLTQF"),
  KEY "AAAAJWTU" ("AAABMRIL","AAABIJNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWTX" (
  "AAAAJWUB" varchar(8) NOT NULL default '00000000',
  "AAAAJWUC" varchar(50) NOT NULL default ' ',
  "AAAAJWUD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJWUB","AAAAJWUC","AAAAJWUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWUE" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWUL" (
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAAJWUT" varchar(3) NOT NULL default '000',
  "AAAAUZKI" varchar(1) NOT NULL default ' ',
  "AAAAJWUQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIISW","AAABMKGJ","AAAAJWUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWUU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAAJWUY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIISW","AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWVC" (
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAJWVF" varchar(40) NOT NULL default ' ',
  "AAABGFXP" varchar(30) NOT NULL default ' ',
  "AAAAJWVH" varchar(10) NOT NULL default ' ',
  "AAAAJWVI" varchar(1) NOT NULL default ' ',
  "AAAAJWVJ" varchar(1) NOT NULL default ' ',
  "AAAAJWVK" varchar(1) NOT NULL default ' ',
  "AAAAJWVL" varchar(1) NOT NULL default ' ',
  "AAAAJWVM" varchar(1) NOT NULL default ' ',
  "AAAAJWVN" varchar(60) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABCECN" varchar(8) NOT NULL default '00000000',
  "AAABCECM" varchar(6) NOT NULL default '000000',
  "AAABCECO" varchar(12) NOT NULL default ' ',
  "AAABEBUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIISW","AAABLQVJ"),
  KEY "AAAAJWVV" ("AAABIISW","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWVY" (
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIISW","AAABLQVJ","AAABLTJH","AAABLTJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWWH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAYKRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIISW","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWWP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIGLF" varchar(6) NOT NULL default ' ',
  "AAABIGLE" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAJWWU" varchar(1) NOT NULL default ' ',
  "AAAAJWWV" varchar(128) NOT NULL default ' ',
  "AAAAJWWW" varchar(40) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIGLF","AAABIGLE","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWXD" (
  "AAABGFMQ" varchar(64) NOT NULL default ' ',
  "AAAAVNFC" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWXH" (
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAAATUHB" varchar(10) NOT NULL default ' ',
  "AAAAJWXK" varchar(10) NOT NULL default ' ',
  "AAABLENH" varchar(2) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABHBVI" varchar(32) NOT NULL default ' ',
  "AAAAJWXO" varchar(32) NOT NULL default ' ',
  "AAABGHXN" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" int(11) NOT NULL default '0',
  "AAABLJJN" varchar(10) NOT NULL default ' ',
  "AAABHMNR" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJWYN" varchar(36) NOT NULL default ' ',
  "AAAAJWYA" varchar(10) NOT NULL default ' ',
  "AAAAJWYB" varchar(30) NOT NULL default ' ',
  "AAAAJWYC" varchar(6) NOT NULL default ' ',
  "AAAAJWYD" varchar(10) NOT NULL default ' ',
  "AAAAJWYE" varchar(30) NOT NULL default ' ',
  "AAAAJWYF" varchar(6) NOT NULL default ' ',
  "AAAAJWYG" varchar(60) NOT NULL default ' ',
  "AAAAJWYH" varchar(10) NOT NULL default ' ',
  "AAAAJWYI" varchar(12) NOT NULL default ' ',
  "AAABLQJI" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAAARISU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJWYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQM" varchar(36) NOT NULL default ' ',
  "AAAAJWYR" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWYU" (
  "AAAAPQWN" varchar(30) NOT NULL default ' ',
  "AAAAJWZK" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAAAJWZC" varchar(1) NOT NULL default ' ',
  "AAABGYNY" varchar(1) NOT NULL default ' ',
  "AAAAJWZE" varchar(1) NOT NULL default ' ',
  "AAAAJWZF" varchar(4) NOT NULL default ' ',
  "AAAAJWZG" varchar(1) NOT NULL default ' ',
  "AAAAJWZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPQWN"),
  KEY "AAAAJWZJ" ("AAAAJWZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWZL" (
  "AAAAPQWN" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPQWN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJWZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGZJM" varchar(10) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAJWZZ" varchar(8) NOT NULL default '00000000',
  "AAAAJXAA" varchar(8) NOT NULL default '00000000',
  "AAAATHWV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAAAJXAH" ("AAABMRIL","AAABMREX","AAABGZJM","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAJXBZ" varchar(8) NOT NULL default '00000000',
  "AAAAJXAT" varchar(8) NOT NULL default '00000000',
  "AAAAJXAU" decimal(4,0) NOT NULL default '0',
  "AAAAJXAV" varchar(10) NOT NULL default ' ',
  "AAAAJXAW" varchar(5) NOT NULL default ' ',
  "AAAAWELD" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAJXCU" varchar(5) NOT NULL default ' ',
  "AAAAJXCV" decimal(15,2) NOT NULL default '0.00',
  "AAAAJXCW" varchar(1) NOT NULL default ' ',
  "AAABKIRF" decimal(15,2) NOT NULL default '0.00',
  "AAAAJXCY" varchar(1) NOT NULL default ' ',
  "AAAAJXDE" varchar(8) NOT NULL default '00000000',
  "AAAAJXCZ" varchar(8) NOT NULL default '00000000',
  "AAAAJXDA" varchar(6) NOT NULL default '000000',
  "AAAAJXDB" varchar(12) NOT NULL default ' ',
  "AAABGZJM" varchar(10) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAJXCT" varchar(30) NOT NULL default ' ',
  "AAAAJXDG" varchar(4) NOT NULL default ' ',
  "AAABFMVU" varchar(10) NOT NULL default ' ',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAAAJXBN" varchar(3) NOT NULL default '000',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAJXCS" varchar(1) NOT NULL default ' ',
  "AAAAJXDD" varchar(8) NOT NULL default '00000000',
  "AAAAJXDH" varchar(10) NOT NULL default ' ',
  "AAAAJXBS" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAJXBT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAAAJXBZ"),
  KEY "AAAAJXCA" ("AAABMRIL","AAABMREX","AAABGZJM","AAABMPWP","AAAAJXDE","AAAAJXCT"),
  KEY "AAAAJXCH" ("AAABMRIL","AAAAJXDG","AAABFMVU","AAABLHZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJXDG" varchar(4) NOT NULL default ' ',
  "AAAAJXDH" varchar(10) NOT NULL default ' ',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAJXCS" varchar(1) NOT NULL default ' ',
  "AAAAJXCT" varchar(30) NOT NULL default ' ',
  "AAAAJXCU" varchar(5) NOT NULL default ' ',
  "AAAAJXCV" decimal(15,2) NOT NULL default '0.00',
  "AAAAJXCW" varchar(1) NOT NULL default ' ',
  "AAABKIRF" decimal(15,2) NOT NULL default '0.00',
  "AAAAJXCY" varchar(1) NOT NULL default ' ',
  "AAAAJXCZ" varchar(8) NOT NULL default '00000000',
  "AAAAJXDA" varchar(6) NOT NULL default '000000',
  "AAAAJXDB" varchar(12) NOT NULL default ' ',
  "AAABFMVU" varchar(10) NOT NULL default ' ',
  "AAAAJXDD" varchar(8) NOT NULL default '00000000',
  "AAAAJXDE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAJXDG","AAAAJXDH","AAABLHZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABGCFE" varchar(3) NOT NULL default '000',
  "AAAAJXDN" varchar(40) NOT NULL default ' ',
  "AAAAJXDO" varchar(40) NOT NULL default ' ',
  "AAAAJXDP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR","AAABGCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABGCFE" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(40) NOT NULL default ' ',
  "AAABEFTK" varchar(40) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR","AAABGCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABGCFE" varchar(3) NOT NULL default '000',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR","AAABGCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAJXFL" varchar(2) NOT NULL default '00',
  "AAAAJXFK" varchar(20) NOT NULL default ' ',
  "AAAAVBJH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(40) NOT NULL default ' ',
  "AAABLEKT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLHQ","AAABLLIM","AAAAJXFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAJXFK" varchar(20) NOT NULL default ' ',
  "AAAAJXFL" varchar(2) NOT NULL default '00',
  "AAAAVBJH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(40) NOT NULL default ' ',
  "AAABLEKT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQWU","AAABMCVI","AAAAJXFK","AAAAJXFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXFM" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAAJXFU" varchar(8) NOT NULL default '00000000',
  "AAAAJXFV" varchar(1) NOT NULL default ' ',
  "AAAAJXFW" varchar(1) NOT NULL default ' ',
  "AAAAJXFX" varchar(1) NOT NULL default ' ',
  "AAAAJXFY" varchar(1) NOT NULL default ' ',
  "AAAAJXFZ" varchar(1) NOT NULL default ' ',
  "AAAAJXGA" varchar(1) NOT NULL default ' ',
  "AAAAJXGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILZK" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXGM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAMSTY" VARBINARY(255) NOT NULL,
  "AAAAJXGV" VARBINARY(255) NOT NULL,
  "AAAAJXGW" VARBINARY(255) NOT NULL,
  "AAABJCYE" VARBINARY(255),
  "AAAAJXGS" decimal(8,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABLUXQ","AAAAMSTY"(255),"AAAAJXGV"(255),"AAAAJXGW"(255)),
  KEY "AAAAJXGX" ("AAABJCYE"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEOJJ","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEOJJ","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXHT" (
  "AAAAJXIY" varchar(30) NOT NULL default ' ',
  "AAAAJXHV" varchar(24) NOT NULL default ' ',
  "AAAAJXIU" varchar(1) NOT NULL default ' ',
  "AAABFQOM" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAAAJXHZ" varchar(20) NOT NULL default ' ',
  "AAAAJXIA" varchar(1) NOT NULL default ' ',
  "AAAAJXJA" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAAAJXII" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAJXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXIK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJXIY" varchar(30) NOT NULL default ' ',
  "AAAAJXIN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAJXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXIQ" (
  "AAABGSHN" varchar(30) NOT NULL default ' ',
  "AAAAJXIS" varchar(14) NOT NULL default ' ',
  "AAAAJXIT" varchar(12) NOT NULL default ' ',
  "AAAAJXIU" varchar(1) NOT NULL default ' ',
  "AAABBRJH" varchar(5) NOT NULL default ' ',
  "AAABHIOL" varchar(30) NOT NULL default ' ',
  "AAAAJXIX" varchar(14) NOT NULL default ' ',
  "AAAAJXIY" varchar(30) NOT NULL default ' ',
  "AAAAJXIZ" varchar(1) NOT NULL default ' ',
  "AAAAJXJA" varchar(30) NOT NULL default ' ',
  "AAAAJXJB" varchar(24) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAAAJXJI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGSHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXJK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGSHN" varchar(30) NOT NULL default ' ',
  "AAAAXKZP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGSHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMIDK" varchar(30) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABKVKH" varchar(1) NOT NULL default ' ',
  "AAAAJXKY" varchar(30) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAAASHIU" varchar(18) NOT NULL default ' ',
  "AAAAMIDL" varchar(30) NOT NULL default ' ',
  "AAAAMIDM" varchar(1) NOT NULL default ' ',
  "AAAAJXKG" varchar(1) NOT NULL default ' ',
  "AAABFTLZ" varchar(1) NOT NULL default ' ',
  "AAAAJXKI" varchar(1) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAJXKK" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAJXKM" varchar(1) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAAAMIDO" varchar(8) NOT NULL default '00000000',
  "AAAAJXKP" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAJXKR" varchar(1) NOT NULL default ' ',
  "AAAAWSIV" varchar(1) NOT NULL default ' ',
  "AAAAJXKT" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMIDK"),
  KEY "AAAAJXKX" ("AAAAJXKY"),
  KEY "AAAAJXKZ" ("AAABMRIL","AAAASHIU"),
  KEY "AAAAJXLC" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMIDK" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAATI" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAQPGN" varchar(1) NOT NULL default ' ',
  "AAAAJXLR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMIDK","AAABMRHS"),
  KEY "AAAAJXLQ" ("AAAAJXLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXLS" (
  "AAABIWUT" varchar(30) NOT NULL default ' ',
  "AAABLJNI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABIWUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAWKZV" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABKROS" varchar(4) NOT NULL default '0000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABKRNY" varchar(10) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAJXMP" varchar(12) NOT NULL default ' ',
  "AAAAWKZX" varchar(8) NOT NULL default '00000000',
  "AAAAJXMR" varchar(12) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAWKZW" varchar(16) NOT NULL default ' ',
  "AAAAUASM" varchar(1) NOT NULL default ' ',
  "AAAAUASN" varchar(1) NOT NULL default ' ',
  "AAAAJXMW" varchar(1) NOT NULL default ' ',
  "AAABKRNP" varchar(1) NOT NULL default ' ',
  "AAAAWLAP" varchar(1) NOT NULL default ' ',
  "AAAAJXMZ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWLAE" varchar(1) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVL" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAAAJXNI" varchar(10) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAAAMWEI" decimal(13,2) NOT NULL default '0.00',
  "AAABLSDT" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNP" decimal(13,2) NOT NULL default '0.00',
  "AAAASTDX" varchar(1) NOT NULL default ' ',
  "AAAAMWFD" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNS" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNT" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNU" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNV" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXNW" decimal(13,2) NOT NULL default '0.00',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLZMW" varchar(4) NOT NULL default '0000',
  "AAAAJXNZ" varchar(18) NOT NULL default ' ',
  "AAAAJXOA" varchar(1) NOT NULL default ' ',
  "AAAAJXOB" varchar(8) NOT NULL default '00000000',
  "AAAAJXOC" varchar(6) NOT NULL default '000000',
  "AAAAJXOD" varchar(8) NOT NULL default '00000000',
  "AAAAJXOE" varchar(6) NOT NULL default '000000',
  "AAAAJXOF" decimal(13,3) NOT NULL default '0.000',
  "AAAAJXOG" decimal(13,2) NOT NULL default '0.00',
  "AAAAJXOH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCM","AAABMDBB","AAAAWKZV"),
  KEY "AAAAJXOM" ("AAABMRIL","AAABMCIM","AAABMCME","AAABMCMG"),
  KEY "AAAAJXOR" ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXOW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAAAJXPB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD","AAABMOHM","AAABKWVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAJXPY" varchar(4) NOT NULL default '0000',
  "AAAAWJNS" varchar(1) NOT NULL default ' ',
  "AAABFITE" varchar(8) NOT NULL default ' ',
  "AAAAJXPN" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAAAJXPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXPZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAJXRJ" varchar(4) NOT NULL default '0000',
  "AAAAJXQE" varchar(1) NOT NULL default ' ',
  "AAAAJXQF" varchar(255) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAAAJXRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAJXRJ" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJXQX" varchar(40) NOT NULL default ' ',
  "AAAAJYCS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAAAJXRJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAXPEN" varchar(4) NOT NULL default '0000',
  "AAAAWJNT" varchar(1) NOT NULL default ' ',
  "AAAAJXRR" varchar(8) NOT NULL default ' ',
  "AAABFLTS" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAAAXPEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXSE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAJYAW" varchar(4) NOT NULL default '0000',
  "AAAAWJPX" varchar(40) NOT NULL default ' ',
  "AAAAJYAL" varchar(40) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAAAJYAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXSW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABIXAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXTM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAAAJXUH" varchar(4) NOT NULL default '0000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAAAWJMZ" varchar(1) NOT NULL default ' ',
  "AAAAZCIY" varchar(8) NOT NULL default ' ',
  "AAAAJXTU" varchar(4) NOT NULL default ' ',
  "AAABKQPU" varchar(4) NOT NULL default '0000',
  "AAAAJXVL" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAAAJXUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXUI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAAAJXVL" varchar(4) NOT NULL default '0000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAAAWJMX" varchar(1) NOT NULL default ' ',
  "AAAAJXUP" varchar(8) NOT NULL default ' ',
  "AAAAJXUQ" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAAAJXVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXVC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABKQPU" varchar(4) NOT NULL default '0000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAAAZCOQ" varchar(1) NOT NULL default ' ',
  "AAABGAMX" varchar(8) NOT NULL default ' ',
  "AAAAZCNQ" varchar(4) NOT NULL default ' ',
  "AAAAJXVL" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABKQPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXVX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAALNWJ" varchar(40) NOT NULL default ' ',
  "AAAAJYCR" varchar(1) NOT NULL default ' ',
  "AAAAJYCS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABIXAW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXWS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAAAJXWZ" varchar(30) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABIXAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXXN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABEPOT" varchar(12) NOT NULL default ' ',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAAAWJPK" varchar(20) NOT NULL default ' ',
  "AAAAWJOX" varchar(2) NOT NULL default ' ',
  "AAAAJYBE" varchar(8) NOT NULL default '00000000',
  "AAAAJYBF" varchar(8) NOT NULL default '00000000',
  "AAAAWJMW" varchar(1) NOT NULL default ' ',
  "AAAAJXXY" varchar(8) NOT NULL default ' ',
  "AAAAJXXZ" varchar(4) NOT NULL default ' ',
  "AAABKNPA" varchar(2) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAAAJXYC" varchar(1) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWJMX" varchar(1) NOT NULL default ' ',
  "AAAAZCOQ" varchar(1) NOT NULL default ' ',
  "AAAAWJMZ" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABIXAW" varchar(4) NOT NULL default '0000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABIXAW","AAAAJYDB","AAAAJYDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJXZR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJXZR" varchar(40) NOT NULL default ' ',
  "AAAAJYCR" varchar(1) NOT NULL default ' ',
  "AAAAJYCS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYAF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYAU" varchar(12) NOT NULL default ' ',
  "AAAAJYAV" varchar(3) NOT NULL default '000',
  "AAAAJYAW" varchar(4) NOT NULL default '0000',
  "AAAAWJPX" varchar(40) NOT NULL default ' ',
  "AAAAJYAL" varchar(40) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYAU","AAAAJYAV","AAAAJYAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAAWJQO" varchar(2) NOT NULL default ' ',
  "AAAAJYBE" varchar(8) NOT NULL default '00000000',
  "AAAAJYBF" varchar(8) NOT NULL default '00000000',
  "AAABEPOT" varchar(12) NOT NULL default ' ',
  "AAAAJYBH" varchar(1) NOT NULL default ' ',
  "AAAAWJNT" varchar(1) NOT NULL default ' ',
  "AAAAWJNS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYBV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYCK" varchar(12) NOT NULL default ' ',
  "AAAAJYBY" varchar(1) NOT NULL default ' ',
  "AAAAJYBZ" varchar(1) NOT NULL default ' ',
  "AAAAJYCA" varchar(1) NOT NULL default ' ',
  "AAAAJYCB" varchar(1) NOT NULL default ' ',
  "AAAAJYCC" varchar(132) NOT NULL default ' ',
  "AAAAJYCD" varchar(255) NOT NULL default ' ',
  "AAAAJYCE" varchar(1) NOT NULL default ' ',
  "AAAAJYCF" varchar(1) NOT NULL default ' ',
  "AAAAJYCG" varchar(1) NOT NULL default ' ',
  "AAAAWJRE" varchar(2) NOT NULL default ' ',
  "AAAAWJQN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAWDFK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAJYDB" varchar(12) NOT NULL default ' ',
  "AAAAJYDC" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAJYCQ" varchar(40) NOT NULL default ' ',
  "AAAAJYCR" varchar(1) NOT NULL default ' ',
  "AAAAJYCS" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAAAJYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAJYDB","AAAAJYDC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYDE" (
  "AAABIDWD" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIFAN" varchar(5) NOT NULL default ' ',
  "AAABKZCF" varchar(3) NOT NULL default '000',
  "AAABKZCE" varchar(3) NOT NULL default '000',
  "AAAATLAW" varchar(2) NOT NULL default '00',
  "AAABCVPG" varchar(30) NOT NULL default ' ',
  "AAABDYWF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIDWD","AAABMREX","AAABIFAN","AAABKZCF","AAABKZCE","AAAATLAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYDT" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABEXFH" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAJZDE" varchar(2) NOT NULL default ' ',
  "AAABEVTQ" varchar(1) NOT NULL default ' ',
  "AAAAJYXL" varchar(1) NOT NULL default ' ',
  "AAAAJZDC" varchar(5) NOT NULL default ' ',
  "AAAAJZDD" varchar(2) NOT NULL default ' ',
  "AAABEXQH" varchar(1) NOT NULL default ' ',
  "AAAAJYXP" varchar(13) NOT NULL default ' ',
  "AAAAJZED" varchar(10) NOT NULL default ' ',
  "AAAAJZEB" varchar(3) NOT NULL default ' ',
  "AAAAJYXS" varchar(10) NOT NULL default ' ',
  "AAAAJYXT" varchar(8) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABGYLO" varchar(24) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAJYYH" varchar(24) NOT NULL default ' ',
  "AAAAJYYI" varchar(10) NOT NULL default ' ',
  "AAAAJYYJ" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAAAJYYL" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAAAJYYN" varchar(2) NOT NULL default ' ',
  "AAAAJYYO" varchar(1) NOT NULL default ' ',
  "AAAAJYYP" varchar(1) NOT NULL default ' ',
  "AAABLBYI" varchar(5) NOT NULL default ' ',
  "AAAAJYYR" varchar(2) NOT NULL default ' ',
  "AAAAJYYS" varchar(3) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(17,3) NOT NULL default '0.000',
  "AAABMPVS" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(6) NOT NULL default '000000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAAAJYKR" varchar(10) NOT NULL default ' ',
  "AAAAJYKS" varchar(10) NOT NULL default ' ',
  "AAAAJYGR" decimal(17,2) NOT NULL default '0.00',
  "AAAAJYGS" varchar(5) NOT NULL default ' ',
  "AAAAJYGT" varchar(15) NOT NULL default ' ',
  "AAAAJYGU" varchar(12) NOT NULL default ' ',
  "AAAAJYGV" varchar(4) NOT NULL default ' ',
  "AAAAJYGW" varchar(32) NOT NULL default ' ',
  "AAAAJYGX" varchar(6) NOT NULL default '000000',
  "AAAAJYIO" varchar(32) NOT NULL default ' ',
  "AAAAJYGZ" varchar(8) NOT NULL default '00000000',
  "AAAAJYHA" varchar(6) NOT NULL default '000000',
  "AAAAJYHB" varchar(12) NOT NULL default ' ',
  "AAAAJYHC" varchar(1) NOT NULL default ' ',
  "AAAAJYHD" varchar(10) NOT NULL default ' ',
  "AAAAJYHE" varchar(3) NOT NULL default '000',
  "AAAAJYHF" varchar(6) NOT NULL default '000000',
  "AAAAJYJH" varchar(32) NOT NULL default ' ',
  "AAAAJYJR" varchar(32) NOT NULL default ' ',
  "AAAAJYHI" varchar(32) NOT NULL default ' ',
  "AAAAJYLB" varchar(32) NOT NULL default ' ',
  "AAAAJYHK" varchar(4) NOT NULL default ' ',
  "AAAAJYHL" varchar(60) NOT NULL default ' ',
  "AAAAJYHM" varchar(12) NOT NULL default ' ',
  "AAAAJYHN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  KEY "AAAAJYHQ" ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABEXFH","AAABMQDO","AAAAJZDE","AAAAJYXP","AAAAJYXS","AAAAJZED","AAABEVTQ","AAAAJZDC","AAABMQHP","AAABMQHY","AAABMPVB"),
  KEY "AAAAJYIG" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAABEXQH","AAAAJYXT","AAAAJYIO"),
  KEY "AAAAJYIP" ("AAABMQHU","AAABMQCD","AAABMQCE","AAABMQCV","AAABMQCJ"),
  KEY "AAAAJYIV" ("AAABMQHU","AAABMQDO","AAABGUBO","AAAAJYXT","AAAAJYXP","AAAAJYXS","AAABEXFH","AAAAJYJH","AAAAJZEB"),
  KEY "AAAAJYJF" ("AAABMQHU","AAAAJYJH","AAABEXFH"),
  KEY "AAAAJYJJ" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAABEXQH","AAAAJYXT","AAAAJYJR"),
  KEY "AAAAJYJS" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAABMQCJ","AAABMQCO","AAABMQHY","AAABMQCL"),
  KEY "AAAAJYKC" ("AAABMQHU","AAABMQHV","AAABMQDO","AAAAJZEB","AAABMQCV","AAABMQDA","AAABMQCX","AAABMQCW","AAABEXFH"),
  KEY "AAAAJYKM" ("AAABMQHU","AAABMQHV","AAABMQDO","AAAAJZEB","AAAAJYKR","AAAAJYKS","AAABMQDA","AAABEXFH"),
  KEY "AAAAJYKV" ("AAABMQHU","AAABMQHW","AAABMQHX","AAABEXFH","AAABMQDO","AAAAJYLB","AAAAJZEB","AAABMQHV"),
  KEY "AAAAJYLE" ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAAAJZEB","AAABMQDO","AAABEXFH","AAABEXQH","AAAAJZDE","AAABEVTQ","AAAAJZDC","AAAAJZDD","AAAAJYYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGYLO" varchar(24) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAJYYH" varchar(24) NOT NULL default ' ',
  "AAAAJYYI" varchar(10) NOT NULL default ' ',
  "AAAAJYYJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAAAJYMM" ("AAABMRIL","AAABGYLO","AAABGUBO","AAABEZXF","AAABEXFE","AAABEVYG","AAABEZWP","AAABEVXF","AAABEZWC","AAABEZWD","AAABEZWE","AAABEZWF","AAABMQDU","AAAAJYYH","AAAAJYYI","AAAAJYYJ"),
  KEY "AAAAJYND" ("AAABMRIL","AAABEXFE","AAABEVYG","AAABEZWP","AAABEVXF","AAAAJYYI","AAAAJYYJ"),
  KEY "AAAAJYNL" ("AAABMRIL","AAABEZWC","AAABEZWD","AAABEZWE","AAABEZWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABEZBF" varchar(1) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAABGZDN" varchar(10) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABKJFL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAAAJYOP" ("AAABMRIL","AAABMREX","AAABHSAY","AAABGPZS","AAABJASU","AAABHSBA","AAABEZBF","AAABEZVC","AAABEZVD","AAABLCPX","AAABEYWM","AAABKWVJ","AAABGXQH","AAABGZDN","AAABGYJY","AAABKJFL"),
  KEY "AAAAJYPG" ("AAABMRIL","AAABMREX","AAABGXQH","AAABJASU","AAABHSBA","AAABEZVC","AAABEZVD"),
  KEY "AAAAJYPO" ("AAABMRIL","AAABMREX","AAABGXQH","AAABJASU","AAABEZBF","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYPV" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABEXFH" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAJZDE" varchar(2) NOT NULL default ' ',
  "AAABEVTQ" varchar(1) NOT NULL default ' ',
  "AAAAJYXL" varchar(1) NOT NULL default ' ',
  "AAAAJZDC" varchar(5) NOT NULL default ' ',
  "AAAAJZDD" varchar(2) NOT NULL default ' ',
  "AAABEXQH" varchar(1) NOT NULL default ' ',
  "AAAAJYXP" varchar(13) NOT NULL default ' ',
  "AAAAJZED" varchar(10) NOT NULL default ' ',
  "AAAAJZEB" varchar(3) NOT NULL default ' ',
  "AAAAJYXS" varchar(10) NOT NULL default ' ',
  "AAAAJYXT" varchar(8) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABGYLO" varchar(24) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAJYYH" varchar(24) NOT NULL default ' ',
  "AAAAJYYI" varchar(10) NOT NULL default ' ',
  "AAAAJYYJ" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAAAJYYL" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAAAJYYN" varchar(2) NOT NULL default ' ',
  "AAAAJYYO" varchar(1) NOT NULL default ' ',
  "AAAAJYYP" varchar(1) NOT NULL default ' ',
  "AAABLBYI" varchar(5) NOT NULL default ' ',
  "AAAAJYYR" varchar(2) NOT NULL default ' ',
  "AAAAJYYS" varchar(3) NOT NULL default ' ',
  "AAABMQDX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQED" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQER" decimal(17,2) NOT NULL default '0.00',
  "AAABMQES" decimal(17,2) NOT NULL default '0.00',
  "AAABMQET" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGN" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(6) NOT NULL default '000000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAJYVV" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAJYWE" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAAAJYWJ" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAJYWR" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJYWT" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABEXFH" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAJZDE" varchar(2) NOT NULL default ' ',
  "AAABEVTQ" varchar(1) NOT NULL default ' ',
  "AAAAJYXL" varchar(1) NOT NULL default ' ',
  "AAAAJZDC" varchar(5) NOT NULL default ' ',
  "AAAAJZDD" varchar(2) NOT NULL default ' ',
  "AAABEXQH" varchar(1) NOT NULL default ' ',
  "AAAAJYXP" varchar(13) NOT NULL default ' ',
  "AAAAJZED" varchar(10) NOT NULL default ' ',
  "AAAAJZEB" varchar(3) NOT NULL default ' ',
  "AAAAJYXS" varchar(10) NOT NULL default ' ',
  "AAAAJYXT" varchar(8) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABGYLO" varchar(24) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAJYYH" varchar(24) NOT NULL default ' ',
  "AAAAJYYI" varchar(10) NOT NULL default ' ',
  "AAAAJYYJ" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAAAJYYL" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAAAJYYN" varchar(2) NOT NULL default ' ',
  "AAAAJYYO" varchar(1) NOT NULL default ' ',
  "AAAAJYYP" varchar(1) NOT NULL default ' ',
  "AAABLBYI" varchar(5) NOT NULL default ' ',
  "AAAAJYYR" varchar(2) NOT NULL default ' ',
  "AAAAJYYS" varchar(3) NOT NULL default ' ',
  "AAABMQDX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQED" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQER" decimal(17,2) NOT NULL default '0.00',
  "AAABMQES" decimal(17,2) NOT NULL default '0.00',
  "AAABMQET" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(17,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(17,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(17,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGN" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(17,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(17,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS"),
  KEY "AAAAJZCP" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAABMQHZ","AAAAJZEB"),
  KEY "AAAAJZCW" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAAAJZDC","AAAAJZDD","AAAAJZDE","AAABEVTQ","AAABMQHP","AAABMQHY","AAABMQHS"),
  KEY "AAAAJZDJ" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAABEVTQ","AAABEZWC","AAABEZWD","AAABEZWE","AAABEZWF","AAABMQHS","AAABMQHY"),
  KEY "AAAAJZDW" ("AAABMQHU","AAABMQHV","AAABEXFH","AAABMQDO","AAAAJZEB","AAABEVTQ","AAAAJZED","AAABMQHY","AAABMQHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJZGW" varchar(32) NOT NULL default ' ',
  "AAAAJZHF" varchar(32) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAAAJZEN" varchar(10) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABFHBJ" varchar(1) NOT NULL default ' ',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABGYLO" varchar(24) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAAAJZEZ" varchar(5) NOT NULL default ' ',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAAAJZFB" varchar(5) NOT NULL default ' ',
  "AAAAJZFC" decimal(17,2) NOT NULL default '0.00',
  "AAAAJZFD" varchar(5) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  "AAABEXBH" varchar(15) NOT NULL default ' ',
  "AAAAJZFI" varchar(4) NOT NULL default '0000',
  "AAAAJZJH" varchar(32) NOT NULL default ' ',
  "AAAAJZFK" varchar(6) NOT NULL default '000000',
  "AAAAJZFL" varchar(32) NOT NULL default ' ',
  "AAABEXCU" varchar(3) NOT NULL default ' ',
  "AAAAJZFN" varchar(8) NOT NULL default ' ',
  "AAAAJZFO" varchar(1) NOT NULL default ' ',
  "AAABEXCX" varchar(3) NOT NULL default ' ',
  "AAAAJZFQ" varchar(8) NOT NULL default ' ',
  "AAABEYTA" varchar(32) NOT NULL default ' ',
  "AAAAJZFS" varchar(22) NOT NULL default ' ',
  "AAAAJZFT" varchar(8) NOT NULL default '00000000',
  "AAAAJZFU" varchar(6) NOT NULL default '000000',
  "AAAAJZFV" varchar(12) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAAAJZGA" varchar(10) NOT NULL default '0000000000',
  "AAAAKQZS" varchar(10) NOT NULL default ' ',
  "AAAAKQZR" varchar(10) NOT NULL default ' ',
  "AAABMCNN" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABJWEQ" varchar(10) NOT NULL default ' ',
  "AAABJWER" varchar(3) NOT NULL default '000',
  "AAAAJZGK" varchar(6) NOT NULL default '000000',
  "AAAAKAAM" varchar(4) NOT NULL default ' ',
  "AAAAKAAN" varchar(60) NOT NULL default ' ',
  "AAABJHFY" varchar(12) NOT NULL default ' ',
  "AAAAQKRS" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAAAJZYW" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJZGW"),
  KEY "AAAAJZGX" ("AAABMRIL","AAABMREX","AAABMPLS","AAABJWEQ","AAABJWER"),
  KEY "AAAAJZHD" ("AAABMRIL","AAAAJZHF"),
  KEY "AAAAJZHG" ("AAABMRIL","AAABMREX","AAABGXQH","AAABFHBJ","AAABGYJY","AAABEYWM","AAABJGWE","AAABEXQF","AAABMQBT"),
  KEY "AAAAJZHQ" ("AAABEXCI","AAABMREX","AAABFHBJ","AAABGXQH","AAABLCPX","AAABGUBO","AAABGYJY","AAABEYWM","AAABJGWE","AAABJWEQ"),
  KEY "AAAAJZIB" ("AAABEXCI","AAABMREX","AAABFHBJ","AAABGXQH","AAABLCPX","AAABGUBO","AAABGYJY","AAABEYWM","AAABJGWE","AAABMQCE","AAABKJVG"),
  KEY "AAAAJZIN" ("AAABMREX","AAABFHBJ","AAABGXQH","AAABLCPX","AAABGUBO","AAABGYJY","AAABEYWM","AAABJGWE","AAAAJZJH"),
  KEY "AAAAJZIX" ("AAABMRIL","AAABMQCD","AAABMQCE","AAABKJVG","AAABMCOP"),
  KEY "AAAAJZJD" ("AAABMRIL","AAABMREX","AAABEXCU","AAAAJZJH","AAABEXQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZWC","AAABEZWD","AAABEZWE","AAABEZWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZWC","AAABEZWD","AAABEZWE","AAABEZWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABLOQJ" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABIEDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABLOQJ","AAABMCMG","AAABEYFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABLOQJ" varchar(3) NOT NULL default '000',
  "AAAAJZLI" varchar(3) NOT NULL default '000',
  "AAAAJZLD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABLOQJ","AAAAJZLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAAAJZLW" varchar(30) NOT NULL default ' ',
  "AAABMNDR" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAAAWUCM" varchar(30) NOT NULL default ' ',
  "AAAAJZMA" varchar(30) NOT NULL default ' ',
  "AAAAJZMB" varchar(30) NOT NULL default ' ',
  "AAAAJZMC" varchar(30) NOT NULL default ' ',
  "AAABLOQJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABLOQJ" varchar(3) NOT NULL default '000',
  "AAAAJZMS" varchar(3) NOT NULL default '000',
  "AAAAJZMN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABLOQJ","AAAAJZMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAAAJZNE" varchar(3) NOT NULL default '000',
  "AAAAJZNF" varchar(3) NOT NULL default '000',
  "AAABLOQJ" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAAAJZNE","AAAAJZNF","AAABLOQJ","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAAAKABS" varchar(13) NOT NULL default ' ',
  "AAAAKABQ" varchar(32) NOT NULL default ' ',
  "AAAAKADQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEXGD","AAAAKABS","AAAAKABQ"),
  KEY "AAAAJZNS" ("AAABEXGD","AAAAKABQ","AAAAKABS","AAAAKADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAAAKABS" varchar(13) NOT NULL default ' ',
  "AAAAKABQ" varchar(32) NOT NULL default ' ',
  "AAAAKADQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEXGD","AAAAKABS"),
  KEY "AAAAJZOG" ("AAABEXGD","AAAAKABQ","AAAAKABS","AAAAKADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAAAKABS" varchar(13) NOT NULL default ' ',
  "AAAAKABQ" varchar(32) NOT NULL default ' ',
  "AAAAKADQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEXGD","AAAAKABS","AAAAKABQ"),
  KEY "AAAAJZOV" ("AAABEXGD","AAAAKABQ","AAAAKABS","AAAAKADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVMC","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZPI" (
  "AAABMMYV" varchar(15) NOT NULL default ' ',
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAAAJZTP" varchar(30) NOT NULL default ' ',
  "AAAAJZPP" varchar(30) NOT NULL default ' ',
  "AAAAJZSG" varchar(4) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMMYV","AAABERMO","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXWT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXWT" varchar(15) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  "AAAAJZQS" varchar(30) NOT NULL default ' ',
  "AAAAJZQT" varchar(10) NOT NULL default ' ',
  "AAAAJZQU" varchar(2) NOT NULL default ' ',
  "AAAAJZQV" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  "AAAAJZRC" varchar(1) NOT NULL default ' ',
  "AAAAJZRD" varchar(1) NOT NULL default ' ',
  "AAAAJZRE" varchar(1) NOT NULL default ' ',
  "AAAAJZRF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABIKIG" varchar(30) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABEZTU","AAABERMO","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYV" varchar(15) NOT NULL default ' ',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAAAJZTN" varchar(6) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAAAJZSG" varchar(30) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMMYV","AAABEZTU","AAABERMO","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJZTN" varchar(6) NOT NULL default ' ',
  "AAAAJZTB" varchar(6) NOT NULL default ' ',
  "AAAAJZTC" varchar(3) NOT NULL default ' ',
  "AAAAJZTO" varchar(30) NOT NULL default ' ',
  "AAAAJZTP" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  "AAABHIEG" varchar(32) NOT NULL default ' ',
  "AAAAJZSX" varchar(6) NOT NULL default ' ',
  "AAAAJZTL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJZTN","AAAAJZTB","AAAAJZTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJZTN" varchar(6) NOT NULL default ' ',
  "AAAAJZTO" varchar(30) NOT NULL default ' ',
  "AAAAJZTP" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(8) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAAAJZTL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJZTN","AAAAJZTO","AAAAJZTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQYU" varchar(12) NOT NULL default ' ',
  "AAAAJZUT" varchar(5) NOT NULL default '00000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAJZXH" varchar(1) NOT NULL default ' ',
  "AAAAJZXI" varchar(1) NOT NULL default ' ',
  "AAAAJZXJ" int(11) NOT NULL default '0',
  "AAAAJZXK" int(11) NOT NULL default '0',
  "AAAAJZXL" int(11) NOT NULL default '0',
  "AAAAJZXM" int(11) NOT NULL default '0',
  "AAAAJZXN" varchar(1) NOT NULL default ' ',
  "AAAAXDCK" varchar(1) NOT NULL default ' ',
  "AAAAJZXP" varchar(1) NOT NULL default ' ',
  "AAAAJZXQ" varchar(1) NOT NULL default ' ',
  "AAAAJZXR" varchar(50) NOT NULL default ' ',
  "AAAAJZXS" int(11) NOT NULL default '0',
  "AAAAJZXT" varchar(50) NOT NULL default ' ',
  "AAAAJZXU" int(11) NOT NULL default '0',
  "AAAAJZXV" varchar(1) NOT NULL default ' ',
  "AAAAJZXW" varchar(1) NOT NULL default ' ',
  "AAAAJZXX" int(11) NOT NULL default '0',
  "AAAAJZXY" varchar(1) NOT NULL default ' ',
  "AAAAJZXZ" varchar(1) NOT NULL default ' ',
  "AAAAJZYA" varchar(1) NOT NULL default ' ',
  "AAAAJZYB" varchar(1) NOT NULL default ' ',
  "AAAAJZVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQYU","AAAAJZUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJZYE" varchar(5) NOT NULL default '00000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAJZXH" varchar(1) NOT NULL default ' ',
  "AAAAJZXI" varchar(1) NOT NULL default ' ',
  "AAAAJZXJ" int(11) NOT NULL default '0',
  "AAAAJZXK" int(11) NOT NULL default '0',
  "AAAAJZXL" int(11) NOT NULL default '0',
  "AAAAJZXM" int(11) NOT NULL default '0',
  "AAAAJZXN" varchar(1) NOT NULL default ' ',
  "AAAAXDCK" varchar(1) NOT NULL default ' ',
  "AAAAJZXP" varchar(1) NOT NULL default ' ',
  "AAAAJZXQ" varchar(1) NOT NULL default ' ',
  "AAAAJZXR" varchar(50) NOT NULL default ' ',
  "AAAAJZXS" int(11) NOT NULL default '0',
  "AAAAJZXT" varchar(50) NOT NULL default ' ',
  "AAAAJZXU" int(11) NOT NULL default '0',
  "AAAAJZXV" varchar(1) NOT NULL default ' ',
  "AAAAJZXW" varchar(1) NOT NULL default ' ',
  "AAAAJZXX" int(11) NOT NULL default '0',
  "AAAAJZXY" varchar(1) NOT NULL default ' ',
  "AAAAJZXZ" varchar(1) NOT NULL default ' ',
  "AAAAJZYA" varchar(1) NOT NULL default ' ',
  "AAAAJZYB" varchar(1) NOT NULL default ' ',
  "AAAAJZVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJZYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAAAJZWI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEZWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEXUE" varchar(100) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABEYQI" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEXUA" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEXUE","AAABEYFR","AAABMCMG","AAABEYQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAJZYD" varchar(12) NOT NULL default ' ',
  "AAAAJZYE" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAJZXH" varchar(1) NOT NULL default ' ',
  "AAAAJZXI" varchar(1) NOT NULL default ' ',
  "AAAAJZXJ" int(11) NOT NULL default '0',
  "AAAAJZXK" int(11) NOT NULL default '0',
  "AAAAJZXL" int(11) NOT NULL default '0',
  "AAAAJZXM" int(11) NOT NULL default '0',
  "AAAAJZXN" varchar(1) NOT NULL default ' ',
  "AAAAXDCK" varchar(1) NOT NULL default ' ',
  "AAAAJZXP" varchar(1) NOT NULL default ' ',
  "AAAAJZXQ" varchar(1) NOT NULL default ' ',
  "AAAAJZXR" varchar(50) NOT NULL default ' ',
  "AAAAJZXS" int(11) NOT NULL default '0',
  "AAAAJZXT" varchar(50) NOT NULL default ' ',
  "AAAAJZXU" int(11) NOT NULL default '0',
  "AAAAJZXV" varchar(1) NOT NULL default ' ',
  "AAAAJZXW" varchar(1) NOT NULL default ' ',
  "AAAAJZXX" int(11) NOT NULL default '0',
  "AAAAJZXY" varchar(1) NOT NULL default ' ',
  "AAAAJZXZ" varchar(1) NOT NULL default ' ',
  "AAAAJZYA" varchar(1) NOT NULL default ' ',
  "AAAAJZYB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAJZYD","AAAAJZYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAJZYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKAAY" varchar(32) NOT NULL default ' ',
  "AAAAKABQ" varchar(32) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGZDN" varchar(10) NOT NULL default ' ',
  "AAAAJZYW" varchar(10) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAJZYY" varchar(3) NOT NULL default '000',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABEZBF" varchar(1) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABEVZI" varchar(10) NOT NULL default ' ',
  "AAABEWAK" varchar(10) NOT NULL default ' ',
  "AAABEZXA" varchar(1) NOT NULL default ' ',
  "AAABEZXB" varchar(2) NOT NULL default ' ',
  "AAABEZWN" varchar(2) NOT NULL default ' ',
  "AAABEWEX" varchar(1) NOT NULL default ' ',
  "AAABEWFY" varchar(1) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  "AAAAJZZU" decimal(17,2) NOT NULL default '0.00',
  "AAAAJZZV" varchar(5) NOT NULL default ' ',
  "AAAAJZZW" decimal(17,2) NOT NULL default '0.00',
  "AAAAJZZX" varchar(5) NOT NULL default ' ',
  "AAAAJZZY" varchar(5) NOT NULL default ' ',
  "AAAAJZZZ" varchar(32) NOT NULL default ' ',
  "AAABBNPS" varchar(8) NOT NULL default '00000000',
  "AAAAKAAB" varchar(8) NOT NULL default '00000000',
  "AAAAKAAC" varchar(32) NOT NULL default ' ',
  "AAAAKAAD" varchar(3) NOT NULL default ' ',
  "AAAAKAAE" varchar(22) NOT NULL default ' ',
  "AAAAKAAF" varchar(1) NOT NULL default ' ',
  "AAAAKABS" varchar(22) NOT NULL default ' ',
  "AAAAKAAH" varchar(30) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLBXH" varchar(34) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKAAL" varchar(5) NOT NULL default '00000',
  "AAAAKAAM" varchar(4) NOT NULL default ' ',
  "AAAAKAAN" varchar(60) NOT NULL default ' ',
  "AAABJHFY" varchar(12) NOT NULL default ' ',
  "AAAAQKRS" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAAAKAAR" varchar(3) NOT NULL default ' ',
  "AAABEVJX" varchar(1) NOT NULL default ' ',
  "AAAAKADQ" varchar(1) NOT NULL default '0',
  "AAAAKAAU" varchar(1) NOT NULL default ' ',
  "AAABEXEX" varchar(1) NOT NULL default ' ',
  "AAABEVSW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKAAY","AAAAKABQ","AAABHSAY"),
  KEY "AAAAKABB" ("AAABMRIL","AAABMREX","AAABGYJY","AAABGVMC","AAABEYWM","AAABLCPX"),
  KEY "AAAAKABI" ("AAABMRIL","AAABHSAY","AAABMREX","AAABLCPX"),
  KEY "AAAAKABN" ("AAABMRIL","AAABHSAY","AAAAKABQ","AAABMREX","AAAAKABS"),
  KEY "AAAAKABT" ("AAABMRIL","AAABHSAY","AAABMREX","AAABGYJY","AAABGVMC","AAABEYWM","AAABLCPX","AAABGZDN"),
  KEY "AAAAKACC" ("AAABMRIL","AAABHSAY","AAABMREX","AAABEZVC","AAABEZVD","AAABLCPX","AAABMQBT"),
  KEY "AAAAKACK" ("AAABMRIL","AAABEYWM","AAABLCPX","AAABMREX","AAABHSAY","AAABGZDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKACR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAXAKY" varchar(4) NOT NULL default '0000',
  "AAAAXAKZ" varchar(2) NOT NULL default '00',
  "AAAAWIST" varchar(2) NOT NULL default '00',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAAAKACY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXQ","AAAAXAKY","AAAAXAKZ","AAAAWIST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKADE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  "AAAAKADH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKADK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKADP" varchar(1) NOT NULL default '0',
  "AAAAKADQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKADP","AAAAKADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKADR" (
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  "AAAAKAFJ" varchar(24) NOT NULL default ' ',
  "AAABLNPY" varchar(6) NOT NULL default ' ',
  "AAAAKADV" varchar(5) NOT NULL default ' ',
  "AAAAKADW" varchar(1) NOT NULL default ' ',
  "AAAAKADX" varchar(20) NOT NULL default ' ',
  "AAAAKADY" varchar(12) NOT NULL default ' ',
  "AAAAKADZ" varchar(60) NOT NULL default ' ',
  "AAAAKAEA" varchar(12) NOT NULL default ' ',
  "AAAAKAEB" decimal(15,0) NOT NULL default '0',
  "AAAAKAEC" varchar(12) NOT NULL default ' ',
  "AAAAKAED" decimal(15,0) NOT NULL default '0',
  "AAAAKAEE" varchar(12) NOT NULL default ' ',
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAAAKAEG" varchar(4) NOT NULL default ' ',
  "AAAAKAEH" varchar(30) NOT NULL default ' ',
  "AAAAKAEI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGL","AAAAKAFJ","AAABLNPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAEM" (
  "AAAAUYET" varchar(32) NOT NULL default ' ',
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  "AAAAKAFJ" varchar(24) NOT NULL default ' ',
  "AAAAKAFC" varchar(1) NOT NULL default ' ',
  "AAABHDZV" varchar(32) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABHGZO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYET","AAABHEGL","AAAAKAFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAEX" (
  "AAAAUYET" varchar(32) NOT NULL default ' ',
  "AAABHEGL" varchar(32) NOT NULL default ' ',
  "AAABLNPY" varchar(1) NOT NULL default ' ',
  "AAAAKAFJ" varchar(24) NOT NULL default ' ',
  "AAAAKAFC" varchar(1) NOT NULL default ' ',
  "AAABHDZV" varchar(32) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABHGZO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUYET","AAABHEGL","AAABLNPY","AAAAKAFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDVYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQWL" varchar(12) NOT NULL default '000000000000',
  "AAABKQOJ" varchar(8) NOT NULL default '00000000',
  "AAABKQOK" varchar(4) NOT NULL default '0000',
  "AAABKQQF" varchar(8) NOT NULL default '00000000',
  "AAABKQQE" varchar(10) NOT NULL default '0000000000',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAAAKAFT" double NOT NULL default '0',
  "AAAAKAFU" double NOT NULL default '0',
  "AAAAKAFV" double NOT NULL default '0',
  "AAABKQFT" varchar(3) NOT NULL default ' ',
  "AAAAKAFX" varchar(8) NOT NULL default '00000000',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  "AAABKQHC" varchar(4) NOT NULL default '0000',
  "AAABKQHD" tinyint(3) unsigned NOT NULL default '0',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKQMB" varchar(1) NOT NULL default ' ',
  "AAAAKAGD" varchar(10) NOT NULL default '0000000000',
  "AAABKQKJ" varchar(8) NOT NULL default '00000000',
  "AAABKQKK" varchar(6) NOT NULL default '000000',
  "AAABKQKT" varchar(8) NOT NULL default '00000000',
  "AAABKQKU" varchar(6) NOT NULL default '000000',
  "AAAAKAGI" varchar(1) NOT NULL default ' ',
  "AAAAKAGJ" varchar(1) NOT NULL default ' ',
  "AAABKQGJ" varchar(8) NOT NULL default ' ',
  "AAAAKAGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAKAGM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQWL","AAABKQOJ","AAABKQOK","AAABKQQF"),
  KEY "AAAAKAGS" ("AAABMRIL","AAABKQFL","AAABKQKJ"),
  KEY "AAAAKAGW" ("AAABMRIL","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAGZ" (
  "AAABCAKV" varchar(30) NOT NULL default ' ',
  "AAABLEAH" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCAKV","AAABLEAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAHF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAKAHO" varchar(12) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAKAHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKAHR" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAKAHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAHV" (
  "AAAAKAIB" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAAAKAHZ" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKAIB","AAABMDEP","AAABHFQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAIE" (
  "AAAAUZVO" varchar(40) NOT NULL default ' ',
  "AAAAKAIG" varchar(10) NOT NULL default ' ',
  "AAAAKAIH" varchar(10) NOT NULL default ' ',
  "AAAAKAII" varchar(10) NOT NULL default ' ',
  "AAAAKAIJ" varchar(10) NOT NULL default ' ',
  "AAABELXV" varchar(10) NOT NULL default ' ',
  "AAAAKAIL" varchar(20) NOT NULL default ' ',
  "AAAAKAIM" varchar(20) NOT NULL default ' ',
  "AAAAKAIN" varchar(10) NOT NULL default ' ',
  "AAAAKAIO" varchar(20) NOT NULL default ' ',
  "AAAAKAIP" varchar(20) NOT NULL default ' ',
  "AAAAKAIQ" varchar(20) NOT NULL default ' ',
  "AAAAKAIR" varchar(20) NOT NULL default ' ',
  "AAAAKAIS" varchar(20) NOT NULL default ' ',
  "AAAAKAIT" varchar(40) NOT NULL default ' ',
  "AAAAKAIU" varchar(40) NOT NULL default ' ',
  "AAAAKAIV" varchar(40) NOT NULL default ' ',
  "AAAAKAIW" varchar(40) NOT NULL default ' ',
  "AAAAKAIX" varchar(40) NOT NULL default ' ',
  "AAAARUVM" varchar(20) NOT NULL default ' ',
  "AAAAKAIZ" varchar(20) NOT NULL default ' ',
  "AAAAKAJA" varchar(20) NOT NULL default ' ',
  "AAAAKAJB" varchar(20) NOT NULL default ' ',
  "AAAAKAJC" varchar(20) NOT NULL default ' ',
  "AAAAKAJD" varchar(20) NOT NULL default ' ',
  "AAAAKAJE" varchar(20) NOT NULL default ' ',
  "AAAAKAJF" varchar(20) NOT NULL default ' ',
  "AAAAKAJG" varchar(20) NOT NULL default ' ',
  "AAAAKAJH" varchar(20) NOT NULL default ' ',
  "AAAAKAJI" varchar(20) NOT NULL default ' ',
  "AAAAKAJJ" varchar(20) NOT NULL default ' ',
  "AAAAKAJK" varchar(20) NOT NULL default ' ',
  "AAAAKAJL" varchar(20) NOT NULL default ' ',
  "AAAAKAJM" varchar(20) NOT NULL default ' ',
  "AAAAKAJN" varchar(20) NOT NULL default ' ',
  "AAAAKAJO" varchar(20) NOT NULL default ' ',
  "AAAAKAJP" varchar(20) NOT NULL default ' ',
  "AAAAKAJQ" varchar(20) NOT NULL default ' ',
  "AAAAKAJR" varchar(20) NOT NULL default ' ',
  "AAAAKAJS" varchar(20) NOT NULL default ' ',
  "AAAAKAJT" varchar(20) NOT NULL default ' ',
  "AAABLTQJ" varchar(25) NOT NULL default ' ',
  "AAAAKAJV" varchar(25) NOT NULL default ' ',
  "AAAAKAJW" varchar(25) NOT NULL default ' ',
  "AAAAKAJX" varchar(25) NOT NULL default ' ',
  "AAAAKAJY" varchar(25) NOT NULL default ' ',
  "AAAAKAJZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAKB" (
  "AAAAKAKF" varchar(64) NOT NULL default ' ',
  "AAAAKAKD" varchar(2) NOT NULL default '00',
  "AAAAKAKE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAKAKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAKG" (
  "AAAAVFFM" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVFFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAKJ" (
  "AAAAKAKL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKAKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAKM" (
  "AAAAUZVO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAKP" (
  "AAABLTUD" varchar(1) NOT NULL default ' ',
  "AAAAKAKR" varchar(1) NOT NULL default ' ',
  "AAAAKAKS" varchar(1) NOT NULL default ' ',
  "AAAAKAKT" varchar(1) NOT NULL default ' ',
  "AAAAKAKU" varchar(1) NOT NULL default ' ',
  "AAAAKAKV" varchar(3) NOT NULL default ' ',
  "AAAAKAKW" varchar(3) NOT NULL default ' ',
  "AAABIIZZ" varchar(1) NOT NULL default ' ',
  "AAAAKAKY" varchar(1) NOT NULL default ' ',
  "AAAAKAKZ" varchar(1) NOT NULL default ' ',
  "AAAAKALA" varchar(1) NOT NULL default ' ',
  "AAAAUUTB" varchar(1) NOT NULL default ' ',
  "AAAAKALC" varchar(25) NOT NULL default ' ',
  "AAAAKALD" varchar(3) NOT NULL default ' ',
  "AAAAKALE" varchar(3) NOT NULL default ' ',
  "AAAAKALF" varchar(25) NOT NULL default ' ',
  "AAAAKALG" varchar(1) NOT NULL default ' ',
  "AAAAKALH" varchar(1) NOT NULL default ' ',
  "AAAAKALI" varchar(1) NOT NULL default ' ',
  "AAAAKALJ" varchar(1) NOT NULL default ' ',
  "AAAAKALK" varchar(1) NOT NULL default ' ',
  "AAAAKALL" varchar(1) NOT NULL default ' ',
  "AAAAKALM" varchar(1) NOT NULL default ' ',
  "AAAAKALN" varchar(1) NOT NULL default ' ',
  "AAAAKALO" varchar(40) NOT NULL default ' ',
  "AAAAKALP" varchar(40) NOT NULL default ' ',
  "AAAAKALQ" varchar(40) NOT NULL default ' ',
  "AAAAKALR" varchar(25) NOT NULL default ' ',
  "AAAAKALS" varchar(1) NOT NULL default ' ',
  "AAAAKALT" varchar(25) NOT NULL default ' ',
  "AAAAKALU" varchar(1) NOT NULL default ' ',
  "AAAAKALV" varchar(1) NOT NULL default ' ',
  "AAAAKALW" varchar(1) NOT NULL default ' ',
  "AAAAKALX" varchar(1) NOT NULL default ' ',
  "AAAAKALY" varchar(1) NOT NULL default ' ',
  "AAAAKALZ" varchar(1) NOT NULL default ' ',
  "AAAAKAMA" varchar(40) NOT NULL default ' ',
  "AAAAKAMB" varchar(1) NOT NULL default ' ',
  "AAAAKAMC" varchar(1) NOT NULL default ' ',
  "AAAAKAMD" varchar(25) NOT NULL default ' ',
  "AAAAKAME" varchar(25) NOT NULL default ' ',
  "AAAAKAMF" varchar(1) NOT NULL default ' ',
  "AAAAKAMG" varchar(1) NOT NULL default ' ',
  "AAAAKAMH" varchar(25) NOT NULL default ' ',
  "AAAAKAMI" varchar(40) NOT NULL default ' ',
  "AAAAKAMJ" varchar(40) NOT NULL default ' ',
  "AAAAKAMK" varchar(25) NOT NULL default ' ',
  "AAAAKAML" varchar(40) NOT NULL default ' ',
  "AAAAKAMM" varchar(1) NOT NULL default ' ',
  "AAAAKAMN" varchar(1) NOT NULL default ' ',
  "AAAAKAMO" varchar(1) NOT NULL default ' ',
  "AAAAKAMP" varchar(1) NOT NULL default ' ',
  "AAABMNYU" varchar(25) NOT NULL default ' ',
  "AAAAKAMR" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(25) NOT NULL default ' ',
  "AAAAKAMT" varchar(1) NOT NULL default ' ',
  "AAAAKAMU" varchar(1) NOT NULL default ' ',
  "AAAAKAMV" varchar(1) NOT NULL default ' ',
  "AAAAKAMW" varchar(1) NOT NULL default ' ',
  "AAAAKAMX" varchar(1) NOT NULL default ' ',
  "AAAAKAMY" varchar(40) NOT NULL default ' ',
  "AAABAMIH" varchar(2) NOT NULL default ' ',
  "AAABIIZY" varchar(2) NOT NULL default ' ',
  "AAABAMXD" varchar(1) NOT NULL default ' ',
  "AAAAKANC" varchar(1) NOT NULL default ' ',
  "AAAAKAND" varchar(1) NOT NULL default ' ',
  "AAAAKANE" varchar(1) NOT NULL default ' ',
  "AAAAKANF" varchar(1) NOT NULL default ' ',
  "AAAAKANG" varchar(1) NOT NULL default ' ',
  "AAAAKANH" varchar(1) NOT NULL default ' ',
  "AAABEESD" varchar(1) NOT NULL default ' ',
  "AAAAKANJ" varchar(1) NOT NULL default ' ',
  "AAAAKANK" varchar(1) NOT NULL default ' ',
  "AAAAKANL" varchar(1) NOT NULL default ' ',
  "AAAAKANM" varchar(25) NOT NULL default ' ',
  "AAAAKANN" varchar(25) NOT NULL default ' ',
  "AAAAKANO" varchar(25) NOT NULL default ' ',
  "AAAAKANP" varchar(25) NOT NULL default ' ',
  "AAAAKANQ" varchar(25) NOT NULL default ' ',
  "AAAAKANR" varchar(40) NOT NULL default ' ',
  "AAAALEEY" varchar(25) NOT NULL default ' ',
  "AAAAKANT" varchar(1) NOT NULL default ' ',
  "AAAAKANU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKANW" (
  "AAAAKANZ" varchar(20) NOT NULL default ' ',
  "AAAAKANY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKANZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKARB" varchar(10) NOT NULL default ' ',
  "AAABMQYE" varchar(1) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAKAOJ" varchar(1) NOT NULL default ' ',
  "AAAAKAOK" varchar(1) NOT NULL default ' ',
  "AAAAKAOL" varchar(10) NOT NULL default ' ',
  "AAAAKAOM" varchar(4) NOT NULL default '0000',
  "AAAAKAON" varchar(4) NOT NULL default ' ',
  "AAAAKAOO" varchar(3) NOT NULL default '000',
  "AAAAKAOP" varchar(3) NOT NULL default '000',
  "AAABMCNC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAKARB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKARB" varchar(10) NOT NULL default ' ',
  "AAAAKARC" varchar(3) NOT NULL default '000',
  "AAABKEWS" varchar(22) NOT NULL default ' ',
  "AAAAKAPA" varchar(22) NOT NULL default ' ',
  "AAABJVXT" varchar(22) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  "AAAAKAPD" varchar(1) NOT NULL default ' ',
  "AAABKJAJ" varchar(8) NOT NULL default '00000000',
  "AAABKJAK" varchar(8) NOT NULL default '00000000',
  "AAABKLXJ" varchar(8) NOT NULL default '00000000',
  "AAABKABQ" varchar(1) NOT NULL default ' ',
  "AAABKHOF" varchar(2) NOT NULL default '00',
  "AAABJVXX" decimal(12,3) NOT NULL default '0.000',
  "AAABJVXZ" decimal(14,3) NOT NULL default '0.000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKYYM" decimal(9,6) NOT NULL default '0.000000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABJWFE" varchar(8) NOT NULL default '00000000',
  "AAABJWFF" varchar(8) NOT NULL default '00000000',
  "AAABJVXY" decimal(5,2) NOT NULL default '0.00',
  "AAABJVYD" varchar(1) NOT NULL default ' ',
  "AAABKKRH" varchar(16) NOT NULL default ' ',
  "AAABKCGJ" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABJVYK" varchar(22) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABJVYP" decimal(5,2) NOT NULL default '0.00',
  "AAABKHKO" varchar(1) NOT NULL default ' ',
  "AAABJVYR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAKARB","AAAAKARC"),
  KEY "AAAAKAQI" ("AAABKKRH"),
  KEY "AAAAKAQK" ("AAABKEWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKARB" varchar(10) NOT NULL default ' ',
  "AAAAKARC" varchar(3) NOT NULL default '000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAKAQS" decimal(13,2) NOT NULL default '0.00',
  "AAAAKAQT" decimal(13,2) NOT NULL default '0.00',
  "AAAAKAQU" varchar(1) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAKAQX" varchar(3) NOT NULL default '000',
  "AAAAKAQY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAKARB","AAAAKARC","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKARE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHIKM" varchar(10) NOT NULL default ' ',
  "AAAAKART" varchar(6) NOT NULL default '000000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAKARK" decimal(15,3) NOT NULL default '0.000',
  "AAAAKWLS" decimal(13,3) NOT NULL default '0.000',
  "AAAAQABA" varchar(3) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAKARO" varchar(8) NOT NULL default '00000000',
  "AAAATQNA" varchar(6) NOT NULL default '000000',
  "AAAAKARQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABHIKM","AAAAKART")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKARU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAKASH" varchar(8) NOT NULL default '00000000',
  "AAAAKASE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAAKASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKASA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAKASG" varchar(20) NOT NULL default ' ',
  "AAAAKASH" varchar(8) NOT NULL default '00000000',
  "AAAAKASE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAAKASG","AAAAKASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKASI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKASN" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAAAKATS" varchar(1) NOT NULL default ' ',
  "AAAAKATT" varchar(10) NOT NULL default ' ',
  "AAAAKATU" varchar(12) NOT NULL default ' ',
  "AAAAKATV" varchar(3) NOT NULL default ' ',
  "AAABHECT" varchar(12) NOT NULL default ' ',
  "AAAAKEAC" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  "AAAAKASX" varchar(12) NOT NULL default ' ',
  "AAAAKASY" varchar(14) NOT NULL default '00000000000000',
  "AAAATMPW" varchar(14) NOT NULL default '00000000000000',
  "AAAAKATA" varchar(10) NOT NULL default '0000000000',
  "AAAAKATB" varchar(15) NOT NULL default ' ',
  "AAAAKATC" varchar(64) NOT NULL default ' ',
  "AAAAKTIC" varchar(30) NOT NULL default ' ',
  "AAAAKATE" varchar(8) NOT NULL default '00000000',
  "AAAAKATF" varchar(8) NOT NULL default '00000000',
  "AAAAKATG" varchar(8) NOT NULL default '00000000',
  "AAABIFVP" varchar(64) NOT NULL default ' ',
  "AAABLEBK" varchar(14) NOT NULL default '00000000000000',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBM" varchar(14) NOT NULL default '00000000000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAKATM" varchar(10) NOT NULL default '0000000000',
  "AAAAKATN" varchar(10) NOT NULL default '0000000000',
  "AAAAKATO" varchar(10) NOT NULL default '0000000000',
  "AAAAKATP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAAAKATS","AAAAKATT","AAAAKATU","AAAAKATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKATW" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAAAKATY" varchar(32) NOT NULL default ' ',
  "AAAAKATZ" varchar(32) NOT NULL default ' ',
  "AAAAKAUA" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAAAKAUC" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAUF" (
  "AAABGFRN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAUI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFRN" varchar(25) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGFRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAUO" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAUV" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAVG" (
  "AAAAQATQ" varchar(32) NOT NULL default ' ',
  "AAAAQATP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAQATQ","AAAAQATP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAVL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAVV" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKAWJ" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAWN" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKAXG" ("AAABLLLJ"),
  KEY "AAAAKAXI" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAXK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAXR" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKAYB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAYF" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKAYS" ("AAABLLNL"),
  KEY "AAAAKAYU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAYW" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKAZJ" ("AAABLLNL"),
  KEY "AAAAKAZL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKAZN" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABHZOI" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  "AAABHZOK" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHZOM" varchar(1) NOT NULL default ' ',
  "AAABHZON" varchar(4) NOT NULL default ' ',
  "AAABHZOO" varchar(4) NOT NULL default ' ',
  "AAABHZOP" varchar(8) NOT NULL default '00000000',
  "AAABHZOQ" varchar(8) NOT NULL default '00000000',
  "AAABHZOR" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHZOV" varchar(32) NOT NULL default ' ',
  "AAABHZOW" varchar(32) NOT NULL default ' ',
  "AAABHZOX" varchar(32) NOT NULL default ' ',
  "AAABHZOY" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZPD" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB"),
  KEY "AAAAKBBD" ("AAABJAAI"),
  KEY "AAAAKBBF" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAKBBI" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAKBBL" ("AAABHZOJ"),
  KEY "AAAAKBBN" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBBQ" (
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZPY" varchar(10) NOT NULL default '0000000000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZQD" varchar(1) NOT NULL default ' ',
  "AAABHZQE" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN"),
  KEY "AAAAKBCM" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBCP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAKBDD" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBDG" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBDS" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKBEG" ("AAABLLNL"),
  KEY "AAAAKBEI" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBEK" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBEX" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBFB" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKBGB" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBGD" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKBGR" ("AAABLLNL"),
  KEY "AAAAKBGT" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBGV" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBHK" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBHO" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBHY" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBIC" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKBIP" ("AAABLLNL"),
  KEY "AAAAKBIR" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBIT" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBJG" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBJK" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKBJX" ("AAABLLNL"),
  KEY "AAAAKBJZ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBKB" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBKO" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBKS" (
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNL"),
  KEY "AAAAKBLE" ("AAABLLNN"),
  KEY "AAAAKBLG" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBLI" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBLS" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBLW" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBMD" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBMO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBMY" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKBNM" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBNQ" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKBOJ" ("AAABLLLJ"),
  KEY "AAAAKBOL" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBON" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBOU" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBPE" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBPI" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKBPV" ("AAABLLNL"),
  KEY "AAAAKBPX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBPZ" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKBQM" ("AAABLLNL"),
  KEY "AAAAKBQO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBQQ" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABHZOI" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  "AAABHZOK" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHZOM" varchar(1) NOT NULL default ' ',
  "AAABHZON" varchar(4) NOT NULL default ' ',
  "AAABHZOO" varchar(4) NOT NULL default ' ',
  "AAABHZOP" varchar(8) NOT NULL default '00000000',
  "AAABHZOQ" varchar(8) NOT NULL default '00000000',
  "AAABHZOR" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHZOV" varchar(32) NOT NULL default ' ',
  "AAABHZOW" varchar(32) NOT NULL default ' ',
  "AAABHZOX" varchar(32) NOT NULL default ' ',
  "AAABHZOY" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZPD" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB"),
  KEY "AAAAKBSG" ("AAABJAAI"),
  KEY "AAAAKBSI" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAKBSL" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAKBSO" ("AAABHZOJ"),
  KEY "AAAAKBSQ" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBST" (
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZPY" varchar(10) NOT NULL default '0000000000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZQD" varchar(1) NOT NULL default ' ',
  "AAABHZQE" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN"),
  KEY "AAAAKBTP" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBTS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAKBUG" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBUJ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBVJ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBVV" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKBWJ" ("AAABLLNL"),
  KEY "AAAAKBWL" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBWN" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKBXN" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAKBXQ" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBXS" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKBYG" ("AAABLLNL"),
  KEY "AAAAKBYI" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBYK" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKBYU" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBYY" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKBZL" ("AAABLLNL"),
  KEY "AAAAKBZN" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKBZP" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKCAC" ("AAABLLNL"),
  KEY "AAAAKCAE" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCAG" (
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABMPMW" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCAP" (
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNL"),
  KEY "AAAAKCBB" ("AAABLLNN"),
  KEY "AAAAKCBD" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCBF" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCBP" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCBT" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCCA" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCCL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCCV" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKCDJ" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCDN" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKCEG" ("AAABLLLJ"),
  KEY "AAAAKCEI" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCEK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCER" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCFB" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCFF" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKCFS" ("AAABLLNL"),
  KEY "AAAAKCFU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCFW" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKCGJ" ("AAABLLNL"),
  KEY "AAAAKCGL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCGN" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABHZOI" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  "AAABHZOK" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHZOM" varchar(1) NOT NULL default ' ',
  "AAABHZON" varchar(4) NOT NULL default ' ',
  "AAABHZOO" varchar(4) NOT NULL default ' ',
  "AAABHZOP" varchar(8) NOT NULL default '00000000',
  "AAABHZOQ" varchar(8) NOT NULL default '00000000',
  "AAABHZOR" varchar(1) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  "AAABHZOV" varchar(32) NOT NULL default ' ',
  "AAABHZOW" varchar(32) NOT NULL default ' ',
  "AAABHZOX" varchar(32) NOT NULL default ' ',
  "AAABHZOY" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZPD" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB"),
  KEY "AAAAKCID" ("AAABJAAI"),
  KEY "AAAAKCIF" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAKCII" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAKCIL" ("AAABHZOJ"),
  KEY "AAAAKCIN" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCIQ" (
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZPY" varchar(10) NOT NULL default '0000000000',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABHZQA" varchar(20) NOT NULL default ' ',
  "AAABHZQB" varchar(10) NOT NULL default ' ',
  "AAABHZQC" varchar(10) NOT NULL default ' ',
  "AAABHZQD" varchar(1) NOT NULL default ' ',
  "AAABHZQE" varchar(1) NOT NULL default ' ',
  "AAABHZQF" varchar(20) NOT NULL default ' ',
  "AAABHZQG" varchar(20) NOT NULL default ' ',
  "AAABHZQH" varchar(1) NOT NULL default ' ',
  "AAABHZQI" varchar(1) NOT NULL default ' ',
  "AAABHZQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN"),
  KEY "AAAAKCJM" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCJP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAKCKD" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCKG" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKCLG" ("AAABLLLR"),
  KEY "AAAAKCLI" ("AAABHZJL","AAABLLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCLL" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCLX" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKCML" ("AAABLLNL"),
  KEY "AAAAKCMN" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCMP" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKCNP" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCNR" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKCOF" ("AAABLLNL"),
  KEY "AAAAKCOH" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCOJ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCOT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCOX" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKCPK" ("AAABLLNL"),
  KEY "AAAAKCPM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCPO" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKCQB" ("AAABLLNL"),
  KEY "AAAAKCQD" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCQF" (
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABMPMW" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCQO" (
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNL"),
  KEY "AAAAKCRA" ("AAABLLNN"),
  KEY "AAAAKCRC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCRE" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCRO" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCRS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCSC" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCSJ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCSU" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKCTI" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCTM" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKCUF" ("AAABLLLJ"),
  KEY "AAAAKCUH" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCUJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCUQ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCVA" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCVE" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKCVR" ("AAABLLNL"),
  KEY "AAAAKCVT" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCVV" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKCWI" ("AAABLLNL"),
  KEY "AAAAKCWK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCWM" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCWY" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKCXM" ("AAABLLNL"),
  KEY "AAAAKCXO" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCXQ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKCYQ" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAKCYT" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCYV" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKCZJ" ("AAABLLNL"),
  KEY "AAAAKCZL" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKCZN" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKCZX" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDAB" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDAO" ("AAABLLNL"),
  KEY "AAAAKDAQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDAS" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKDBF" ("AAABLLNL"),
  KEY "AAAAKDBH" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDBJ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDBQ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDCB" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKDCP" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDCT" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKDDM" ("AAABLLLJ"),
  KEY "AAAAKDDO" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDDQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDDX" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDEH" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDEL" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDEY" ("AAABLLNL"),
  KEY "AAAAKDFA" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDFC" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKDFP" ("AAABLLNL"),
  KEY "AAAAKDFR" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDFT" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDGF" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDGT" ("AAABLLNL"),
  KEY "AAAAKDGV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDGX" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDHK" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDHO" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKDIO" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAKDIR" ("AAABLLLR"),
  KEY "AAAAKDIT" ("AAABHZJL","AAABLLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDIW" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKDJK" ("AAABLLNL"),
  KEY "AAAAKDJM" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDJO" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDKD" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDKH" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDKR" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDKV" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDLI" ("AAABLLNL"),
  KEY "AAAAKDLK" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDLM" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDLZ" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDMD" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKDMQ" ("AAABLLNL"),
  KEY "AAAAKDMS" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDMU" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDNH" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDNL" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDNS" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDOD" (
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default '0000',
  "AAABHZDA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABHZDJ","AAABHZJH"),
  KEY "AAAAKDOR" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDOV" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLKV" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR"),
  KEY "AAAAKDPO" ("AAABLLLJ"),
  KEY "AAAAKDPQ" ("AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDPS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDPZ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDQJ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDQN" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDRA" ("AAABLLNL"),
  KEY "AAAAKDRC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDRE" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKDRR" ("AAABLLNL"),
  KEY "AAAAKDRT" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDRV" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDSH" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDSV" ("AAABLLNL"),
  KEY "AAAAKDSX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDSZ" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDTM" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDTQ" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABLLLB" varchar(15) NOT NULL default ' ',
  "AAABLLLC" varchar(15) NOT NULL default ' ',
  "AAABLLLD" varchar(15) NOT NULL default ' ',
  "AAABLLLE" varchar(20) NOT NULL default ' ',
  "AAABLLLF" varchar(20) NOT NULL default ' ',
  "AAABLLLG" varchar(20) NOT NULL default ' ',
  "AAABLLLH" varchar(40) NOT NULL default ' ',
  "AAABLLLI" varchar(40) NOT NULL default ' ',
  "AAABLLLJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH"),
  KEY "AAAAKDUQ" ("AAABLLNH","AAABHZJH"),
  KEY "AAAAKDUT" ("AAABLLLR"),
  KEY "AAAAKDUV" ("AAABHZJL","AAABLLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDUY" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAKDVM" ("AAABLLNL"),
  KEY "AAAAKDVO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDVQ" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABLLNN","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDWF" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDWJ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDWT" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDWX" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAKDXK" ("AAABLLNL"),
  KEY "AAAAKDXM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDXO" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDYB" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDYF" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAKDYS" ("AAABLLNL"),
  KEY "AAAAKDYU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDYW" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAKDZJ" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDZN" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABBIVC" varchar(70) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABBIVC","AAABMDEN","AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKDZY" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABBIVC" varchar(70) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAKEAC" varchar(1) NOT NULL default ' ',
  "AAAAKEAD" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAVCTN" varchar(12) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAAAKEAI" varchar(4) NOT NULL default ' ',
  "AAAAKEAJ" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABBIVC","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEAP" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABBIVC" varchar(70) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABBIVC","AAABMDEN","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEAZ" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABBIVC" varchar(70) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABBIVC","AAABMDEN","AAABMLPV","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEBK" (
  "AAAAKECE" varchar(12) NOT NULL default ' ',
  "AAAAKECY" varchar(70) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAAAKEBP" varchar(40) NOT NULL default ' ',
  "AAABGLCJ" decimal(15,0) NOT NULL default '0',
  "AAABLTLN" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAKEBT" varchar(10) NOT NULL default ' ',
  "AAAAKEBU" varchar(32) NOT NULL default ' ',
  "AAAAKEBV" varchar(10) NOT NULL default ' ',
  "AAAAKEBW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKECE","AAAAKECY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEBZ" (
  "AAAAKECE" varchar(12) NOT NULL default ' ',
  "AAAAKECY" varchar(70) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKECE","AAAAKECY","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKECH" (
  "AAAAKECY" varchar(70) NOT NULL default ' ',
  "AAAARHPS" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(8) NOT NULL default ' ',
  "AAAAKECM" varchar(6) NOT NULL default '000000',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAAAVBWQ" varchar(10) NOT NULL default '0000000000',
  "AAAAUPDH" varchar(10) NOT NULL default '0000000000',
  "AAAAKECR" int(11) NOT NULL default '0',
  "AAAAKECS" int(11) NOT NULL default '0',
  "AAAAKECT" int(11) NOT NULL default '0',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABIXXY" varchar(8) NOT NULL default ' ',
  "AAAATMFN" varchar(8) NOT NULL default ' ',
  "AAABHHRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKECY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKECZ" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAKEEH" varchar(10) NOT NULL default '0000000000',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  "AAABHHCG" varchar(32) NOT NULL default ' ',
  "AAAAMBWL" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAABILHK","AAABLEHK","AAABJZWV","AAAAKEEH"),
  KEY "AAAAKEDQ" ("AAABHHCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEDS" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAKEEH" varchar(10) NOT NULL default '0000000000',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW","AAABLOZE","AAABLUNJ","AAABILHK","AAABLEHK","AAABJZWV","AAAAKEEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEEI" (
  "AAAAKEES" varchar(10) NOT NULL default ' ',
  "AAABIMRU" varchar(32) NOT NULL default ' ',
  "AAAAKEEL" varchar(64) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAAAKEEN" int(11) NOT NULL default '0',
  "AAAAKEEO" int(11) NOT NULL default '0',
  "AAAAKEEP" int(11) NOT NULL default '0',
  "AAAAKEEQ" varchar(1) NOT NULL default ' ',
  "AAAAVIMQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKEES","AAABIMRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEEU" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAAAKEFJ" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAAAKEEZ" varchar(12) NOT NULL default ' ',
  "AAAAKEFA" varchar(8) NOT NULL default '00000000',
  "AAABHJBM" varchar(32) NOT NULL default ' ',
  "AAAAKEFC" varchar(8) NOT NULL default '00000000',
  "AAAAKEFD" varchar(8) NOT NULL default '00000000',
  "AAAAKEFE" varchar(8) NOT NULL default '00000000',
  "AAABENDJ" varchar(8) NOT NULL default ' ',
  "AAAAKEFG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAAAKEFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEFK" (
  "AAAAKEFS" varchar(1) NOT NULL default ' ',
  "AAAAKEFM" varchar(1) NOT NULL default ' ',
  "AAAAKEFN" varchar(1) NOT NULL default ' ',
  "AAAAKEFO" varchar(1) NOT NULL default ' ',
  "AAAAKEFP" varchar(4) NOT NULL default '0000',
  "AAAAKEFQ" varchar(4) NOT NULL default '0000',
  "AAAAKEFR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAKEFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEFT" (
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAAAKEFY" varchar(2) NOT NULL default '00',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAAAKEFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEFZ" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAABLUSG" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEGE" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEGK" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLVCU" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEGP" (
  "AAABHBUL" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAKEGW" varchar(40) NOT NULL default ' ',
  "AAAAKEGX" varchar(64) NOT NULL default ' ',
  "AAAAKEGY" varchar(12) NOT NULL default ' ',
  "AAAAKEGZ" varchar(1) NOT NULL default ' ',
  "AAAAKEHA" varchar(8) NOT NULL default '00000000',
  "AAAAKEHB" varchar(12) NOT NULL default ' ',
  "AAABHBUM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(6) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAAAKEHG" varchar(12) NOT NULL default ' ',
  "AAAAKEHH" varchar(8) NOT NULL default '00000000',
  "AAAAKEHI" varchar(12) NOT NULL default ' ',
  "AAAAKEHJ" varchar(8) NOT NULL default '00000000',
  "AAAAKEHK" varchar(16) NOT NULL default ' ',
  "AAAAKEHL" varchar(16) NOT NULL default ' ',
  "AAAAKEHM" varchar(50) NOT NULL default ' ',
  "AAAAKEHN" varchar(12) NOT NULL default ' ',
  "AAAAKEHO" varchar(8) NOT NULL default '00000000',
  "AAAAKEHP" varchar(12) NOT NULL default ' ',
  "AAAAKEHQ" varchar(8) NOT NULL default '00000000',
  "AAAAKEHR" varchar(16) NOT NULL default ' ',
  "AAAAKEHS" varchar(12) NOT NULL default ' ',
  "AAAAKEHT" varchar(8) NOT NULL default '00000000',
  "AAAAKEHU" varchar(12) NOT NULL default ' ',
  "AAAAKEHV" varchar(8) NOT NULL default '00000000',
  "AAAAKTIC" varchar(30) NOT NULL default ' ',
  "AAABLJJY" varchar(64) NOT NULL default ' ',
  "AAAAKEHY" varchar(1) NOT NULL default ' ',
  "AAAAKEHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBUL","AAABLOZE","AAABLUNJ","AAABILHK","AAABLEHK","AAABJZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEIG" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEIN" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAAATRZZ" varchar(15) NOT NULL default ' ',
  "AAAAKEIQ" varchar(10) NOT NULL default ' ',
  "AAAAKEIR" varchar(32) NOT NULL default ' ',
  "AAAAKEIS" varchar(10) NOT NULL default ' ',
  "AAAAKEIT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAAATRZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEIW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKEIY" varchar(4) NOT NULL default '0000',
  "AAAAKEIZ" varchar(50) NOT NULL default ' ',
  "AAAAKEJA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEJC" (
  "AAABIJYD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEJF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIJYD" varchar(10) NOT NULL default ' ',
  "AAABJEBV" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIJYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEJL" (
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAAAKEJN" varchar(1) NOT NULL default ' ',
  "AAAAKEJO" varchar(1) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEJT" (
  "AAAAKEQZ" varchar(3) NOT NULL default '000',
  "AAABLITM" varchar(64) NOT NULL default ' ',
  "AAABEBCL" varchar(64) NOT NULL default ' ',
  "AAABGHQL" varchar(2) NOT NULL default '00',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAKEJZ" varchar(255) NOT NULL default ' ',
  "AAAAKEKA" varchar(255) NOT NULL default ' ',
  "AAABKPMT" varchar(1) NOT NULL default '0',
  "AAAAKEKC" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKEQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEKE" (
  "AAABIYQP" varchar(10) NOT NULL default '0000000000',
  "AAAAKEQF" varchar(10) NOT NULL default ' ',
  "AAAAKEQG" varchar(32) NOT NULL default ' ',
  "AAABBJZL" varchar(40) NOT NULL default ' ',
  "AAAAKEQI" varchar(64) NOT NULL default ' ',
  "AAAAKEQJ" varchar(10) NOT NULL default ' ',
  "AAAAKEQK" varchar(32) NOT NULL default ' ',
  "AAAAKEQL" varchar(40) NOT NULL default ' ',
  "AAAAKEQM" varchar(64) NOT NULL default ' ',
  "AAABILHK" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAKEQP" varchar(10) NOT NULL default ' ',
  "AAAAVCTN" varchar(12) NOT NULL default ' ',
  "AAAAKEQR" varchar(1) NOT NULL default ' ',
  "AAAAQVTN" varchar(30) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAABIYQL" varchar(1) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAKEQY" varchar(3) NOT NULL default '000',
  "AAAAKEQZ" varchar(3) NOT NULL default '000',
  "AAAAVASH" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABBERJ" varchar(8) NOT NULL default '00000000',
  "AAAARPHN" varchar(6) NOT NULL default '000000',
  "AAAATJIW" varchar(8) NOT NULL default '00000000',
  "AAAAMACC" varchar(6) NOT NULL default '000000',
  "AAAAKERG" varchar(8) NOT NULL default '00000000',
  "AAAAKERH" varchar(6) NOT NULL default '000000',
  "AAAAKERI" varchar(8) NOT NULL default '00000000',
  "AAAAKERJ" varchar(6) NOT NULL default '000000',
  "AAAAPZFE" varchar(8) NOT NULL default '00000000',
  "AAAAXMJS" varchar(6) NOT NULL default '000000',
  "AAAAKERM" varchar(8) NOT NULL default '00000000',
  "AAAAKERN" varchar(6) NOT NULL default '000000',
  "AAAAMBNL" varchar(8) NOT NULL default '00000000',
  "AAAAKERP" varchar(6) NOT NULL default '000000',
  "AAAAKERQ" varchar(10) NOT NULL default '0000000000',
  "AAAAKERR" varchar(10) NOT NULL default '0000000000',
  "AAAAVBWQ" varchar(10) NOT NULL default '0000000000',
  "AAAAUPDH" varchar(10) NOT NULL default '0000000000',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKELX" (
  "AAAAKEQY" varchar(3) NOT NULL default '000',
  "AAAAKELZ" varchar(1) NOT NULL default ' ',
  "AAABFYIP" varchar(1) NOT NULL default ' ',
  "AAAAKEMB" varchar(1) NOT NULL default ' ',
  "AAAAKEMC" varchar(1) NOT NULL default ' ',
  "AAAAUOPA" varchar(1) NOT NULL default ' ',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  "AAAAKEMF" varchar(1) NOT NULL default ' ',
  "AAAAKEMG" varchar(1) NOT NULL default ' ',
  "AAAAKEMH" varchar(1) NOT NULL default ' ',
  "AAABLUCJ" varchar(1) NOT NULL default ' ',
  "AAAAKEMJ" varchar(1) NOT NULL default ' ',
  "AAAAKEMK" varchar(1) NOT NULL default ' ',
  "AAAAKEML" varchar(1) NOT NULL default ' ',
  "AAAAKEMM" varchar(5) NOT NULL default '00000',
  "AAAAKEMN" varchar(5) NOT NULL default '00000',
  "AAABCZKS" varchar(5) NOT NULL default '00000',
  "AAABCZKT" varchar(5) NOT NULL default '00000',
  "AAAAKEMQ" varchar(1) NOT NULL default ' ',
  "AAAAKEMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKEQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEMT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKEQY" varchar(3) NOT NULL default '000',
  "AAABHWUC" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAKEQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEMZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKENM" (
  "AAABIYQP" varchar(10) NOT NULL default '0000000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYQP","AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKENT" (
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKENW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABHWUC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEOC" (
  "AAABFTRT" varchar(10) NOT NULL default '0000000000',
  "AAABILHK" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAKEQP" varchar(10) NOT NULL default ' ',
  "AAAAVCTN" varchar(12) NOT NULL default ' ',
  "AAABBJZL" varchar(40) NOT NULL default ' ',
  "AAAAKEQI" varchar(64) NOT NULL default ' ',
  "AAAAQVTN" varchar(30) NOT NULL default ' ',
  "AAAAKEQZ" varchar(3) NOT NULL default '000',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAKEQY" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEOQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAKEPG" varchar(20) NOT NULL default ' ',
  "AAAAKEOT" varchar(20) NOT NULL default ' ',
  "AAAAKEOU" varchar(20) NOT NULL default ' ',
  "AAAAKEOV" varchar(4) NOT NULL default '0000',
  "AAAAKEOW" varchar(20) NOT NULL default ' ',
  "AAAAKEOX" varchar(4) NOT NULL default '0000',
  "AAAAKEOY" varchar(20) NOT NULL default ' ',
  "AAAAKEOZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEPB" (
  "AAAAKEQZ" varchar(3) NOT NULL default '000',
  "AAAAVASH" varchar(10) NOT NULL default '0000000000',
  "AAABIYQP" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAKEPG" varchar(20) NOT NULL default ' ',
  "AAAAKEPH" varchar(8) NOT NULL default '00000000',
  "AAAAKEPI" varchar(6) NOT NULL default '000000',
  "AAAAKEPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKEQZ","AAAAVASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEPM" (
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABIISW" varchar(12) NOT NULL default ' ',
  "AAAAVCYW" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEPV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABIISW" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV","AAABIISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEQD" (
  "AAABIYQP" varchar(10) NOT NULL default '0000000000',
  "AAAAKEQF" varchar(10) NOT NULL default ' ',
  "AAAAKEQG" varchar(32) NOT NULL default ' ',
  "AAABBJZL" varchar(40) NOT NULL default ' ',
  "AAAAKEQI" varchar(64) NOT NULL default ' ',
  "AAAAKEQJ" varchar(10) NOT NULL default ' ',
  "AAAAKEQK" varchar(32) NOT NULL default ' ',
  "AAAAKEQL" varchar(40) NOT NULL default ' ',
  "AAAAKEQM" varchar(64) NOT NULL default ' ',
  "AAABILHK" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAKEQP" varchar(10) NOT NULL default ' ',
  "AAAAVCTN" varchar(12) NOT NULL default ' ',
  "AAAAKEQR" varchar(1) NOT NULL default ' ',
  "AAAAQVTN" varchar(30) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAABIYQL" varchar(1) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default ' ',
  "AAAAKEQY" varchar(3) NOT NULL default '000',
  "AAAAKEQZ" varchar(3) NOT NULL default '000',
  "AAAAVASH" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABBERJ" varchar(8) NOT NULL default '00000000',
  "AAAARPHN" varchar(6) NOT NULL default '000000',
  "AAAATJIW" varchar(8) NOT NULL default '00000000',
  "AAAAMACC" varchar(6) NOT NULL default '000000',
  "AAAAKERG" varchar(8) NOT NULL default '00000000',
  "AAAAKERH" varchar(6) NOT NULL default '000000',
  "AAAAKERI" varchar(8) NOT NULL default '00000000',
  "AAAAKERJ" varchar(6) NOT NULL default '000000',
  "AAAAPZFE" varchar(8) NOT NULL default '00000000',
  "AAAAXMJS" varchar(6) NOT NULL default '000000',
  "AAAAKERM" varchar(8) NOT NULL default '00000000',
  "AAAAKERN" varchar(6) NOT NULL default '000000',
  "AAAAMBNL" varchar(8) NOT NULL default '00000000',
  "AAAAKERP" varchar(6) NOT NULL default '000000',
  "AAAAKERQ" varchar(10) NOT NULL default '0000000000',
  "AAAAKERR" varchar(10) NOT NULL default '0000000000',
  "AAAAVBWQ" varchar(10) NOT NULL default '0000000000',
  "AAAAUPDH" varchar(10) NOT NULL default '0000000000',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKERW" (
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKERZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKESF" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAAAKESU" varchar(15) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAAAKESU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKESN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAAAKESU" varchar(15) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKGBL","AAAAKESU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKESV" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAKEYR" varchar(10) NOT NULL default ' ',
  "AAAAKEYS" varchar(32) NOT NULL default ' ',
  "AAAAKEYT" varchar(10) NOT NULL default ' ',
  "AAAAKEYU" varchar(32) NOT NULL default ' ',
  "AAABIRUN" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(4) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default '0000',
  "AAAAKEYV" varchar(8) NOT NULL default ' ',
  "AAAAKEYW" varchar(60) NOT NULL default ' ',
  "AAAATMFY" varchar(12) NOT NULL default ' ',
  "AAAAUNDZ" varchar(8) NOT NULL default '00000000',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAKEYY" varchar(12) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAKEXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV"),
  UNIQUE KEY "AAAAKETO" ("AAABIRUN","AAABMDEP","AAABMNDU","AAABECXR","AAAAKEYV","AAAAKEYW","AAABJZWV","AAAAKEYY","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKETY" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABHAZI" varchar(5) NOT NULL default ' ',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHAZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEUE" (
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABMMJT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEUI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEUO" (
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAABILHK" varchar(12) NOT NULL default ' ',
  "AAAALLLW" varchar(10) NOT NULL default ' ',
  "AAAALLLX" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZWV","AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEUY" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAAAKEVF" varchar(10) NOT NULL default ' ',
  "AAAAKEVG" varchar(10) NOT NULL default ' ',
  "AAABIFOK" varchar(2) NOT NULL default '00',
  "AAABLMUZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLOZE","AAAAKEVF","AAAAKEVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEVH" (
  "AAABHAZI" varchar(5) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEVL" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEVQ" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABLOZE","AAABMLPV","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEWC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKGBL","AAABLOZE","AAABMLPV","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEWO" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAAAKFAN" varchar(12) NOT NULL default ' ',
  "AAAAKEXA" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAAAKFAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEWW" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAAAKFAN" varchar(12) NOT NULL default ' ',
  "AAAAKEXA" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLOZE","AAABLUNJ","AAAAKFAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEXE" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAKEYR" varchar(10) NOT NULL default ' ',
  "AAAAKEYS" varchar(32) NOT NULL default ' ',
  "AAAAKEYT" varchar(10) NOT NULL default ' ',
  "AAAAKEYU" varchar(32) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default '0000',
  "AAAAKEYV" varchar(40) NOT NULL default ' ',
  "AAAAKEYW" varchar(60) NOT NULL default ' ',
  "AAAATMFY" varchar(12) NOT NULL default ' ',
  "AAAAUNDZ" varchar(8) NOT NULL default '00000000',
  "AAABJZWV" varchar(10) NOT NULL default ' ',
  "AAAAKEYY" varchar(12) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAKEXV" varchar(1) NOT NULL default ' ',
  "AAAAKEXW" varchar(4) NOT NULL default '0000',
  "AAAAKEXX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV"),
  KEY "AAAAKEXZ" ("AAAAKEYR","AAAAKEYS","AAAAKEYT","AAAAKEYU","AAABIRUN","AAABJZWV","AAAAKEYY","AAABLEHK"),
  KEY "AAAAKEYI" ("AAABIRUN","AAABMDEP","AAABMNDU","AAABECXR","AAABJZWV","AAAAKEYY","AAABLEHK"),
  KEY "AAAAKEYQ" ("AAAAKEYR","AAAAKEYS","AAAAKEYT","AAAAKEYU","AAAAKEYV","AAAAKEYW","AAABJZWV","AAAAKEYY","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEZA" (
  "AAABILHK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEZD" (
  "AAAAKEZK" varchar(10) NOT NULL default ' ',
  "AAABJAAI" varchar(10) NOT NULL default ' ',
  "AAABILHK" varchar(12) NOT NULL default ' ',
  "AAABIKDN" varchar(70) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJSHI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKEZK","AAABJAAI","AAABILHK","AAABIKDN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEZP" (
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLITM" varchar(100) NOT NULL default ' ',
  "AAABLQHQ" varchar(128) NOT NULL default ' ',
  "AAABMLQA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKEZV" (
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLEJW" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFAB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKGBL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFAJ" (
  "AAAAKFAN" varchar(12) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKFAN","AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFAQ" (
  "AAAAKFBB" varchar(8) NOT NULL default ' ',
  "AAAAKFAS" varchar(32) NOT NULL default ' ',
  "AAAAKFAT" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKFBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFAW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKFBB" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAKFBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFBC" (
  "AAABGFRN" varchar(25) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFBG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFRN" varchar(25) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGFRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFBM" (
  "AAABIFRO" varchar(12) NOT NULL default ' ',
  "AAABGFRN" varchar(25) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLKWV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABIFRO","AAABGFRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLPTE" varchar(12) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAAAKFCD" varchar(4) NOT NULL default ' ',
  "AAABLRTU" varchar(8) NOT NULL default '00000000',
  "AAAAKFCF" varchar(8) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABFPKP" varchar(1) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGCM" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAAAKFCZ" varchar(18) NOT NULL default ' ',
  "AAAAKFCU" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLGCM","AAABMOHM","AAAAKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTYH" (
  "AAABLGCM" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFDF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGCM" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAAAVNHH" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLGCM","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAAAQWTN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQWTN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQWTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQWTN" varchar(6) NOT NULL default ' ',
  "AAAAKFEC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQWTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAAAKGZT" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAABGHHC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKNZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQWTN" varchar(6) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAQWTN","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLDAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAAAKOAQ" smallint(6) NOT NULL default '0',
  "AAAAKOAR" varchar(1) NOT NULL default ' ',
  "AAAAKOAN" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLDAF","AAABMDYP","AAABLDBT","AAABMDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFGK" (
  "AAAAKFSQ" varchar(2) NOT NULL default '00',
  "AAABKYRC" varchar(1) NOT NULL default '0',
  "AAAAKFHB" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKFSQ","AAABKYRC","AAAAKFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFGS" (
  "AAAAKFSQ" varchar(2) NOT NULL default '00',
  "AAABKYRC" varchar(1) NOT NULL default '0',
  "AAAAKFHB" varchar(1) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABGIHX" varchar(1) NOT NULL default ' ',
  "AAAAKFGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKFSQ","AAABKYRC","AAAAKFHB","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFIS" varchar(12) NOT NULL default '000000000000',
  "AAAAKFHG" varchar(8) NOT NULL default '00000000',
  "AAAAKFHH" varchar(6) NOT NULL default '000000',
  "AAAAKFHI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKFIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKFIS" varchar(12) NOT NULL default '000000000000',
  "AAAAKFHP" varchar(30) NOT NULL default ' ',
  "AAAAKFHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKFIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFIS" varchar(12) NOT NULL default '000000000000',
  "AAAAKJMU" varchar(1) NOT NULL default ' ',
  "AAABKYVO" varchar(10) NOT NULL default ' ',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKFIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFIS" varchar(12) NOT NULL default '000000000000',
  "AAABKYRC" varchar(1) NOT NULL default '0',
  "AAAAKJMU" varchar(1) NOT NULL default ' ',
  "AAABKYVO" varchar(10) NOT NULL default ' ',
  "AAAAKNFI" varchar(30) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAAAKFIO" int(11) NOT NULL default '0',
  "AAAAUUTB" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKFIS","AAABKYRC","AAAAKJMU","AAABKYVO","AAAAKNFI","AAABLXTG","AAABMEFM","AAABMEFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAAAKFJH" varchar(2) NOT NULL default ' ',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAAAKFJJ" varchar(2) NOT NULL default ' ',
  "AAABFTWG" varchar(10) NOT NULL default ' ',
  "AAAAKFJL" varchar(4) NOT NULL default ' ',
  "AAAAKFJM" varchar(5) NOT NULL default ' ',
  "AAAAKFJN" varchar(5) NOT NULL default ' ',
  "AAABLWPJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAKFJP" varchar(1) NOT NULL default ' ',
  "AAAAKFJQ" smallint(6) NOT NULL default '0',
  "AAAAKFJR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPC","AAAAKNLT","AAAAKNLU","AAABMLPV","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFNYB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPC","AAAAKNLT","AAAAKNLU","AAABMLPV","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAAAKJYR" varchar(12) NOT NULL default ' ',
  "AAAAKFRI" varchar(1) NOT NULL default ' ',
  "AAAAKFLD" varchar(8) NOT NULL default '00000000',
  "AAABKKHJ" int(11) NOT NULL default '0',
  "AAAAKMGO" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFKV" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFKW" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNFX","AAAAKJYR","AAAAKFRI","AAAAKFLD","AAABKKHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAAAKFTX" varchar(3) NOT NULL default ' ',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKFSY" varchar(1) NOT NULL default ' ',
  "AAAAKFSZ" varchar(1) NOT NULL default ' ',
  "AAAAKFTA" varchar(1) NOT NULL default ' ',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTC" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTD" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTE" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTF" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTG" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTH" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTI" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMAJL" decimal(3,0) NOT NULL default '0',
  "AAAAWDEM" varchar(1) NOT NULL default ' ',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKFSO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPC","AAAAKNLT","AAAAKNLU","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMRCU" int(11) NOT NULL default '0',
  "AAAAKFRI" varchar(1) NOT NULL default ' ',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAAAKFMS" varchar(3) NOT NULL default '000',
  "AAAAKFSC" int(11) NOT NULL default '0',
  "AAAAKFQV" varchar(3) NOT NULL default '000',
  "AAAAKFQW" varchar(3) NOT NULL default '000',
  "AAABKWVR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFQY" decimal(10,7) NOT NULL default '0.0000000',
  "AAABIYVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAKFRB" decimal(10,7) NOT NULL default '0.0000000',
  "AAABIWAM" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKMPC","AAAAKNLT","AAAAKNLU","AAABMLPV","AAABMRCU","AAAAKFRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAAAKMPR" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAQTEC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPC","AAAAKNLT","AAAAKNLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKMPC","AAAAKNLT","AAAAKNLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIGV" varchar(5) NOT NULL default ' ',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKFOM" varchar(100) NOT NULL default ' ',
  "AAAAKFON" varchar(100) NOT NULL default ' ',
  "AAAAKFOO" varchar(100) NOT NULL default ' ',
  "AAAAKFOP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIGV","AAAAKIHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFPB" varchar(16) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKFOZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKFPB","AAABLDAF","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFRS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKFRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKFTX" varchar(3) NOT NULL default ' ',
  "AAAAKFRS" varchar(6) NOT NULL default ' ',
  "AAABLPGZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKFSB" decimal(5,2) NOT NULL default '0.00',
  "AAAAKFSC" int(11) NOT NULL default '0',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKFSG" varchar(3) NOT NULL default '000',
  "AAAAKFSH" varchar(8) NOT NULL default '00000000',
  "AAAAKFSI" varchar(8) NOT NULL default '00000000',
  "AAAAKFSJ" varchar(8) NOT NULL default '00000000',
  "AAAAKFSK" varchar(5) NOT NULL default ' ',
  "AAAAKFPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKFTX","AAAAKFRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKFRS" varchar(6) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKFRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAAAKJYR" varchar(12) NOT NULL default ' ',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAKFRI" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAAAKFSC" int(11) NOT NULL default '0',
  "AAAAKFQV" varchar(3) NOT NULL default '000',
  "AAAAKFQW" varchar(3) NOT NULL default '000',
  "AAABKWVR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFQY" decimal(10,7) NOT NULL default '0.0000000',
  "AAABIYVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAKFRB" decimal(10,7) NOT NULL default '0.0000000',
  "AAABIWAM" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNFX","AAAAKJYR","AAAAKNFI","AAABMRFQ","AAAAKFRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAAAKJYR" varchar(12) NOT NULL default ' ',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAKFTX" varchar(3) NOT NULL default ' ',
  "AAAAKFRS" varchar(6) NOT NULL default ' ',
  "AAAAKFRT" varchar(1) NOT NULL default ' ',
  "AAAAKFRU" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default ' ',
  "AAAAKMPC" varchar(12) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABLPGZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKFSB" decimal(5,2) NOT NULL default '0.00',
  "AAAAKFSC" int(11) NOT NULL default '0',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKFSG" varchar(3) NOT NULL default '000',
  "AAAAKFSH" varchar(8) NOT NULL default '00000000',
  "AAAAKFSI" varchar(8) NOT NULL default '00000000',
  "AAAAKFSJ" varchar(8) NOT NULL default '00000000',
  "AAAAKFSK" varchar(5) NOT NULL default ' ',
  "AAABMAJL" decimal(3,0) NOT NULL default '0',
  "AAAAWDEM" varchar(1) NOT NULL default ' ',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKFSO" varchar(1) NOT NULL default ' ',
  "AAAAKFSP" int(11) NOT NULL default '0',
  "AAAAKFSQ" varchar(2) NOT NULL default '00',
  "AAAAKFSR" varchar(1) NOT NULL default ' ',
  "AAAAKFSS" varchar(3) NOT NULL default ' ',
  "AAAAKFST" varchar(4) NOT NULL default ' ',
  "AAAAKFSU" varchar(6) NOT NULL default ' ',
  "AAAAKNFZ" varchar(1) NOT NULL default ' ',
  "AAAAKNGA" varchar(10) NOT NULL default ' ',
  "AAABKYVO" varchar(10) NOT NULL default ' ',
  "AAAAKFSY" varchar(1) NOT NULL default ' ',
  "AAAAKFSZ" varchar(1) NOT NULL default ' ',
  "AAAAKFTA" varchar(1) NOT NULL default ' ',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTC" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTD" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTE" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTF" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTG" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTH" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTI" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFTJ" int(11) NOT NULL default '0',
  "AAAAKFTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNFX","AAAAKJYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFTO" (
  "AAAAKFTX" varchar(3) NOT NULL default ' ',
  "AAAAKFTQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKFTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFTS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKFTX" varchar(3) NOT NULL default ' ',
  "AAABDXVJ" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAKFTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNZO","AAABLDAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAAAKFVB" varchar(5) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAAAKHUM" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHUN" varchar(1) NOT NULL default ' ',
  "AAAAKOAQ" smallint(6) NOT NULL default '0',
  "AAAAKOAR" varchar(1) NOT NULL default ' ',
  "AAAAKOAN" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKNZO","AAABLDAF","AAABMDYP","AAAAKFVB","AAABLDBT","AAABMDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGBA" varchar(5) NOT NULL default ' ',
  "AAAAKFVH" varchar(1) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAAAKNTW" varchar(1) NOT NULL default ' ',
  "AAAAKNTX" varchar(1) NOT NULL default ' ',
  "AAAAKNUH" varchar(1) NOT NULL default ' ',
  "AAAAKNUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKGBA" varchar(5) NOT NULL default ' ',
  "AAAAKFVT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKGBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIGV" varchar(5) NOT NULL default ' ',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKGBA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIGV","AAAAKIHI","AAAAKGBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKGCA" varchar(8) NOT NULL default '00000000',
  "AAAAKFWM" varchar(2) NOT NULL default ' ',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAAAKGBA" varchar(5) NOT NULL default ' ',
  "AAAAKGBB" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGBC" varchar(5) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDID" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAB" varchar(1) NOT NULL default '0',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKGBQ" varchar(2) NOT NULL default ' ',
  "AAABLCXZ" varchar(1) NOT NULL default ' ',
  "AAABLCYD" varchar(1) NOT NULL default ' ',
  "AAAAQIXO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK","AAAAKIHI","AAAAKGCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKFYH" varchar(8) NOT NULL default '00000000',
  "AAAAKFXT" varchar(8) NOT NULL default '00000000',
  "AAAAKFZL" varchar(8) NOT NULL default '00000000',
  "AAAAKFZM" varchar(6) NOT NULL default '000000',
  "AAAAKFXW" decimal(17,2) NOT NULL default '0.00',
  "AAAAKFXX" varchar(5) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKGBQ" varchar(2) NOT NULL default ' ',
  "AAAAKFYA" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFYB" varchar(1) NOT NULL default ' ',
  "AAAAKFYC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK","AAAAKIHI","AAAAKFYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKFYW" varchar(8) NOT NULL default '00000000',
  "AAAAKFYX" varchar(10) NOT NULL default ' ',
  "AAABMQRD" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKFYQ" varchar(8) NOT NULL default '00000000',
  "AAAAKFYR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK","AAAAKIHI","AAAAKFYW","AAAAKFYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAABGNSP" varchar(40) NOT NULL default ' ',
  "AAABIOCE" varchar(12) NOT NULL default ' ',
  "AAAAKGAM" varchar(8) NOT NULL default '00000000',
  "AAAAKGAN" varchar(6) NOT NULL default '000000',
  "AAAAOZTV" varchar(12) NOT NULL default ' ',
  "AAAAKFZH" varchar(8) NOT NULL default '00000000',
  "AAAAKFZI" varchar(6) NOT NULL default '000000',
  "AAAAKIGV" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKFZL" varchar(8) NOT NULL default '00000000',
  "AAAAKFZM" varchar(6) NOT NULL default '000000',
  "AAAAKFZN" varchar(8) NOT NULL default '00000000',
  "AAAAKFZO" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK"),
  UNIQUE KEY "AAAAKFZT" ("AAABMRIL","AAABGNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKFZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKGAB" varchar(5) NOT NULL default ' ',
  "AAAAKGAC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK","AAAAKIHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPFU" varchar(13) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAABIOCE" varchar(12) NOT NULL default ' ',
  "AAAAKGAM" varchar(8) NOT NULL default '00000000',
  "AAAAKGAN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCPFU","AAAAKGCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKGCM" varchar(8) NOT NULL default '00000000',
  "AAAAKGCN" varchar(2) NOT NULL default ' ',
  "AAAAKGCA" varchar(8) NOT NULL default '00000000',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAAAKGBA" varchar(5) NOT NULL default ' ',
  "AAAAKGBB" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGBC" varchar(5) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDID" varchar(1) NOT NULL default '0',
  "AAABMDZV" varchar(1) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMEAB" varchar(1) NOT NULL default '0',
  "AAABMEAC" varchar(1) NOT NULL default ' ',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMDHW" varchar(1) NOT NULL default '0',
  "AAABMDHX" varchar(1) NOT NULL default ' ',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMDHZ" varchar(1) NOT NULL default '0',
  "AAABMDIA" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKGBQ" varchar(2) NOT NULL default ' ',
  "AAABLCXZ" varchar(1) NOT NULL default ' ',
  "AAABLCYD" varchar(1) NOT NULL default ' ',
  "AAAAQIXO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKGCK","AAAAKIHI","AAAAKGCM","AAAAKGCN","AAAAKGCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKGCK" varchar(5) NOT NULL default '00000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKGCM" varchar(8) NOT NULL default '00000000',
  "AAAAKGCN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKGCK","AAAAKIHI","AAAAKGCM","AAAAKGCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAAAKGDC" varchar(5) NOT NULL default ' ',
  "AAAAWXUP" varchar(8) NOT NULL default '00000000',
  "AAAAKGDE" varchar(8) NOT NULL default '00000000',
  "AAAAKLSW" int(11) NOT NULL default '0',
  "AAAAKGCV" varchar(1) NOT NULL default ' ',
  "AAAAKGCW" varchar(1) NOT NULL default ' ',
  "AAAAKGCX" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKGCY" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKGCZ" decimal(10,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXD","AAAAKGDC","AAAAWXUP","AAAAKGDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABHYJH" varchar(5) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAAAKGDL" varchar(1) NOT NULL default '0',
  "AAAAKGDM" varchar(1) NOT NULL default ' ',
  "AAAAKGDN" varchar(1) NOT NULL default ' ',
  "AAAAKGDO" varchar(1) NOT NULL default ' ',
  "AAAAKGDP" varchar(1) NOT NULL default ' ',
  "AAABKWRN" varchar(1) NOT NULL default ' ',
  "AAAAKGDR" varchar(1) NOT NULL default ' ',
  "AAAAKGDS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYXD","AAABHYJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABHYJH" varchar(5) NOT NULL default ' ',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAABBOPO" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXD","AAABHYJH","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAKGEP" varchar(5) NOT NULL default ' ',
  "AAAAKGEL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXD","AAABMNZR","AAAAKGEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  "AAAAKGEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGEY" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGHN" varchar(16) NOT NULL default ' ',
  "AAAAKGFE" varchar(20) NOT NULL default ' ',
  "AAAAKGFF" varchar(3) NOT NULL default ' ',
  "AAAAKGFG" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKGFI" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABKZRB" varchar(10) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKZYR" varchar(13) NOT NULL default ' ',
  "AAABKZYE" varchar(8) NOT NULL default '00000000',
  "AAABKZYF" varchar(8) NOT NULL default '00000000',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  "AAABLAPD" varchar(1) NOT NULL default ' ',
  "AAAAKGFS" varchar(8) NOT NULL default '00000000',
  "AAAAKGFT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKGHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGFW" (
  "AAAAKGGQ" varchar(10) NOT NULL default ' ',
  "AAAAKGGJ" varchar(4) NOT NULL default '0000',
  "AAAAKHFL" varchar(4) NOT NULL default '0000',
  "AAAAKGGR" varchar(20) NOT NULL default ' ',
  "AAAAKGGB" varchar(1) NOT NULL default ' ',
  "AAAAKGGP" varchar(2) NOT NULL default ' ',
  "AAAAKGGD" varchar(10) NOT NULL default ' ',
  "AAAAKGGE" varchar(10) NOT NULL default ' ',
  "AAAAKGGF" varchar(1) NOT NULL default ' ',
  "AAAAKGGG" varchar(1) NOT NULL default ' ',
  "AAAAKGGH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGGQ","AAAAKGGJ","AAAAKHFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGGL" (
  "AAAAKGGQ" varchar(10) NOT NULL default ' ',
  "AAAAKGGR" varchar(20) NOT NULL default ' ',
  "AAABLCCM" varchar(4) NOT NULL default '0000',
  "AAAAKGGP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGGQ","AAAAKGGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGHN" varchar(16) NOT NULL default ' ',
  "AAAAKGHO" varchar(5) NOT NULL default '00000',
  "AAAAKGHQ" varchar(22) NOT NULL default ' ',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMDHS" decimal(17,2) NOT NULL default '0.00',
  "AAABLACT" decimal(15,6) NOT NULL default '0.000000',
  "AAABLACQ" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(17,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGHG" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGHH" decimal(17,2) NOT NULL default '0.00',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAAAKGHK" varchar(2) NOT NULL default ' ',
  "AAAAKGHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGHN","AAAAKGHO"),
  KEY "AAAAKGHP" ("AAAAKGHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGID" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKGID" varchar(10) NOT NULL default ' ',
  "AAAAKGIA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKGID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAPJWN" varchar(3) NOT NULL default ' ',
  "AAAAKGIH" varchar(1) NOT NULL default ' ',
  "AAAAKGII" decimal(5,2) NOT NULL default '0.00',
  "AAAAKGIJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAPJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAABJNOE" varchar(8) NOT NULL default '00000000',
  "AAAAKOBD" varchar(8) NOT NULL default ' ',
  "AAAAKGIS" varchar(3) NOT NULL default ' ',
  "AAAAKGIT" varchar(10) NOT NULL default ' ',
  "AAAAKGIU" varchar(6) NOT NULL default ' ',
  "AAAAKGIV" varchar(2) NOT NULL default ' ',
  "AAAAKGUF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCA","AAABFJCB","AAABJNOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAAAKNRN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAAAKGJL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFJCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAAAKGJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFJCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGWJ" varchar(10) NOT NULL default '0000000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAAAKGLE" varchar(13) NOT NULL default ' ',
  "AAAAKGKI" varchar(10) NOT NULL default ' ',
  "AAABKXRL" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAAAKGKL" varchar(13) NOT NULL default ' ',
  "AAAAKGKM" varchar(5) NOT NULL default ' ',
  "AAAAKGKN" varchar(4) NOT NULL default ' ',
  "AAAAKMXK" varchar(1) NOT NULL default ' ',
  "AAAAKGKP" varchar(3) NOT NULL default ' ',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  "AAAAKGUF" varchar(8) NOT NULL default ' ',
  "AAAAKGKT" varchar(8) NOT NULL default '00000000',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAAAKNWC" varchar(1) NOT NULL default ' ',
  "AAAAKGKY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKGWJ"),
  KEY "AAAAKGLB" ("AAABMRIL","AAABMREX","AAAAKGLE","AAABLAYL"),
  KEY "AAAAKGLG" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGWJ" varchar(10) NOT NULL default '0000000000',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAAAKOBD" varchar(8) NOT NULL default ' ',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAAAKNZG" varchar(8) NOT NULL default '00000000',
  "AAAAKNZI" varchar(8) NOT NULL default '00000000',
  "AAAAKNZJ" varchar(3) NOT NULL default '000',
  "AAAAKNZK" varchar(4) NOT NULL default '0000',
  "AAAAKGWH" varchar(8) NOT NULL default '00000000',
  "AAAAKGWI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKGWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAAAKGMA" varchar(2) NOT NULL default ' ',
  "AAAAKGMB" varchar(1) NOT NULL default ' ',
  "AAAAKGMC" varchar(1) NOT NULL default ' ',
  "AAAAKGMD" varchar(1) NOT NULL default ' ',
  "AAAAKGME" varchar(1) NOT NULL default ' ',
  "AAAAKMXK" varchar(1) NOT NULL default ' ',
  "AAAAKGMG" varchar(1) NOT NULL default ' ',
  "AAAAKGMH" varchar(1) NOT NULL default ' ',
  "AAAAKGMI" varchar(1) NOT NULL default ' ',
  "AAAAKGMJ" varchar(1) NOT NULL default ' ',
  "AAABJMAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKGNG" varchar(8) NOT NULL default '00000000',
  "AAAAKGMR" decimal(13,3) NOT NULL default '0.000',
  "AAAAKGMS" decimal(13,3) NOT NULL default '0.000',
  "AAAAKGMT" varchar(6) NOT NULL default '000000',
  "AAAAKGMU" varchar(6) NOT NULL default '000000',
  "AAABKTUJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGMW" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGMX" decimal(15,6) NOT NULL default '0.000000',
  "AAAAKGMY" decimal(9,5) NOT NULL default '0.00000',
  "AAAAKGMZ" decimal(8,3) NOT NULL default '0.000',
  "AAAAKGNA" decimal(17,2) NOT NULL default '0.00',
  "AAAAKGNB" decimal(10,5) NOT NULL default '0.00000',
  "AAABMDHR" decimal(17,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKGNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGTG","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGNQ" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGNX" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG","AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAAAKGOW" varchar(1) NOT NULL default ' ',
  "AAAAKGOX" varchar(70) NOT NULL default ' ',
  "AAAAKGOY" varchar(70) NOT NULL default ' ',
  "AAABLSDL" varchar(20) NOT NULL default ' ',
  "AAAAKGPA" varchar(1) NOT NULL default ' ',
  "AAAAKGPB" varchar(1) NOT NULL default ' ',
  "AAABKMUI" varchar(1) NOT NULL default ' ',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABLOCX" varchar(14) NOT NULL default ' ',
  "AAAAKGPF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAAAKGOW" varchar(1) NOT NULL default ' ',
  "AAAAKGOX" varchar(70) NOT NULL default ' ',
  "AAAAKGOY" varchar(70) NOT NULL default ' ',
  "AAABLSDL" varchar(20) NOT NULL default ' ',
  "AAAAKGPA" varchar(1) NOT NULL default ' ',
  "AAAAKGPB" varchar(1) NOT NULL default ' ',
  "AAABKMUI" varchar(1) NOT NULL default ' ',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABLOCX" varchar(14) NOT NULL default ' ',
  "AAAAKGPF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAAAKGPZ" varchar(1) NOT NULL default ' ',
  "AAABGUFZ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC","AAABIWWJ"),
  KEY "AAAAKGPS" ("AAAAKGPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGQC" varchar(10) NOT NULL default '0000000000',
  "AAABIWWJ" varchar(30) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAAAKGPZ" varchar(1) NOT NULL default ' ',
  "AAABGUFZ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGQC","AAABIWWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAAAKGQH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAAAKGQN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKHJN" varchar(2) NOT NULL default ' ',
  "AAAAKGRI" varchar(1) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAGS" varchar(13) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAAAKGZT" varchar(18) NOT NULL default ' ',
  "AAAAYVAO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAAAKGRF" ("AAABMREX","AAAAKHJN","AAAAKGRI","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGRM" varchar(1) NOT NULL default ' ',
  "AAABCKQQ" varchar(10) NOT NULL default ' ',
  "AAABCPXN" varchar(2) NOT NULL default ' ',
  "AAABGWKJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGRR" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAAAKGRZ" varchar(1) NOT NULL default ' ',
  "AAAAKGRU" varchar(30) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAAAKGRW" varchar(30) NOT NULL default ' ',
  "AAAAKGRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG","AAAAKGRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGSA" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKGSD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGSG" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGSJ" (
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKGTG","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGTG" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAASNWT" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKGTG","AAABMREF","AAAASNWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGUF" varchar(8) NOT NULL default ' ',
  "AAABKYXD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAKGUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKGUF" varchar(8) NOT NULL default ' ',
  "AAAAKGTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKGUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKGUF" varchar(8) NOT NULL default ' ',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAAAKGUC" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAKGUF","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAKGUO" varchar(30) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAKGUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQGL","AAABLCAK","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMDG" varchar(10) NOT NULL default ' ',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAAAKOBD" varchar(8) NOT NULL default ' ',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAAAKNZG" varchar(8) NOT NULL default '00000000',
  "AAAAKGWS" varchar(30) NOT NULL default ' ',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKGVI" varchar(10) NOT NULL default ' ',
  "AAAAKHOG" varchar(5) NOT NULL default ' ',
  "AAAAKHJP" varchar(8) NOT NULL default ' ',
  "AAAAKHJQ" varchar(1) NOT NULL default ' ',
  "AAAAKGVM" varchar(8) NOT NULL default '00000000',
  "AAAAKNZI" varchar(8) NOT NULL default '00000000',
  "AAAAKNZJ" varchar(3) NOT NULL default '000',
  "AAAAKNZK" varchar(4) NOT NULL default '0000',
  "AAAAKNZX" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKNZY" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKNZZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKOAA" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKOAB" decimal(17,2) NOT NULL default '0.00',
  "AAAAKOAC" decimal(17,2) NOT NULL default '0.00',
  "AAAAKOAD" decimal(17,2) NOT NULL default '0.00',
  "AAAAKOAE" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  "AAAAKNVB" varchar(1) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABLCCM" varchar(8) NOT NULL default '00000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAAAKGWH" varchar(8) NOT NULL default '00000000',
  "AAAAKGWI" varchar(8) NOT NULL default '00000000',
  "AAAAKGWJ" varchar(10) NOT NULL default '0000000000',
  "AAAAKHLH" varchar(1) NOT NULL default ' ',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKHLJ" varchar(1) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABJMDG"),
  KEY "AAAAKGWQ" ("AAABMRIL","AAAAKGWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKGXB" varchar(22) NOT NULL default ' ',
  "AAAAKOAH" varchar(2) NOT NULL default ' ',
  "AAABMQHO" varchar(22) NOT NULL default ' ',
  "AAAAKGWY" decimal(15,3) NOT NULL default '0.000',
  "AAAAKGWZ" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAKGXB","AAAAKOAH","AAABMQHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGXE" (
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABLXTG","AAABLDAF","AAABMDYP","AAABLDBT","AAABMDYS","AAABMDZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGXT" (
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLXTG","AAABLDAF","AAABMDYP","AAABLDBT","AAABMDYS","AAABMDZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLDAF","AAABMDYP","AAABLDBT","AAABMDYS","AAABMDZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKGZT" varchar(18) NOT NULL default ' ',
  "AAABBWRC" varchar(10) NOT NULL default ' ',
  "AAAAKGZI" varchar(5) NOT NULL default ' ',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  "AAAAKGZL" varchar(16) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKGZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNRD","AAABMREX"),
  KEY "AAAAKGZS" ("AAAAKGZT","AAABMREX","AAABMRIL"),
  KEY "AAAAKGZW" ("AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKGZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKHAN" varchar(8) NOT NULL default '00000000',
  "AAAAKHAE" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHAF" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  "AAAAKHAI" varchar(1) NOT NULL default ' ',
  "AAAAKHAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNRD","AAABMREX","AAAAKHAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABLCQY" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNRD","AAABMREX","AAABLDAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLDBA" varchar(13) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABLDAG" varchar(3) NOT NULL default '000',
  "AAAAKICQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLDAZ","AAABLDBA","AAABLDAF","AAABLDAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABLCVG" varchar(1) NOT NULL default ' ',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAAAKHUM" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHUN" varchar(1) NOT NULL default ' ',
  "AAAAKOAQ" smallint(6) NOT NULL default '0',
  "AAAAKOAR" varchar(1) NOT NULL default ' ',
  "AAAAKOAN" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKNRD","AAABMREX","AAABLDAF","AAABMDYP","AAABLCPX","AAABLDBT","AAABMDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKOBD" varchar(8) NOT NULL default ' ',
  "AAAAKHDF" varchar(4) NOT NULL default ' ',
  "AAABMDYS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKOBX" varchar(1) NOT NULL default ' ',
  "AAAAKOBY" varchar(1) NOT NULL default ' ',
  "AAAAKOBZ" varchar(1) NOT NULL default ' ',
  "AAAAKOCA" varchar(1) NOT NULL default ' ',
  "AAAAKOCB" varchar(1) NOT NULL default ' ',
  "AAAAKOCC" varchar(1) NOT NULL default ' ',
  "AAAAKOCD" varchar(1) NOT NULL default ' ',
  "AAABJMAV" varchar(1) NOT NULL default ' ',
  "AAAAKOCF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKOBD","AAAAKHDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKHDU" varchar(10) NOT NULL default '0000000000',
  "AAABLADP" varchar(8) NOT NULL default '00000000',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAAAKHDN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHDO" varchar(1) NOT NULL default ' ',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNRD","AAABMREX","AAAAKHDU"),
  KEY "AAAAKHDV" ("AAABLADP","AAAAKNRD","AAABMREX"),
  KEY "AAAAKHDZ" ("AAABMDZT","AAAAKNRD","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKNUL" varchar(4) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAAAKIEN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIEM" varchar(5) NOT NULL default ' ',
  "AAAAKIDM" varchar(8) NOT NULL default '00000000',
  "AAAAKHEL" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIES" varchar(8) NOT NULL default '00000000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKNUL","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJREU" varchar(1) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAAAKHEX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJREU","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHFC" (
  "AAAALARQ" varchar(3) NOT NULL default '000',
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAAAKHFL" varchar(4) NOT NULL default '0000',
  "AAAAKHFG" varchar(30) NOT NULL default ' ',
  "AAAAKHFH" varchar(30) NOT NULL default ' ',
  "AAAAKHFI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAALARQ","AAABGYRZ","AAAAKHFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHFM" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAALAOZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHFS" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAAATVFS" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDAZ","AAAATVFS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHGB" (
  "AAAATVFS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVFS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHGI" (
  "AAAAKHGN" varchar(30) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAKHGP" varchar(2) NOT NULL default '00',
  "AAAAKHGM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHGN","AAABLCPX","AAAAKHGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHGQ" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAKHHW" varchar(1) NOT NULL default ' ',
  "AAABLXVF" varchar(1) NOT NULL default ' ',
  "AAAAKHHY" varchar(1) NOT NULL default ' ',
  "AAAATVFS" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDAZ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHHB" (
  "AAAAKHSN" varchar(4) NOT NULL default ' ',
  "AAAAKHHJ" varchar(30) NOT NULL default ' ',
  "AAAAKHHK" varchar(10) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAALAOZ" varchar(1) NOT NULL default ' ',
  "AAAAKHHH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHSN","AAAAKHHJ","AAAAKHHK","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHHM" (
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAAALARQ" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYRZ","AAAALARQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHHS" (
  "AAAALARQ" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAKHHW" varchar(1) NOT NULL default ' ',
  "AAABLXVF" varchar(1) NOT NULL default ' ',
  "AAAAKHHY" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAALARQ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHIC" (
  "AAABGYRZ" varchar(4) NOT NULL default ' ',
  "AAAALARQ" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHIG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYRZ","AAAALARQ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHIK" (
  "AAAAKHIR" varchar(30) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAAAKHIT" varchar(2) NOT NULL default '00',
  "AAAAKHIO" varchar(30) NOT NULL default ' ',
  "AAAAKHIP" varchar(30) NOT NULL default ' ',
  "AAAALAOZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHIR","AAABLCPX","AAAAKHIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHIU" (
  "AAAATVFS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVFS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHJB" (
  "AAAATVFS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKHJE" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVFS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAAAKHJN" varchar(2) NOT NULL default ' ',
  "AAAAKHJO" varchar(2) NOT NULL default ' ',
  "AAAAKHJP" varchar(8) NOT NULL default ' ',
  "AAAAKHJQ" varchar(1) NOT NULL default ' ',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAAAKHJS" varchar(8) NOT NULL default '00000000',
  "AAAAKHJT" varchar(8) NOT NULL default '00000000',
  "AAAAKHJU" varchar(8) NOT NULL default '00000000',
  "AAAAKHJV" varchar(8) NOT NULL default '00000000',
  "AAAAKHJW" varchar(8) NOT NULL default '00000000',
  "AAAAKHJX" varchar(8) NOT NULL default '00000000',
  "AAAAKHJY" varchar(8) NOT NULL default '00000000',
  "AAAAKHJZ" varchar(8) NOT NULL default '00000000',
  "AAAAKHKA" varchar(1) NOT NULL default ' ',
  "AAAAKHKB" varchar(1) NOT NULL default ' ',
  "AAAAKHKC" varchar(1) NOT NULL default ' ',
  "AAAAKHKD" varchar(1) NOT NULL default ' ',
  "AAAAKHKE" varchar(1) NOT NULL default ' ',
  "AAAAKHKF" varchar(1) NOT NULL default ' ',
  "AAAAKHKG" varchar(1) NOT NULL default ' ',
  "AAAAKHKH" varchar(1) NOT NULL default ' ',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAAAKNTC" varchar(10) NOT NULL default ' ',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKHKW" varchar(8) NOT NULL default '00000000',
  "AAAAKHKX" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKHLE" decimal(15,6) NOT NULL default '0.000000',
  "AAAAKHLF" decimal(9,5) NOT NULL default '0.00000',
  "AAAAKHLG" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHLH" varchar(1) NOT NULL default ' ',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKHLJ" varchar(1) NOT NULL default ' ',
  "AAAAKHLK" varchar(1) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAAAKHLM" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHLN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHLO" decimal(9,5) NOT NULL default '0.00000',
  "AAAAKHLP" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHLQ" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICI" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICL" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHLT" varchar(1) NOT NULL default ' ',
  "AAAAKHLU" varchar(8) NOT NULL default '00000000',
  "AAAAKHLV" varchar(8) NOT NULL default '00000000',
  "AAAAKHLW" varchar(8) NOT NULL default '00000000',
  "AAAAKICH" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICP" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICS" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICT" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAABLLJG" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKVPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIRQ" varchar(2) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAPWOE" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(4) NOT NULL default ' ',
  "AAABMNNA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIRQ","AAABMNML","AAAAPWOE","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIRQ" varchar(2) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAPWOE" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(10) NOT NULL default ' ',
  "AAABMNNA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIRQ","AAABMNML","AAAAPWOE","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALAPT" varchar(8) NOT NULL default ' ',
  "AAAALARQ" varchar(2) NOT NULL default ' ',
  "AAAAKHNN" varchar(1) NOT NULL default ' ',
  "AAAAKHNO" varchar(4) NOT NULL default ' ',
  "AAAAKHNP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAALAPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAALAPT" varchar(8) NOT NULL default ' ',
  "AAAALAQF" varchar(30) NOT NULL default ' ',
  "AAAALARH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAALAPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKHOK" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKHOG" varchar(5) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAKHOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKHOO" varchar(5) NOT NULL default ' ',
  "AAAAKHOP" varchar(6) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAAAKHOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKVPA" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default ' ',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABKVPB" decimal(10,7) NOT NULL default '0.0000000',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKVPA","AAABMNZR","AAABLTIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAKOAH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAAAKOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHPV" varchar(40) NOT NULL default ' ',
  "AAAAKOAH" varchar(2) NOT NULL default ' ',
  "AAAAKHPS" varchar(2) NOT NULL default '00',
  "AAAAKHPT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAKHPV","AAAAKOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHPX" (
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAAAKHPZ" varchar(30) NOT NULL default ' ',
  "AAAAKHQA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHQC" (
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABDQCZ" varchar(10) NOT NULL default ' ',
  "AAAAKHQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMCU","AAABDQCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAAAKHQM" varchar(4) NOT NULL default ' ',
  "AAAAKNVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHSI" varchar(10) NOT NULL default ' ',
  "AAAAKHQY" varchar(10) NOT NULL default ' ',
  "AAAAKHQV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHSI","AAAAKHQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHSI" varchar(10) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHSI","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHSI" varchar(10) NOT NULL default ' ',
  "AAAAKHVP" varchar(10) NOT NULL default ' ',
  "AAAAKHVL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHSI","AAAAKHVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHSI" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAAAKNRD" varchar(16) NOT NULL default ' ',
  "AAAAKHRW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHSI"),
  KEY "AAAAKHRZ" ("AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKHSI" varchar(10) NOT NULL default ' ',
  "AAAAKHSF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKHSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHSJ" (
  "AAAAKHSN" varchar(4) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHSN","AAABMMCU","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHSX" varchar(8) NOT NULL default ' ',
  "AAAAKHTC" varchar(30) NOT NULL default ' ',
  "AAAAKHTB" varchar(1) NOT NULL default ' ',
  "AAAAKHSV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHSX","AAAAKHTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHSZ" (
  "AAAAKHTC" varchar(30) NOT NULL default ' ',
  "AAAAKHTB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKHTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHTN" varchar(8) NOT NULL default '00000000',
  "AAAAKHTH" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHTI" decimal(17,2) NOT NULL default '0.00',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAABLDAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAABLDAF" varchar(8) NOT NULL default '00000000',
  "AAABMDYP" varchar(8) NOT NULL default '00000000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMDYS" varchar(2) NOT NULL default '00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDZH" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMDZB" varchar(8) NOT NULL default '00000000',
  "AAAAKHUM" decimal(17,2) NOT NULL default '0.00',
  "AAAAKHUN" varchar(1) NOT NULL default ' ',
  "AAAAKOAQ" smallint(6) NOT NULL default '0',
  "AAAAKOAR" varchar(1) NOT NULL default ' ',
  "AAAAKOAN" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAABLDAF","AAABMDYP","AAABLDBT","AAABMDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHVO" varchar(8) NOT NULL default '00000000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHVO" varchar(8) NOT NULL default '00000000',
  "AAAAKHVP" varchar(10) NOT NULL default ' ',
  "AAAAKHVL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHVO","AAAAKHVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHZD" varchar(8) NOT NULL default '00000000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHZD" varchar(8) NOT NULL default '00000000',
  "AAAAKHZE" varchar(3) NOT NULL default '000',
  "AAABDIKT" varchar(1) NOT NULL default ' ',
  "AAAAKHWF" varchar(2) NOT NULL default '00',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAAAVSVS" varchar(2) NOT NULL default '00',
  "AAAAKHWI" varchar(1) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAAAKHWM" varchar(10) NOT NULL default ' ',
  "AAAAKHWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHZD","AAAAKHZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHZD" varchar(8) NOT NULL default '00000000',
  "AAAAKHZE" varchar(3) NOT NULL default '000',
  "AAAAKHXF" varchar(30) NOT NULL default ' ',
  "AAAAKHWY" varchar(24) NOT NULL default ' ',
  "AAAAKHWZ" varchar(24) NOT NULL default ' ',
  "AAAAKHXA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHZD","AAAAKHZE","AAAAKHXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAAAYVAO" varchar(40) NOT NULL default ' ',
  "AAAAKHYO" varchar(40) NOT NULL default ' ',
  "AAAAXTPR" varchar(8) NOT NULL default '00000000',
  "AAAAXTPO" varchar(8) NOT NULL default '00000000',
  "AAAAKHXO" varchar(5) NOT NULL default ' ',
  "AAABBWRC" varchar(10) NOT NULL default ' ',
  "AAAAKNZO" varchar(6) NOT NULL default ' ',
  "AAAAKHXR" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO"),
  UNIQUE KEY "AAAAKHYD" ("AAAAYVAO","AAABFJKF","AAABMRIL"),
  UNIQUE KEY "AAAAKHYH" ("AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHYO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKIAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKHZD" varchar(8) NOT NULL default '00000000',
  "AAAAKHZE" varchar(3) NOT NULL default '000',
  "AAAAKHZF" varchar(30) NOT NULL default ' ',
  "AAAAKHYY" varchar(18) NOT NULL default ' ',
  "AAAAKHYZ" varchar(18) NOT NULL default ' ',
  "AAAAKHZA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKHZD","AAAAKHZE","AAAAKHZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKHZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHZS" varchar(16) NOT NULL default ' ',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAAAKHZU" varchar(16) NOT NULL default ' ',
  "AAABHIIX" varchar(8) NOT NULL default '00000000',
  "AAABESXU" varchar(8) NOT NULL default '00000000',
  "AAAAKHZO" decimal(17,2) NOT NULL default '0.00',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKHZS"),
  UNIQUE KEY "AAAAKHZT" ("AAAAKHZU","AAABFJKF","AAABMRIL"),
  KEY "AAAAKHZX" ("AAAAKIAO","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAAAKIAH" varchar(8) NOT NULL default '00000000',
  "AAAALCKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO","AAAAKIAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIAO" varchar(16) NOT NULL default ' ',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAAAYVAO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIAO"),
  UNIQUE KEY "AAAAKIAP" ("AAAAYVAO","AAABFJKF","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAABKKVT" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIEM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIES" varchar(8) NOT NULL default '00000000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAAAKIDK" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIDN" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIES","AAABFJEP","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIES" varchar(8) NOT NULL default '00000000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKIBU" varchar(8) NOT NULL default '00000000',
  "AAAAKIBV" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKIBW" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKIBX" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKIBY" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKIBZ" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKICA" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKICB" decimal(10,5) NOT NULL default '0.00000',
  "AAAAKICC" smallint(6) NOT NULL default '0',
  "AAAAKICD" smallint(6) NOT NULL default '0',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKICF" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICG" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICH" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICI" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICJ" varchar(1) NOT NULL default ' ',
  "AAAAKICK" varchar(1) NOT NULL default ' ',
  "AAAAKICL" decimal(17,2) NOT NULL default '0.00',
  "AAABLCSG" varchar(8) NOT NULL default '00000000',
  "AAAAKICN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICO" varchar(1) NOT NULL default ' ',
  "AAAAKICP" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICQ" varchar(6) NOT NULL default '000000',
  "AAAAKICR" varchar(8) NOT NULL default '00000000',
  "AAAAKICS" decimal(17,2) NOT NULL default '0.00',
  "AAAAKICT" decimal(17,2) NOT NULL default '0.00',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIES","AAABFJEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIES" varchar(8) NOT NULL default '00000000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKNUL" varchar(4) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAAAKIEM" varchar(5) NOT NULL default ' ',
  "AAAAKIEN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKOBB" varchar(1) NOT NULL default ' ',
  "AAAAKIDK" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIDL" varchar(1) NOT NULL default ' ',
  "AAAAKIDM" varchar(8) NOT NULL default '00000000',
  "AAAAKIDN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIDO" varchar(8) NOT NULL default '00000000',
  "AAAAKIDP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIES","AAABFJEP","AAAAKNUL","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAAAKIEA" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIEM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKIES" varchar(8) NOT NULL default '00000000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAAAKNUL" varchar(4) NOT NULL default ' ',
  "AAAAKIEV" varchar(8) NOT NULL default '00000000',
  "AAAAKIEM" varchar(5) NOT NULL default ' ',
  "AAAAKIEN" decimal(17,2) NOT NULL default '0.00',
  "AAAAKOBB" varchar(1) NOT NULL default ' ',
  "AAAAKIEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKIES","AAABFJEP","AAAAKNUL","AAAAKIEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABIFJS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAKIFI" varchar(1) NOT NULL default ' ',
  "AAAAKIFJ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABKRXO" varchar(7) NOT NULL default '0000000',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAAAKIGB" varchar(1) NOT NULL default ' ',
  "AAAAKIGC" varchar(4) NOT NULL default ' ',
  "AAAAKIFV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCWT","AAABKRXO","AAABHNOA","AAAAKIGB","AAAAKIGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIGV" varchar(5) NOT NULL default ' ',
  "AAAAKIGG" varchar(1) NOT NULL default ' ',
  "AAAAKIGH" varchar(1) NOT NULL default ' ',
  "AAAAKIGI" varchar(1) NOT NULL default ' ',
  "AAAAKIGJ" varchar(1) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAAAKIGL" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKIGV" varchar(5) NOT NULL default ' ',
  "AAAAKIGS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKIGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKIHI" varchar(5) NOT NULL default ' ',
  "AAAAKIHF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKIHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAABLAWQ" varchar(12) NOT NULL default ' ',
  "AAAALEBN" varchar(12) NOT NULL default ' ',
  "AAAALEBO" varchar(8) NOT NULL default '00000000',
  "AAAALEBP" varchar(6) NOT NULL default '000000',
  "AAAALEBQ" varchar(12) NOT NULL default ' ',
  "AAAALEBR" varchar(8) NOT NULL default '00000000',
  "AAAALEBS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKUVA" varchar(10) NOT NULL default ' ',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAAALDQY" varchar(10) NOT NULL default ' ',
  "AAABLAQG" varchar(3) NOT NULL default ' ',
  "AAAALDIO" varchar(5) NOT NULL default ' ',
  "AAAAPWCX" varchar(10) NOT NULL default ' ',
  "AAABLETY" varchar(20) NOT NULL default ' ',
  "AAAALEBN" varchar(12) NOT NULL default ' ',
  "AAAALEBO" varchar(8) NOT NULL default '00000000',
  "AAAALEBP" varchar(6) NOT NULL default '000000',
  "AAAALEBQ" varchar(12) NOT NULL default ' ',
  "AAAALEBR" varchar(8) NOT NULL default '00000000',
  "AAAALEBS" varchar(6) NOT NULL default '000000',
  "AAAAKIJP" varchar(20) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAAAKIIO" varchar(1) NOT NULL default ' ',
  "AAAALDFR" varchar(1) NOT NULL default ' ',
  "AAAAKIIQ" varchar(8) NOT NULL default '00000000',
  "AAAAKIJY" varchar(8) NOT NULL default '00000000',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAAAKIIV" varchar(10) NOT NULL default ' ',
  "AAAALDFP" varchar(2) NOT NULL default '00',
  "AAAAKIIX" varchar(3) NOT NULL default ' ',
  "AAAALDJL" varchar(3) NOT NULL default ' ',
  "AAABKUVB" varchar(10) NOT NULL default ' ',
  "AAAAKIJA" varchar(5) NOT NULL default ' ',
  "AAAAKIJB" varchar(5) NOT NULL default ' ',
  "AAAAKIJC" varchar(8) NOT NULL default '00000000',
  "AAAAKIJD" varchar(8) NOT NULL default '00000000',
  "AAAALDFS" varchar(1) NOT NULL default ' ',
  "AAAAKIJF" varchar(12) NOT NULL default ' ',
  "AAAAKIJG" varchar(8) NOT NULL default '00000000',
  "AAAAKIJH" varchar(6) NOT NULL default '000000',
  "AAAAKIJI" varchar(12) NOT NULL default ' ',
  "AAAAKIJJ" varchar(8) NOT NULL default '00000000',
  "AAAAKIJK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAAAKIJN" ("AAABMRIL","AAAAKIJP"),
  KEY "AAAAKIJQ" ("AAABMRIL","AAAAPWCX"),
  KEY "AAAAKIJT" ("AAABMRIL","AAABLDBD"),
  KEY "AAAAKIJW" ("AAAALDFR","AAAAKIJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFVI" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFVI" varchar(16) NOT NULL default ' ',
  "AAAAKIKS" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFVI","AAAAKIKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKIMD" varchar(2) NOT NULL default ' ',
  "AAAAKIME" varchar(2) NOT NULL default ' ',
  "AAAAKIMF" varchar(2) NOT NULL default ' ',
  "AAAAKIMG" varchar(2) NOT NULL default ' ',
  "AAAAKIMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKILD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKIMD" varchar(2) NOT NULL default ' ',
  "AAAAKIME" varchar(2) NOT NULL default ' ',
  "AAAAKIMF" varchar(2) NOT NULL default ' ',
  "AAAAKIMG" varchar(2) NOT NULL default ' ',
  "AAAAKIMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMFT","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKILP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  "AAAAKIMD" varchar(2) NOT NULL default ' ',
  "AAAAKIME" varchar(2) NOT NULL default ' ',
  "AAAAKIMF" varchar(2) NOT NULL default ' ',
  "AAAAKIMG" varchar(2) NOT NULL default ' ',
  "AAAAKIMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKILZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  "AAAAKIMD" varchar(2) NOT NULL default ' ',
  "AAAAKIME" varchar(2) NOT NULL default ' ',
  "AAAAKIMF" varchar(2) NOT NULL default ' ',
  "AAAAKIMG" varchar(2) NOT NULL default ' ',
  "AAAAKIMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMFT","AAAAKMQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABBTUU" decimal(17,2) NOT NULL default '0.00',
  "AAAAKIQY" varchar(1) NOT NULL default ' ',
  "AAABKWVR" decimal(10,7) NOT NULL default '0.0000000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAUMZL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKINJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKOAH" varchar(2) NOT NULL default ' ',
  "AAAAKINN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKINR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKOAH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKINW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKLCH" varchar(3) NOT NULL default '000',
  "AAAAYBRY" varchar(5) NOT NULL default '00000',
  "AAAAKLCB" varchar(2) NOT NULL default ' ',
  "AAAAKLCC" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKMCP","AAAAKLCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKMCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAABHFLY" varchar(4) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAKMPQ" varchar(4) NOT NULL default ' ',
  "AAAAKMPR" varchar(4) NOT NULL default ' ',
  "AAAAKMPS" varchar(5) NOT NULL default ' ',
  "AAAAKMPT" varchar(1) NOT NULL default ' ',
  "AAABKYPU" varchar(1) NOT NULL default ' ',
  "AAABKYPV" varchar(20) NOT NULL default ' ',
  "AAABKYPS" int(11) NOT NULL default '0',
  "AAAAKMPX" varchar(1) NOT NULL default ' ',
  "AAAAKMZU" varchar(1) NOT NULL default ' ',
  "AAAAKMPZ" varchar(1) NOT NULL default ' ',
  "AAAAKMQA" varchar(1) NOT NULL default ' ',
  "AAAAKMQB" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKMQD" varchar(1) NOT NULL default ' ',
  "AAAAKMQE" varchar(1) NOT NULL default ' ',
  "AAAAKMQF" varchar(1) NOT NULL default ' ',
  "AAAAKMQG" varchar(1) NOT NULL default ' ',
  "AAAAKNGA" varchar(10) NOT NULL default ' ',
  "AAABKYVO" varchar(10) NOT NULL default ' ',
  "AAAAKMQJ" varchar(1) NOT NULL default ' ',
  "AAAAKMQK" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default ' ',
  "AAAAKMQM" varchar(1) NOT NULL default ' ',
  "AAABFJGM" varchar(8) NOT NULL default '00000000',
  "AAAAKMQO" varchar(1) NOT NULL default ' ',
  "AAAAKMZV" varchar(1) NOT NULL default ' ',
  "AAAAKMZW" varchar(1) NOT NULL default ' ',
  "AAAAKMQR" varchar(4) NOT NULL default ' ',
  "AAAAKMQS" varchar(4) NOT NULL default ' ',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAAQTEC" varchar(8) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAKIQY" varchar(1) NOT NULL default ' ',
  "AAABBTUU" decimal(17,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAUMZL" varchar(8) NOT NULL default '00000000',
  "AAAAKIQS" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAAAQTEC","AAABLPQY","AAAAKIQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIQZ" (
  "AAAAKIRJ" varchar(2) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABILOU" varchar(1) NOT NULL default ' ',
  "AAABMMYV" varchar(1) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAAAKIRF" varchar(8) NOT NULL default ' ',
  "AAAAKIRG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAAAKIRI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKIRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIRQ" varchar(2) NOT NULL default ' ',
  "AAAAKIRR" varchar(4) NOT NULL default ' ',
  "AAAAKIRO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIRQ","AAAAKIRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAAAQTAC" varchar(6) NOT NULL default '000000',
  "AAAAKJYR" varchar(5) NOT NULL default '00000',
  "AAAAKIRY" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX","AAAAKNFI","AAAAQTAC","AAAAKJYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKISE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAAAQTAC" varchar(6) NOT NULL default '000000',
  "AAAAKJYR" varchar(5) NOT NULL default '00000',
  "AAAAKISK" double NOT NULL default '0',
  "AAAAKISL" double NOT NULL default '0',
  "AAAAKISM" double NOT NULL default '0',
  "AAAAKISN" double NOT NULL default '0',
  "AAAAKISO" double NOT NULL default '0',
  "AAAAKISP" double NOT NULL default '0',
  "AAAAKISQ" double NOT NULL default '0',
  "AAAAKISR" double NOT NULL default '0',
  "AAAAKISS" double NOT NULL default '0',
  "AAAAKIST" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX","AAAAKNFI","AAAAQTAC","AAAAKJYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKISZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAKJLL" varchar(8) NOT NULL default '00000000',
  "AAABHSKS" varchar(3) NOT NULL default ' ',
  "AAAAKLWC" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKJNK" varchar(3) NOT NULL default ' ',
  "AAABGAVR" varchar(5) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX"),
  KEY "AAAAKITO" ("AAABHSKS","AAAAKNLU","AAAAKNFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKITS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAAAKLDZ" varchar(20) NOT NULL default ' ',
  "AAAAKLEA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX","AAAAKNFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAKLDY" varchar(6) NOT NULL default '000000',
  "AAAAKLDZ" varchar(20) NOT NULL default ' ',
  "AAAAKLEA" int(11) NOT NULL default '0',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAAKLEC" varchar(3) NOT NULL default '000',
  "AAAAKLED" varchar(12) NOT NULL default ' ',
  "AAAAKLEE" varchar(8) NOT NULL default '00000000',
  "AAAAKLEF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAAKIVX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIVX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAAAQTAC" varchar(6) NOT NULL default '000000',
  "AAAAKJYR" varchar(5) NOT NULL default '00000',
  "AAABIWUT" double NOT NULL default '0',
  "AAAAKIVT" double NOT NULL default '0',
  "AAABKQPS" double NOT NULL default '0',
  "AAAAQBIC" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKIVX","AAAAKNFI","AAAAQTAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKIWC" varchar(1) NOT NULL default ' ',
  "AAAAKIWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKKNW" varchar(10) NOT NULL default ' ',
  "AAABKXPW" varchar(13) NOT NULL default ' ',
  "AAABLATT" varchar(10) NOT NULL default ' ',
  "AAABKYCB" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAKIWM" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAAAKKNW","AAABKXPW","AAABLATT","AAABKYCB","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYCB" varchar(3) NOT NULL default ' ',
  "AAAAKIXK" smallint(6) NOT NULL default '0',
  "AAABGPGM" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKYCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYCB" varchar(3) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMPZ" varchar(1) NOT NULL default ' ',
  "AAAAKMQA" varchar(1) NOT NULL default ' ',
  "AAAAKMQB" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKMQD" varchar(1) NOT NULL default ' ',
  "AAAAKIZY" varchar(1) NOT NULL default ' ',
  "AAAAKMPR" varchar(4) NOT NULL default ' ',
  "AAAAKMQJ" varchar(1) NOT NULL default ' ',
  "AAAAKMQM" varchar(1) NOT NULL default ' ',
  "AAAAKMZU" varchar(1) NOT NULL default ' ',
  "AAAAKMZV" varchar(1) NOT NULL default ' ',
  "AAAAKMZW" varchar(1) NOT NULL default ' ',
  "AAAAKMZX" varchar(1) NOT NULL default ' ',
  "AAAAKMZY" varchar(1) NOT NULL default ' ',
  "AAABDVPS" varchar(1) NOT NULL default ' ',
  "AAAAKNAA" varchar(1) NOT NULL default ' ',
  "AAAAKNAB" varchar(32) NOT NULL default ' ',
  "AAAAKNAC" varchar(30) NOT NULL default ' ',
  "AAAAKNAD" varchar(1) NOT NULL default ' ',
  "AAAAKNAE" varchar(1) NOT NULL default ' ',
  "AAAAKNAF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMZV" varchar(1) NOT NULL default ' ',
  "AAAAKMZW" varchar(1) NOT NULL default ' ',
  "AAAAKMZX" varchar(1) NOT NULL default ' ',
  "AAAAKMZY" varchar(1) NOT NULL default ' ',
  "AAABDVPS" varchar(1) NOT NULL default ' ',
  "AAAAKNAA" varchar(1) NOT NULL default ' ',
  "AAAAKNAB" varchar(32) NOT NULL default ' ',
  "AAAAKNAC" varchar(30) NOT NULL default ' ',
  "AAAAKNAD" varchar(1) NOT NULL default ' ',
  "AAAAKNAE" varchar(1) NOT NULL default ' ',
  "AAAAKNAF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXH","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKIZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  "AAABKYUV" varchar(4) NOT NULL default ' ',
  "AAAAKMPZ" varchar(1) NOT NULL default ' ',
  "AAAAKMQA" varchar(1) NOT NULL default ' ',
  "AAAAKMQB" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAAAKMQD" varchar(1) NOT NULL default ' ',
  "AAAAKIZY" varchar(1) NOT NULL default ' ',
  "AAAAKMPR" varchar(4) NOT NULL default ' ',
  "AAAAKMQJ" varchar(1) NOT NULL default ' ',
  "AAAAKMQM" varchar(1) NOT NULL default ' ',
  "AAAAKMZU" varchar(1) NOT NULL default ' ',
  "AAAAKMZV" varchar(1) NOT NULL default ' ',
  "AAAAKMZW" varchar(1) NOT NULL default ' ',
  "AAAAKMZX" varchar(1) NOT NULL default ' ',
  "AAAAKMZY" varchar(1) NOT NULL default ' ',
  "AAABDVPS" varchar(1) NOT NULL default ' ',
  "AAAAKNAA" varchar(1) NOT NULL default ' ',
  "AAAAKNAB" varchar(32) NOT NULL default ' ',
  "AAAAKNAC" varchar(30) NOT NULL default ' ',
  "AAAAKNAD" varchar(1) NOT NULL default ' ',
  "AAAAKNAE" varchar(1) NOT NULL default ' ',
  "AAAAKNAF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKMQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAAAKMQT" varchar(4) NOT NULL default ' ',
  "AAAAKMZV" varchar(1) NOT NULL default ' ',
  "AAAAKMZW" varchar(1) NOT NULL default ' ',
  "AAAAKMZX" varchar(1) NOT NULL default ' ',
  "AAAAKMZY" varchar(1) NOT NULL default ' ',
  "AAABDVPS" varchar(1) NOT NULL default ' ',
  "AAAAKNAA" varchar(1) NOT NULL default ' ',
  "AAAAKNAB" varchar(32) NOT NULL default ' ',
  "AAAAKNAC" varchar(30) NOT NULL default ' ',
  "AAAAKNAD" varchar(1) NOT NULL default ' ',
  "AAAAKNAE" varchar(1) NOT NULL default ' ',
  "AAAAKNAF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXH","AAAAKMQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJCL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJCL" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(20) NOT NULL default ' ',
  "AAABMNNA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJCL","AAABMNMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKJCL" varchar(2) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKJCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKJCR" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKJGH" varchar(8) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJGA" decimal(19,2) NOT NULL default '0.00',
  "AAAAKJGB" decimal(19,2) NOT NULL default '0.00',
  "AAAAKJGC" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAABMNSG","AAAAKJGH","AAAAKMXO","AAAAKNJF","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAALBNC" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABLXTG","AAABHFJB","AAAALBNC","AAABMRFQ","AAABMNSG","AAABDDQG"),
  KEY "AAAAKJEK" ("AAABLXTG","AAAAKNLT","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAALBNC" varchar(1) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKJGH" varchar(8) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKJFC" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABLXTG","AAABHFJB","AAAALBNC","AAABMNSG","AAAAKJGH","AAAAKMXO","AAAAKNJF","AAABMRFQ"),
  KEY "AAAAKJFN" ("AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKJGH" varchar(8) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJGA" decimal(19,2) NOT NULL default '0.00',
  "AAAAKJGB" decimal(19,2) NOT NULL default '0.00',
  "AAAAKJGC" decimal(19,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAABMNSG","AAAAKJGH","AAAAKMXO","AAAAKNJF","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAAKJGQ" varchar(2) NOT NULL default ' ',
  "AAAAQTEC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKNLT"),
  KEY "AAAAKJGV" ("AAABHFJB","AAAAKNLT","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJGZ" (
  "AAAAKJHD" varchar(6) NOT NULL default ' ',
  "AAAAKJHE" tinyint(3) unsigned NOT NULL default '0',
  "AAAAKJHC" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAAKJHD","AAAAKJHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAALEGM" varchar(12) NOT NULL default ' ',
  "AAAALEGN" varchar(8) NOT NULL default '00000000',
  "AAAALEGO" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKMDI" varchar(3) NOT NULL default ' ',
  "AAABMRDP" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABLXTG","AAABHFJB","AAABMNSG","AAAAKMDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKNLT","AAABHFJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  "AAAAKLRZ" varchar(13) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKJKA" decimal(17,2) NOT NULL default '0.00',
  "AAABKKVT" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAKKQF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABLXTG","AAABHFJB","AAAAKMXO","AAAAKLRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPQ" varchar(4) NOT NULL default ' ',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKMGO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPQ","AAABKYXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  "AAAAKLRZ" varchar(13) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAAKJKA" decimal(17,2) NOT NULL default '0.00',
  "AAABKKVT" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAKKQF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAAAKMXO","AAAAKLRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAKNLT"),
  KEY "AAAAKJKR" ("AAAAKNLT","AAABHFJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAAAKJMY" varchar(1) NOT NULL default ' ',
  "AAAAKJMZ" varchar(1) NOT NULL default ' ',
  "AAABDIGU" varchar(1) NOT NULL default ' ',
  "AAAAKJLB" varchar(2) NOT NULL default '00',
  "AAABDJZK" varchar(3) NOT NULL default ' ',
  "AAAAQTAC" varchar(6) NOT NULL default '000000',
  "AAABAUXD" varchar(5) NOT NULL default '00000',
  "AAAAKJMN" smallint(6) NOT NULL default '0',
  "AAAAKJMO" varchar(2) NOT NULL default ' ',
  "AAAAKJMP" smallint(6) NOT NULL default '0',
  "AAABKWRI" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKJMR" varchar(2) NOT NULL default ' ',
  "AAAAKJMS" varchar(2) NOT NULL default '00',
  "AAAAKJLL" varchar(8) NOT NULL default '00000000',
  "AAAAKJMJ" varchar(3) NOT NULL default ' ',
  "AAAAKJMK" varchar(3) NOT NULL default ' ',
  "AAAAKJMT" decimal(10,0) NOT NULL default '0',
  "AAAAKJMU" varchar(2) NOT NULL default '00',
  "AAAAKJMV" varchar(5) NOT NULL default '00000',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAAAKMZT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKYXH","AAABESZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJNK" varchar(3) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABKYUV" varchar(4) NOT NULL default ' ',
  "AAAAKJMI" varchar(2) NOT NULL default '00',
  "AAAAKJMJ" varchar(3) NOT NULL default ' ',
  "AAAAKJMK" varchar(3) NOT NULL default ' ',
  "AAAAKJML" varchar(2) NOT NULL default '00',
  "AAABAUXD" varchar(5) NOT NULL default '00000',
  "AAAAKJMN" smallint(6) NOT NULL default '0',
  "AAAAKJMO" varchar(2) NOT NULL default ' ',
  "AAAAKJMP" smallint(6) NOT NULL default '0',
  "AAABKWRI" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKJMR" varchar(2) NOT NULL default ' ',
  "AAAAKJMS" varchar(2) NOT NULL default '00',
  "AAAAKJMT" decimal(10,0) NOT NULL default '0',
  "AAAAKJMU" varchar(2) NOT NULL default '00',
  "AAAAKJMV" varchar(5) NOT NULL default '00000',
  "AAABHIXN" varchar(32) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAAKJMY" varchar(1) NOT NULL default ' ',
  "AAAAKJMZ" varchar(1) NOT NULL default ' ',
  "AAABDIGU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKJNK" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKJNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAAAKNJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPQ" varchar(4) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAAAKJOL" varchar(4) NOT NULL default '0000',
  "AAAAKJOM" varchar(4) NOT NULL default '0000',
  "AAAAKJON" varchar(4) NOT NULL default '0000',
  "AAAAKMXZ" varchar(4) NOT NULL default ' ',
  "AAAAKMYA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPQ","AAABDDQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAABMRFQ","AAABMNSG","AAABDDQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQVX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKJPR" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAABMNSG","AAABDDQG","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJPZ" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFNTS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMKS" varchar(6) NOT NULL default '000000',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABKXPA" varchar(1) NOT NULL default '0',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(17,2) NOT NULL default '0.00',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" varchar(6) NOT NULL default '000000',
  "AAAAKMJJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABLCVG" varchar(1) NOT NULL default ' ',
  "AAAAKMJP" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMOCB" varchar(4) NOT NULL default ' ',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMEAL" varchar(1) NOT NULL default ' ',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKYNB" varchar(1) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMJX" varchar(1) NOT NULL default ' ',
  "AAABKXLO" decimal(13,7) NOT NULL default '0.0000000',
  "AAAAKMJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL","AAAAKMKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMKS" varchar(6) NOT NULL default '000000',
  "AAABMOCC" varchar(4) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL","AAAAKMKS","AAABMOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMVB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAABLOVJ" varchar(1) NOT NULL default ' ',
  "AAABESMD" varchar(20) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  "AAABLDCC" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKKIT" varchar(13) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAABLAWQ" varchar(12) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAALDQY" varchar(10) NOT NULL default ' ',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ"),
  KEY "AAAAKJTU" ("AAABLXTG","AAABMRIL"),
  KEY "AAAAKJTX" ("AAABLXTG","AAABLDCC","AAABMRIL"),
  KEY "AAAAKJUB" ("AAABESMD","AAABMRIL"),
  KEY "AAAAKJUE" ("AAABMRIL","AAAAKKIT","AAABLOVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKKKE" varchar(22) NOT NULL default ' ',
  "AAAAKKKF" varchar(13) NOT NULL default ' ',
  "AAAAKMWJ" varchar(4) NOT NULL default ' ',
  "AAABKYXH" varchar(8) NOT NULL default ' ',
  "AAABKYOZ" varchar(3) NOT NULL default '000',
  "AAABKYPA" varchar(3) NOT NULL default '000',
  "AAAAKMWN" varchar(1) NOT NULL default ' ',
  "AAAAKMWO" varchar(1) NOT NULL default ' ',
  "AAABMEFO" varchar(8) NOT NULL default '00000000',
  "AAABMEFP" varchar(8) NOT NULL default '00000000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKXEG" varchar(1) NOT NULL default ' ',
  "AAAAKMWT" varchar(5) NOT NULL default ' ',
  "AAABGTWE" decimal(17,2) NOT NULL default '0.00',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABKXGJ" varchar(5) NOT NULL default ' ',
  "AAABKXGK" varchar(5) NOT NULL default ' ',
  "AAAAKMWY" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMWZ" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMXA" decimal(17,2) NOT NULL default '0.00',
  "AAABLCRT" varchar(8) NOT NULL default '00000000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLATT" varchar(10) NOT NULL default ' ',
  "AAABKYTL" varchar(10) NOT NULL default ' ',
  "AAAAKMXF" decimal(15,6) NOT NULL default '0.000000',
  "AAAAKMXG" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMXH" varchar(5) NOT NULL default ' ',
  "AAAAKMXI" decimal(15,6) NOT NULL default '0.000000',
  "AAAAKMXJ" varchar(8) NOT NULL default '00000000',
  "AAAAKMXK" varchar(1) NOT NULL default ' ',
  "AAAAKMXL" varchar(5) NOT NULL default ' ',
  "AAAAKMXM" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMXN" varchar(20) NOT NULL default ' ',
  "AAAAKMXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL"),
  KEY "AAAAKJVX" ("AAABMRIL","AAAAKKKE","AAABKYXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKJWE" varchar(1) NOT NULL default ' ',
  "AAAAKJWF" varchar(1) NOT NULL default ' ',
  "AAAAKJWG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKNAP" varchar(8) NOT NULL default '00000000',
  "AAAAKNAQ" varchar(1) NOT NULL default ' ',
  "AAABKXPX" varchar(1) NOT NULL default ' ',
  "AAABKXQR" varchar(1) NOT NULL default ' ',
  "AAABLARY" varchar(1) NOT NULL default '0',
  "AAABKXPR" varchar(5) NOT NULL default ' ',
  "AAABKXPS" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNAW" decimal(13,7) NOT NULL default '0.0000000',
  "AAAAKNAX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNAY" decimal(11,6) NOT NULL default '0.000000',
  "AAAAKNAZ" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBA" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKNBB" decimal(17,2) NOT NULL default '0.00',
  "AAABKXQA" varchar(3) NOT NULL default '000',
  "AAAAKNBD" varchar(3) NOT NULL default '000',
  "AAAAKNBE" decimal(15,5) NOT NULL default '0.00000',
  "AAAAKNBF" varchar(5) NOT NULL default ' ',
  "AAAAKNBG" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBH" varchar(1) NOT NULL default ' ',
  "AAAAKNBI" varchar(8) NOT NULL default '00000000',
  "AAAAKNBJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBK" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBL" decimal(11,6) NOT NULL default '0.000000',
  "AAAAKNBM" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKNBN" decimal(13,9) NOT NULL default '0.000000000',
  "AAAAKNBO" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBP" varchar(5) NOT NULL default ' ',
  "AAAAKNBQ" varchar(8) NOT NULL default '00000000',
  "AAAAKNBR" varchar(1) NOT NULL default ' ',
  "AAAAKNBS" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBT" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBU" decimal(11,6) NOT NULL default '0.000000',
  "AAAAKNBV" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAKNBW" decimal(13,9) NOT NULL default '0.000000000',
  "AAAAKNBX" decimal(17,2) NOT NULL default '0.00',
  "AAAAKNBY" varchar(5) NOT NULL default ' ',
  "AAAAKNBZ" varchar(8) NOT NULL default '00000000',
  "AAAAKNCA" varchar(1) NOT NULL default ' ',
  "AAABKXEM" varchar(1) NOT NULL default '0',
  "AAAAKNCC" smallint(6) NOT NULL default '0',
  "AAAAKNCD" smallint(6) NOT NULL default '0',
  "AAAAKNCE" smallint(6) NOT NULL default '0',
  "AAAAKNCF" decimal(13,2) NOT NULL default '0.00',
  "AAAAKNCG" varchar(8) NOT NULL default '00000000',
  "AAAAKNCH" decimal(11,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJZO" varchar(20) NOT NULL default ' ',
  "AAAAKJYY" varchar(20) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKJYP" varchar(1) NOT NULL default ' ',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAAAKJYR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJZO"),
  KEY "AAAAKJYU" ("AAABLXTG","AAABMRIL"),
  KEY "AAAAKJYX" ("AAAAKJYY","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJZO" varchar(20) NOT NULL default ' ',
  "AAABIOCE" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKJZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJZO" varchar(20) NOT NULL default ' ',
  "AAAAKJZP" varchar(20) NOT NULL default ' ',
  "AAAAKJZM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJZO","AAAAKJZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKJZX" varchar(20) NOT NULL default ' ',
  "AAABLOVJ" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKMXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKJZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKJZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNFX" varchar(12) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFLY" varchar(4) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAAAKKAI" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAAAKKAK" varchar(1) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAABHFJM" varchar(4) NOT NULL default ' ',
  "AAAAKNHM" varchar(14) NOT NULL default '00000000000000',
  "AAAAKKAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKAR" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHFMC" varchar(1) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAKMXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAABHFJB","AAABLXTG"),
  KEY "AAAAKKBG" ("AAABLXTG","AAAAKNLT","AAABMRIL"),
  KEY "AAAAKKBK" ("AAAAKNLT","AAABHFJB"),
  KEY "AAAAKKBN" ("AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMPQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAKMPQ" varchar(4) NOT NULL default ' ',
  "AAABDXVI" varchar(30) NOT NULL default ' ',
  "AAABFCOF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAKMPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKKCH" varchar(8) NOT NULL default '00000000',
  "AAAAKKCI" varchar(8) NOT NULL default '00000000',
  "AAAAKKCJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAKKCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKMCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKLCH" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAKLCC" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAKMCP","AAAAKLCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKMCP" varchar(3) NOT NULL default ' ',
  "AAAAKLCH" varchar(3) NOT NULL default '000',
  "AAAAKLCV" varchar(4) NOT NULL default ' ',
  "AAABFNEH" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAKMCP","AAAAKLCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKDF" (
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAAAKNHM" varchar(14) NOT NULL default '00000000000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAAAKNIB" varchar(4) NOT NULL default ' ',
  "AAAAKNIC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKKNR" varchar(8) NOT NULL default '00000000',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKKNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMKS" varchar(6) NOT NULL default '000000',
  "AAAAKKNR" varchar(8) NOT NULL default '00000000',
  "AAABDDQG" varchar(3) NOT NULL default ' ',
  "AAABKXPA" varchar(1) NOT NULL default '0',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(17,2) NOT NULL default '0.00',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMEFM" varchar(8) NOT NULL default '00000000',
  "AAABMEFN" varchar(8) NOT NULL default '00000000',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMPTI" varchar(6) NOT NULL default '000000',
  "AAABMPTJ" varchar(6) NOT NULL default '000000',
  "AAAAKMJJ" decimal(15,5) NOT NULL default '0.00000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(17,2) NOT NULL default '0.00',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  "AAABLCVG" varchar(1) NOT NULL default ' ',
  "AAAAKMJP" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMOCB" varchar(4) NOT NULL default ' ',
  "AAABMEAI" varchar(8) NOT NULL default '00000000',
  "AAABMEAL" varchar(1) NOT NULL default ' ',
  "AAAAKMJT" decimal(10,7) NOT NULL default '0.0000000',
  "AAABKYNB" varchar(1) NOT NULL default ' ',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(17,2) NOT NULL default '0.00',
  "AAAAKMJX" varchar(1) NOT NULL default ' ',
  "AAABKXLO" decimal(13,7) NOT NULL default '0.0000000',
  "AAAAKMJZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL","AAAAKMKS","AAAAKKNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMKS" varchar(6) NOT NULL default '000000',
  "AAAAKKNR" varchar(8) NOT NULL default '00000000',
  "AAABMOCC" varchar(4) NOT NULL default ' ',
  "AAABMDZE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABMDYY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL","AAAAKMKS","AAAAKKNR","AAABMOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKNCL" varchar(6) NOT NULL default '000000',
  "AAAAKMVB" varchar(4) NOT NULL default '0000',
  "AAAAKKNR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKNCL","AAAAKMVB","AAAAKKNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAAAKNFI" varchar(6) NOT NULL default '000000',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAAAKNCX" varchar(1) NOT NULL default ' ',
  "AAABLTQD" varchar(1) NOT NULL default ' ',
  "AAABLTQE" varchar(1) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAAAKNDC" varchar(6) NOT NULL default '000000',
  "AAAAKNDD" int(11) NOT NULL default '0',
  "AAAAKNDE" varchar(1) NOT NULL default ' ',
  "AAAAKNDF" varchar(1) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFNYB" varchar(50) NOT NULL default ' ',
  "AAAAXNHY" varchar(1) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAAAKLFH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAAAKNLU","AAAAKNFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFNYB" varchar(50) NOT NULL default ' ',
  "AAAAXNHY" varchar(1) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAAAKNLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKNLT" varchar(3) NOT NULL default ' ',
  "AAAAKNLU" varchar(3) NOT NULL default '000',
  "AAABHFJB" varchar(12) NOT NULL default ' ',
  "AAABLTQD" varchar(30) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKNLT","AAAAKNLU","AAABHFJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAKKIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAAAKKNR" varchar(8) NOT NULL default '00000000',
  "AAABLOVJ" varchar(1) NOT NULL default ' ',
  "AAABESMD" varchar(20) NOT NULL default ' ',
  "AAABJQTW" varchar(30) NOT NULL default ' ',
  "AAABLDCC" varchar(8) NOT NULL default '00000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKKIT" varchar(13) NOT NULL default ' ',
  "AAABKXGH" varchar(10) NOT NULL default ' ',
  "AAABLAWQ" varchar(12) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAAALDQY" varchar(10) NOT NULL default ' ',
  "AAABKUUW" varchar(3) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABFMFT" varchar(8) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKYXQ","AAAAKKNR"),
  KEY "AAAAKKJP" ("AAABLXTG","AAABMRIL"),
  KEY "AAAAKKJS" ("AAABLXTG","AAABLDCC","AAABMRIL"),
  KEY "AAAAKKJW" ("AAABESMD","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
