# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl07.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAFJRQ" (
  "AAAAFJSD" varchar(8) NOT NULL default ' ',
  "AAAAFJSE" varchar(18) NOT NULL default ' ',
  "AAAAFJRT" varchar(18) NOT NULL default ' ',
  "AAAAFJRU" int(11) NOT NULL default '0',
  "AAAAFJRV" varchar(3) NOT NULL default ' ',
  "AAAAFJRW" varchar(3) NOT NULL default ' ',
  "AAAAFJRX" varchar(3) NOT NULL default ' ',
  "AAAAFJRY" varchar(3) NOT NULL default ' ',
  "AAAAFJRZ" varchar(26) NOT NULL default ' ',
  "AAAAFJSA" varchar(6) NOT NULL default '000000',
  "AAAAFJSB" varchar(8) NOT NULL default '00000000',
  "AAAAFJSC" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFJSD","AAAAFJSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJSF" (
  "AAAAFJTO" varchar(8) NOT NULL default '00000000',
  "AAAAFJTP" varchar(6) NOT NULL default '000000',
  "AAAAFJTQ" varchar(8) NOT NULL default ' ',
  "AAABKUHD" varchar(8) NOT NULL default ' ',
  "AAABCHWX" smallint(6) NOT NULL default '0',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAAAFJSN" varchar(8) NOT NULL default ' ',
  "AAAAFJSO" varchar(1) NOT NULL default ' ',
  "AAAAFJSP" int(11) NOT NULL default '0',
  "AAAAFJSQ" varchar(1) NOT NULL default ' ',
  "AAAAVRAG" varchar(1) NOT NULL default ' ',
  "AAAAFJSS" varchar(1) NOT NULL default ' ',
  "AAAAFJST" smallint(6) NOT NULL default '0',
  "AAAAFJSU" smallint(6) NOT NULL default '0',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAFJSW" smallint(6) NOT NULL default '0',
  "AAAAFJSX" int(11) NOT NULL default '0',
  "AAAAFJSY" smallint(6) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAFJTA" varchar(26) NOT NULL default ' ',
  "AAAAFJTB" varchar(1) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAFJTD" varchar(8) NOT NULL default ' ',
  "AAABIJTA" varchar(1) NOT NULL default ' ',
  "AAAAFNKU" int(11) NOT NULL default '0',
  "AAABLDBD" smallint(6) NOT NULL default '0',
  "AAABLJJO" varchar(26) NOT NULL default ' ',
  "AAABBHVC" int(11) NOT NULL default '0',
  "AAAAFJTJ" smallint(6) NOT NULL default '0',
  "AAAAFJTK" smallint(6) NOT NULL default '0',
  "AAAAFJTL" smallint(6) NOT NULL default '0',
  "AAAAFJTM" int(11) NOT NULL default '0',
  "AAABKNCK" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFJTO","AAAAFJTP","AAAAFJTQ","AAABKUHD","AAABCHWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJTT" (
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAFJTY" varchar(40) NOT NULL default ' ',
  "AAAAFJTW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUZ","AAAAFJTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJTZ" (
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRFQ","AAABMLES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJUG" (
  "AAAAFKIU" varchar(10) NOT NULL default ' ',
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFJVC" varchar(18) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAAAFNLB" decimal(11,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAVBWM" varchar(6) NOT NULL default ' ',
  "AAAAFNLI" varchar(30) NOT NULL default ' ',
  "AAAAFJVN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKIU","AAABINMV","AAABLVJK","AAAAFJVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJUZ" (
  "AAAAFKIU" varchar(10) NOT NULL default ' ',
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAAAFJVC" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAAAFNLB" decimal(11,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAFNPB" varchar(6) NOT NULL default ' ',
  "AAAAFNLI" varchar(30) NOT NULL default ' ',
  "AAAAFJVN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKIU","AAABINMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJVQ" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAAAVBTH" varchar(6) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAFKIT" varchar(32) NOT NULL default ' ',
  "AAAAFKIU" varchar(10) NOT NULL default ' ',
  "AAAAFKIV" int(11) NOT NULL default '0',
  "AAAAUPXH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJWA" (
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABBKAF" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHVC","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJWG" (
  "AAAAFKIU" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAABGMNB" varchar(30) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAABIGOG" decimal(11,0) NOT NULL default '0',
  "AAAAFNOK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOM" decimal(15,0) NOT NULL default '0',
  "AAAAFNON" decimal(15,0) NOT NULL default '0',
  "AAAAFNOO" decimal(15,0) NOT NULL default '0',
  "AAAAFNOP" decimal(15,0) NOT NULL default '0',
  "AAAAFNOQ" decimal(15,0) NOT NULL default '0',
  "AAAAFNOR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOS" decimal(15,0) NOT NULL default '0',
  "AAAAFNOT" decimal(15,0) NOT NULL default '0',
  "AAAAFNOU" decimal(15,0) NOT NULL default '0',
  "AAAAFNOV" decimal(15,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAVBWM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKIU","AAABMREL","AAABLVJK"),
  KEY "AAAAFJXI" ("AAABLVJK","AAAAFNTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJXL" (
  "AAAAFKIU" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGMNB" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAABIGOG" decimal(11,0) NOT NULL default '0',
  "AAAAFNOK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOM" decimal(15,0) NOT NULL default '0',
  "AAAAFNON" decimal(15,0) NOT NULL default '0',
  "AAAAFNOO" decimal(15,0) NOT NULL default '0',
  "AAAAFNOP" decimal(15,0) NOT NULL default '0',
  "AAAAFNOQ" decimal(15,0) NOT NULL default '0',
  "AAAAFNOR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOS" decimal(15,0) NOT NULL default '0',
  "AAAAFNOT" decimal(15,0) NOT NULL default '0',
  "AAAAFNOU" decimal(15,0) NOT NULL default '0',
  "AAAAFNOV" decimal(15,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAFNPB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKIU","AAABMREL"),
  KEY "AAAAFJYM" ("AAAAFNTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJYO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(55) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJYX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABMDWW" varchar(12) NOT NULL default ' ',
  "AAABCTLF" int(11) NOT NULL default '0',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAABJDQV" varchar(1) NOT NULL default ' ',
  "AAABJDQY" varchar(46) NOT NULL default ' ',
  "AAABJDQZ" varchar(46) NOT NULL default ' ',
  "AAABLTUD" int(11) NOT NULL default '0',
  "AAABEZFO" varchar(12) NOT NULL default ' ',
  "AAABEZFP" varchar(1) NOT NULL default ' ',
  "AAABJDQW" varchar(1) NOT NULL default ' ',
  "AAABGGKN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFJZO" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLEEU" varchar(40) NOT NULL default ' ',
  "AAABMKFH" varchar(40) NOT NULL default ' ',
  "AAABLEEW" varchar(40) NOT NULL default ' ',
  "AAABLEEX" int(11) NOT NULL default '0',
  "AAABLEEF" varchar(3) NOT NULL default '000',
  "AAABLEEG" varchar(3) NOT NULL default '000',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABLEEI" varchar(100) NOT NULL default ' ',
  "AAABLEEJ" varchar(4) NOT NULL default ' ',
  "AAABLEEK" varchar(3) NOT NULL default ' ',
  "AAABLMNC" varchar(5) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLQHQ" varchar(100) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLEEP" varchar(1) NOT NULL default ' ',
  "AAABLEEQ" varchar(1) NOT NULL default ' ',
  "AAABLEER" varchar(14) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABLEET" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABLEEU","AAABMKFH","AAABLEEW","AAABLEEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKAO" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABMKFH" varchar(40) NOT NULL default ' ',
  "AAABLEEW" varchar(40) NOT NULL default ' ',
  "AAABLEEX" int(11) NOT NULL default '0',
  "AAAAFKBF" varchar(14) NOT NULL default ' ',
  "AAABHWYC" varchar(100) NOT NULL default ' ',
  "AAABLEEJ" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(5) NOT NULL default ' ',
  "AAAAFKAY" varchar(255) NOT NULL default ' ',
  "AAAAFKAZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH","AAABLEJK","AAABMKFH","AAABLEEW","AAABLEEX","AAAAFKBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKBG" (
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAAAFKCD" varchar(8) NOT NULL default ' ',
  "AAAAVNMO" varchar(8) NOT NULL default ' ',
  "AAAAVRPE" int(11) NOT NULL default '0',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEER","AAABINIK","AAAAFKCD","AAAAVNMO","AAAAVRPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKBS" (
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAAAFKCD" varchar(8) NOT NULL default ' ',
  "AAAAVNMO" varchar(8) NOT NULL default ' ',
  "AAABLUFA" varchar(72) NOT NULL default ' ',
  "AAABHCWZ" varchar(70) NOT NULL default ' ',
  "AAABMKFH" varchar(255) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEER","AAABINIK","AAAAFKCD","AAAAVNMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKCF" (
  "AAABMREL" varchar(32) NOT NULL default ' ',
  "AAAAFKDI" varchar(8) NOT NULL default ' ',
  "AAAAFKDJ" varchar(6) NOT NULL default ' ',
  "AAABLJKC" int(11) NOT NULL default '0',
  "AAABCIVG" tinyint(3) unsigned NOT NULL default '0',
  "AAABLEJE" tinyint(3) unsigned NOT NULL default '0',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAAAFKDI","AAAAFKDJ","AAABLJKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKCR" (
  "AAABMREL" varchar(32) NOT NULL default ' ',
  "AAAAFKDI" varchar(8) NOT NULL default ' ',
  "AAAAFKDJ" varchar(6) NOT NULL default ' ',
  "AAAAFKCV" varchar(18) NOT NULL default ' ',
  "AAAAFKCW" varchar(18) NOT NULL default ' ',
  "AAAAFKCX" varchar(18) NOT NULL default ' ',
  "AAAAFKCY" int(11) NOT NULL default '0',
  "AAAAFKCZ" varchar(1) NOT NULL default ' ',
  "AAAAFKDA" varchar(1) NOT NULL default ' ',
  "AAAAFKDB" varchar(1) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABHJBM" varchar(32) NOT NULL default ' ',
  "AAABHJBN" varchar(8) NOT NULL default ' ',
  "AAAAFKDF" varchar(12) NOT NULL default ' ',
  "AAAAKXYJ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAAAFKDI","AAAAFKDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKDK" (
  "AAABKAHC" int(11) NOT NULL default '0',
  "AAABFVMJ" varchar(16) NOT NULL default ' ',
  "AAAAFKGQ" varchar(64) NOT NULL default ' ',
  "AAAAFKGR" int(11) NOT NULL default '0',
  "AAAAQYBC" varchar(40) NOT NULL default ' ',
  "AAABMKFU" varchar(15) NOT NULL default ' ',
  "AAAAFKGU" int(11) NOT NULL default '0',
  "AAAAFKGV" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(16) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAAATIPO" int(11) NOT NULL default '0',
  "AAABILXY" varchar(26) NOT NULL default ' ',
  "AAABILYA" varchar(26) NOT NULL default ' ',
  "AAAAFKDY" int(11) NOT NULL default '0',
  "AAAAFKDZ" int(11) NOT NULL default '0',
  "AAAAFKEA" int(11) NOT NULL default '0',
  "AAAAFKEB" int(11) NOT NULL default '0',
  "AAAAFKEC" int(11) NOT NULL default '0',
  "AAAAFKED" int(11) NOT NULL default '0',
  "AAAAVNMX" int(11) NOT NULL default '0',
  "AAAAFKEF" int(11) NOT NULL default '0',
  "AAAAFKEG" int(11) NOT NULL default '0',
  "AAAAFKEH" int(11) NOT NULL default '0',
  "AAAAFKEI" int(11) NOT NULL default '0',
  "AAAAFKEJ" int(11) NOT NULL default '0',
  "AAAAFKEK" int(11) NOT NULL default '0',
  "AAAAFKEL" int(11) NOT NULL default '0',
  "AAAAFKEM" int(11) NOT NULL default '0',
  "AAAAFKEN" int(11) NOT NULL default '0',
  "AAAAFKEO" int(11) NOT NULL default '0',
  "AAAAFKEP" int(11) NOT NULL default '0',
  "AAAAFKEQ" int(11) NOT NULL default '0',
  "AAAAFKER" int(11) NOT NULL default '0',
  "AAAAFKES" int(11) NOT NULL default '0',
  "AAAAFKET" int(11) NOT NULL default '0',
  "AAAAFKEU" int(11) NOT NULL default '0',
  "AAAAFKEV" int(11) NOT NULL default '0',
  "AAAAFKEW" int(11) NOT NULL default '0',
  "AAAAFKEX" int(11) NOT NULL default '0',
  "AAAAFKEY" int(11) NOT NULL default '0',
  "AAAAFKEZ" int(11) NOT NULL default '0',
  "AAAAFKFA" int(11) NOT NULL default '0',
  "AAAAFKFB" int(11) NOT NULL default '0',
  "AAAAFKFC" int(11) NOT NULL default '0',
  "AAAAFKFD" int(11) NOT NULL default '0',
  "AAAAFKFE" int(11) NOT NULL default '0',
  "AAAAFKFF" int(11) NOT NULL default '0',
  "AAAAFKFG" int(11) NOT NULL default '0',
  "AAAAFKFH" int(11) NOT NULL default '0',
  "AAAAFKFI" int(11) NOT NULL default '0',
  "AAAAFKFJ" int(11) NOT NULL default '0',
  "AAAAFKFK" int(11) NOT NULL default '0',
  "AAAAFKFL" int(11) NOT NULL default '0',
  "AAAAFKFM" int(11) NOT NULL default '0',
  "AAAAFKFN" int(11) NOT NULL default '0',
  "AAAAFKFO" int(11) NOT NULL default '0',
  "AAAAFKFP" int(11) NOT NULL default '0',
  "AAAAFKFQ" int(11) NOT NULL default '0',
  "AAAAFKFR" int(11) NOT NULL default '0',
  "AAAAFKFS" int(11) NOT NULL default '0',
  "AAAAFKFT" int(11) NOT NULL default '0',
  "AAAAFKFU" int(11) NOT NULL default '0',
  "AAAAFKFV" int(11) NOT NULL default '0',
  "AAAAFKFW" int(11) NOT NULL default '0',
  "AAAAFKFX" int(11) NOT NULL default '0',
  "AAAAFKFY" int(11) NOT NULL default '0',
  "AAAAFKFZ" int(11) NOT NULL default '0',
  "AAAAFKGA" int(11) NOT NULL default '0',
  "AAAAFKGB" int(11) NOT NULL default '0',
  "AAAAFKGC" int(11) NOT NULL default '0',
  "AAAAFKGD" int(11) NOT NULL default '0',
  "AAAAFKGE" int(11) NOT NULL default '0',
  "AAAAFKGF" int(11) NOT NULL default '0',
  "AAAAFKGG" int(11) NOT NULL default '0',
  "AAAAFKGH" int(11) NOT NULL default '0',
  "AAAAFKGI" int(11) NOT NULL default '0',
  "AAAAFKGJ" int(11) NOT NULL default '0',
  "AAAAFKGK" int(11) NOT NULL default '0',
  "AAAAFKGL" varchar(200) NOT NULL default ' ',
  "AAAAFMAX" smallint(6) NOT NULL default '0',
  "AAAAFMAY" text,
  PRIMARY KEY ("AAABKAHC","AAABFVMJ","AAAAFKGQ","AAAAFKGR","AAAAQYBC","AAABMKFU","AAAAFKGU","AAAAFKGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKGW" (
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  "AAAAFKHM" varchar(20) NOT NULL default ' ',
  "AAABMNMH" varchar(128) NOT NULL default ' ',
  "AAABHCDU" varchar(14) NOT NULL default ' ',
  "AAAAIMEF" varchar(14) NOT NULL default ' ',
  "AAAAFKHC" varchar(14) NOT NULL default ' ',
  "AAAAMTVN" decimal(22,0) NOT NULL default '0',
  "AAAAFKHE" decimal(22,0) NOT NULL default '0',
  "AAAAFKHF" decimal(22,0) NOT NULL default '0',
  "AAAAFKHG" decimal(22,0) NOT NULL default '0',
  "AAAAMTVM" decimal(22,0) NOT NULL default '0',
  "AAAAFKHI" decimal(22,0) NOT NULL default '0',
  "AAAAFKHJ" decimal(22,0) NOT NULL default '0',
  "AAAAFKHK" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABHWXD","AAAAFKHM","AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKHO" (
  "AAAAFNLX" varchar(20) NOT NULL default ' ',
  "AAAAFKID" varchar(18) NOT NULL default ' ',
  "AAAAFNPR" varchar(20) NOT NULL default ' ',
  "AAAAFMEH" varchar(18) NOT NULL default ' ',
  "AAAAFMDW" varchar(18) NOT NULL default ' ',
  "AAAAFNPM" varchar(10) NOT NULL default ' ',
  "AAAAFNLT" int(11) NOT NULL default '0',
  "AAAAFKHW" varchar(3) NOT NULL default ' ',
  "AAAAFKHX" varchar(3) NOT NULL default ' ',
  "AAAAFKHY" varchar(3) NOT NULL default ' ',
  "AAAAFMEB" varchar(3) NOT NULL default ' ',
  "AAAAFMEC" varchar(26) NOT NULL default ' ',
  "AAAAFKIB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFNLX","AAAAFKID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKIE" (
  "AAABLEJI" varchar(5) NOT NULL default ' ',
  "AAABLRTW" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLMLR" int(11) NOT NULL default '0',
  "AAABMREL" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABLRTW","AAABLEJK","AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKIO" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAAAVBTH" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAFKIT" varchar(32) NOT NULL default ' ',
  "AAAAFKIU" varchar(20) NOT NULL default ' ',
  "AAAAFKIV" int(11) NOT NULL default '0',
  "AAAAUPXH" int(11) NOT NULL default '0',
  "AAAAFKIX" varchar(30) NOT NULL default ' ',
  "AAABLOSZ" varchar(3) NOT NULL default ' ',
  "AAAAFKIZ" varchar(10) NOT NULL default ' ',
  "AAAAFKJA" varchar(4) NOT NULL default ' ',
  "AAAAFKJB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKJD" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFKJJ" varchar(30) NOT NULL default ' ',
  "AAAAFKJK" varchar(64) NOT NULL default ' ',
  "AAAAFKJL" varchar(64) NOT NULL default ' ',
  "AAAAFKJM" varchar(64) NOT NULL default ' ',
  "AAABLEFI" int(11) NOT NULL default '0',
  "AAABLEFJ" int(11) NOT NULL default '0',
  "AAABLEFK" varchar(10) NOT NULL default ' ',
  "AAABLEFL" int(11) NOT NULL default '0',
  "AAABLEFM" decimal(22,0) NOT NULL default '0',
  "AAABLEFN" decimal(22,0) NOT NULL default '0',
  "AAABLEFO" int(11) NOT NULL default '0',
  "AAABLEFP" varchar(25) NOT NULL default ' ',
  "AAABLEFQ" decimal(22,0) NOT NULL default '0',
  "AAABLEFR" decimal(22,0) NOT NULL default '0',
  "AAABLEFS" decimal(22,0) NOT NULL default '0',
  "AAABLEFT" int(11) NOT NULL default '0',
  "AAABLEFU" int(11) NOT NULL default '0',
  "AAABLEFV" decimal(22,0) NOT NULL default '0',
  "AAABLEFW" varchar(25) NOT NULL default ' ',
  "AAABLEFX" varchar(255) NOT NULL default ' ',
  "AAABLEFY" varchar(10) NOT NULL default ' ',
  "AAABLEFZ" decimal(22,0) NOT NULL default '0',
  "AAABLEGB" int(11) NOT NULL default '0',
  "AAABLEGC" int(11) NOT NULL default '0',
  "AAABLEGD" decimal(22,0) NOT NULL default '0',
  "AAABLEGE" decimal(22,0) NOT NULL default '0',
  "AAABLEGF" varchar(25) NOT NULL default ' ',
  "AAABLEGG" varchar(25) NOT NULL default ' ',
  "AAABLEGH" int(11) NOT NULL default '0',
  "AAABLEGI" int(11) NOT NULL default '0',
  "AAABLEGJ" int(11) NOT NULL default '0',
  "AAABLEGK" decimal(22,0) NOT NULL default '0',
  "AAABLEGL" int(11) NOT NULL default '0',
  "AAABLEGM" varchar(10) NOT NULL default ' ',
  "AAABLEGN" int(11) NOT NULL default '0',
  "AAABLEGO" int(11) NOT NULL default '0',
  "AAABLEGP" int(11) NOT NULL default '0',
  "AAABLEGQ" int(11) NOT NULL default '0',
  "AAABLEGR" int(11) NOT NULL default '0',
  "AAABLEGS" varchar(255) NOT NULL default ' ',
  "AAABLEGT" decimal(22,0) NOT NULL default '0',
  "AAABLEGU" int(11) NOT NULL default '0',
  "AAABLEGV" int(11) NOT NULL default '0',
  "AAABLEGW" int(11) NOT NULL default '0',
  "AAABLEGX" int(11) NOT NULL default '0',
  "AAABLEGY" decimal(22,0) NOT NULL default '0',
  "AAABLEGZ" int(11) NOT NULL default '0',
  "AAABLEHA" varchar(25) NOT NULL default ' ',
  "AAABLEHB" int(11) NOT NULL default '0',
  "AAABLEHC" decimal(22,0) NOT NULL default '0',
  "AAABLEHD" decimal(22,0) NOT NULL default '0',
  "AAABLEHE" int(11) NOT NULL default '0',
  "AAABLEHF" varchar(10) NOT NULL default ' ',
  "AAABLEHG" decimal(22,0) NOT NULL default '0',
  "AAABLEHH" varchar(25) NOT NULL default ' ',
  "AAABLEHI" int(11) NOT NULL default '0',
  "AAABLEHJ" varchar(17) NOT NULL default ' ',
  "AAABLEHK" int(11) NOT NULL default '0',
  "AAABLEHL" varchar(25) NOT NULL default ' ',
  "AAABLEHM" int(11) NOT NULL default '0',
  "AAABLEHN" varchar(25) NOT NULL default ' ',
  "AAABLEHO" varchar(12) NOT NULL default ' ',
  "AAABLEHP" varchar(255) NOT NULL default ' ',
  "AAABLEHQ" varchar(25) NOT NULL default ' ',
  "AAABLEHS" int(11) NOT NULL default '0',
  "AAABLEHT" int(11) NOT NULL default '0',
  "AAABLEHU" varchar(25) NOT NULL default ' ',
  "AAABLEHV" varchar(25) NOT NULL default ' ',
  "AAABLEHW" varchar(33) NOT NULL default ' ',
  "AAABLEHX" varchar(25) NOT NULL default ' ',
  "AAAAFKMB" varchar(10) NOT NULL default ' ',
  "AAAAFKMC" varchar(25) NOT NULL default ' ',
  "AAAAFKMD" varchar(25) NOT NULL default ' ',
  "AAAAFKME" varchar(10) NOT NULL default ' ',
  "AAAAFKMF" int(11) NOT NULL default '0',
  "AAAAFKMG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKMK" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFKMQ" int(11) NOT NULL default '0',
  "AAAAFKMR" decimal(22,0) NOT NULL default '0',
  "AAAAFKMS" decimal(22,0) NOT NULL default '0',
  "AAAAFKMT" int(11) NOT NULL default '0',
  "AAAAFKMU" decimal(22,0) NOT NULL default '0',
  "AAAAFKMV" varchar(10) NOT NULL default ' ',
  "AAAAFKMW" varchar(255) NOT NULL default ' ',
  "AAAAFKMX" varchar(255) NOT NULL default ' ',
  "AAAAFKMY" varchar(10) NOT NULL default ' ',
  "AAAAFKMZ" int(11) NOT NULL default '0',
  "AAAAFKNA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKNE" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFKNS" varchar(200) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFKNL" decimal(22,0) NOT NULL default '0',
  "AAAAFKNM" decimal(22,0) NOT NULL default '0',
  "AAAAFKNN" decimal(22,0) NOT NULL default '0',
  "AAAAFKNO" decimal(22,0) NOT NULL default '0',
  "AAAAFKNP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFKNS","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKNU" (
  "AAABLEJI" varchar(5) NOT NULL default ' ',
  "AAAAFKOD" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEJH" varchar(20) NOT NULL default ' ',
  "AAABLEJE" int(11) NOT NULL default '0',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLECQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAAAFKOD","AAABLEJK","AAABLEJH","AAABLEJE","AAABLEJF","AAABLECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKOJ" (
  "AAAAFKPH" varchar(14) NOT NULL default ' ',
  "AAAAFKPI" varchar(3) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAAAFKOO" varchar(14) NOT NULL default ' ',
  "AAAAFKOP" varchar(20) NOT NULL default ' ',
  "AAAAFKOQ" varchar(20) NOT NULL default ' ',
  "AAABLKIV" varchar(255) NOT NULL default ' ',
  "AAABIASL" varchar(30) NOT NULL default ' ',
  "AAAAFKOT" decimal(22,0) NOT NULL default '0',
  "AAAAFLZI" decimal(22,0) NOT NULL default '0',
  "AAAAFKOV" varchar(20) NOT NULL default ' ',
  "AAAAFKOW" varchar(20) NOT NULL default ' ',
  "AAAAFKOX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKPH","AAAAFKPI","AAABLEJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKPB" (
  "AAAAFKPH" varchar(14) NOT NULL default ' ',
  "AAAAFKPI" varchar(3) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFLZA" varchar(30) NOT NULL default ' ',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFKPH","AAAAFKPI","AAABLEJK","AAAAFLZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKPL" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFKPQ" int(11) NOT NULL default '0',
  "AAAAFKPR" int(11) NOT NULL default '0',
  "AAAAFKPS" decimal(22,0) NOT NULL default '0',
  "AAAAFKPT" decimal(22,0) NOT NULL default '0',
  "AAAATQWW" varchar(25) NOT NULL default ' ',
  "AAAAFKPV" decimal(22,0) NOT NULL default '0',
  "AAAAFKPW" double NOT NULL default '0',
  "AAAAFKPX" int(11) NOT NULL default '0',
  "AAAAFKPY" double NOT NULL default '0',
  "AAAAFKPZ" varchar(25) NOT NULL default ' ',
  "AAAAFKQA" int(11) NOT NULL default '0',
  "AAAAFKQB" varchar(31) NOT NULL default ' ',
  "AAAAFKQC" int(11) NOT NULL default '0',
  "AAAAFKQD" varchar(10) NOT NULL default ' ',
  "AAAAFKQE" varchar(10) NOT NULL default ' ',
  "AAAAFKQF" varchar(10) NOT NULL default ' ',
  "AAAAFKQG" varchar(10) NOT NULL default ' ',
  "AAAAFKQH" varchar(10) NOT NULL default ' ',
  "AAAAFKQI" varchar(10) NOT NULL default ' ',
  "AAAAFKQJ" varchar(255) NOT NULL default ' ',
  "AAAAFKQK" int(11) NOT NULL default '0',
  "AAAAFKQL" varchar(255) NOT NULL default ' ',
  "AAAAFKQM" varchar(25) NOT NULL default ' ',
  "AAABLEFX" varchar(255) NOT NULL default ' ',
  "AAAAFKQO" varchar(255) NOT NULL default ' ',
  "AAAAFKQP" varchar(10) NOT NULL default ' ',
  "AAABLEFY" varchar(10) NOT NULL default ' ',
  "AAAAFKQR" varchar(35) NOT NULL default ' ',
  "AAAAFKQS" varchar(10) NOT NULL default ' ',
  "AAAAFKQT" int(11) NOT NULL default '0',
  "AAAAFKQU" int(11) NOT NULL default '0',
  "AAAAFKQV" decimal(22,0) NOT NULL default '0',
  "AAAAFKQW" decimal(22,0) NOT NULL default '0',
  "AAAAFKQX" decimal(22,0) NOT NULL default '0',
  "AAAAFKQY" decimal(22,0) NOT NULL default '0',
  "AAAAFKQZ" varchar(48) NOT NULL default ' ',
  "AAABLEGF" varchar(10) NOT NULL default ' ',
  "AAAAFKRB" varchar(25) NOT NULL default ' ',
  "AAAAFKRC" varchar(5) NOT NULL default ' ',
  "AAAAFKRD" varchar(25) NOT NULL default ' ',
  "AAAAFKRE" varchar(25) NOT NULL default ' ',
  "AAAAFKRF" varchar(25) NOT NULL default ' ',
  "AAAAFKRG" varchar(25) NOT NULL default ' ',
  "AAAAFKRH" int(11) NOT NULL default '0',
  "AAAAFKRI" varchar(10) NOT NULL default ' ',
  "AAAAFKRJ" varchar(10) NOT NULL default ' ',
  "AAAAFKRK" varchar(10) NOT NULL default ' ',
  "AAAAFKRL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKRO" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFKRT" decimal(22,0) NOT NULL default '0',
  "AAAAFKRU" varchar(255) NOT NULL default ' ',
  "AAAAFKRV" varchar(25) NOT NULL default ' ',
  "AAAAFKRW" int(11) NOT NULL default '0',
  "AAAAFKRX" decimal(22,0) NOT NULL default '0',
  "AAAAFKRY" decimal(22,0) NOT NULL default '0',
  "AAAAFKRZ" int(11) NOT NULL default '0',
  "AAAAFKSA" decimal(22,0) NOT NULL default '0',
  "AAAAFKSB" decimal(22,0) NOT NULL default '0',
  "AAAAFKSC" decimal(22,0) NOT NULL default '0',
  "AAAAFKSD" int(11) NOT NULL default '0',
  "AAAAFKSE" decimal(22,0) NOT NULL default '0',
  "AAAAFKSF" int(11) NOT NULL default '0',
  "AAAAPPDS" varchar(8) NOT NULL default ' ',
  "AAAAFKSH" decimal(22,0) NOT NULL default '0',
  "AAAAFKSI" decimal(22,0) NOT NULL default '0',
  "AAAAFKSJ" int(11) NOT NULL default '0',
  "AAABMIRX" varchar(48) NOT NULL default ' ',
  "AAAAFKSL" decimal(22,0) NOT NULL default '0',
  "AAAAFKSM" decimal(22,0) NOT NULL default '0',
  "AAAAFKSN" decimal(22,0) NOT NULL default '0',
  "AAAAFKSO" int(11) NOT NULL default '0',
  "AAAAFKSP" varchar(255) NOT NULL default ' ',
  "AAAAFKSQ" int(11) NOT NULL default '0',
  "AAAAFKSR" decimal(22,0) NOT NULL default '0',
  "AAAAFKSS" varchar(8) NOT NULL default ' ',
  "AAAAFKST" decimal(22,0) NOT NULL default '0',
  "AAAAFKSU" decimal(22,0) NOT NULL default '0',
  "AAAAFKSV" varchar(255) NOT NULL default ' ',
  "AAAAFKSW" varchar(25) NOT NULL default ' ',
  "AAAAFKSX" int(11) NOT NULL default '0',
  "AAAAFKSY" varchar(14) NOT NULL default ' ',
  "AAAAFKSZ" varchar(16) NOT NULL default ' ',
  "AAAAFKTA" varchar(16) NOT NULL default ' ',
  "AAAAFKTB" varchar(16) NOT NULL default ' ',
  "AAAAFKTC" varchar(8) NOT NULL default ' ',
  "AAAAFKTD" varchar(19) NOT NULL default ' ',
  "AAAAFKTE" varchar(64) NOT NULL default ' ',
  "AAAAFKTF" varchar(25) NOT NULL default ' ',
  "AAAAFKTG" varchar(25) NOT NULL default ' ',
  "AAAAFKTH" int(11) NOT NULL default '0',
  "AAABGLSO" varchar(55) NOT NULL default ' ',
  "AAABLEHT" int(11) NOT NULL default '0',
  "AAAAFKTK" decimal(22,0) NOT NULL default '0',
  "AAAAFKTL" decimal(22,0) NOT NULL default '0',
  "AAAAFKTM" decimal(22,0) NOT NULL default '0',
  "AAAAFKTN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKTQ" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAABLEIG" varchar(255) NOT NULL default ' ',
  "AAABLEIH" int(11) NOT NULL default '0',
  "AAABLEII" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKUC" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFKVR" decimal(22,0) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLZA" varchar(30) NOT NULL default ' ',
  "AAAAFKYS" varchar(50) NOT NULL default ' ',
  "AAAARLVZ" varchar(50) NOT NULL default ' ',
  "AAAAFKUM" varchar(50) NOT NULL default ' ',
  "AAAAFKVL" decimal(22,0) NOT NULL default '0',
  "AAAAFKVM" decimal(22,0) NOT NULL default '0',
  "AAAAFNOB" decimal(22,0) NOT NULL default '0',
  "AAAAFKUQ" decimal(22,0) NOT NULL default '0',
  "AAAAFKUR" decimal(22,0) NOT NULL default '0',
  "AAABLEIY" decimal(22,0) NOT NULL default '0',
  "AAAAFKUT" decimal(22,0) NOT NULL default '0',
  "AAAAFKUU" decimal(22,0) NOT NULL default '0',
  "AAAAFKUV" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFKVR","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKVA" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFKVR" decimal(22,0) NOT NULL default '0',
  "AAAAVJXP" decimal(22,0) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLZI" decimal(22,0) NOT NULL default '0',
  "AAAATPHI" varchar(255) NOT NULL default ' ',
  "AAABLISW" varchar(50) NOT NULL default ' ',
  "AAAAFKVL" decimal(22,0) NOT NULL default '0',
  "AAAAFKVM" decimal(22,0) NOT NULL default '0',
  "AAAAFKVN" varchar(10) NOT NULL default ' ',
  "AAAAFKVO" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFKVR","AAAAVJXP","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKVU" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" varchar(4) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABHDKR" varchar(255) NOT NULL default ' ',
  "AAABHBVL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABMIRY","AAABJECS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKWF" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLXV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFLXZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNK" decimal(22,0) NOT NULL default '0',
  "AAAAFLMO" decimal(22,0) NOT NULL default '0',
  "AAAAFLRO" decimal(22,0) NOT NULL default '0',
  "AAAAFLMR" decimal(22,0) NOT NULL default '0',
  "AAAAFLMS" decimal(22,0) NOT NULL default '0',
  "AAAAFKWU" decimal(22,0) NOT NULL default '0',
  "AAAAFKWV" decimal(22,2) NOT NULL default '0.00',
  "AAAAFKWW" decimal(22,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLEJF","AAABLEJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKWZ" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(18) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABKIUE" varchar(8) NOT NULL default '00000000',
  "AAABLEIQ" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABMIRY","AAABKIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKXJ" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(18) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABLEIP" varchar(30) NOT NULL default ' ',
  "AAABLEIQ" decimal(22,0) NOT NULL default '0',
  "AAABLEIR" decimal(22,0) NOT NULL default '0',
  "AAABLEIS" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKXU" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABKIUE" varchar(8) NOT NULL default '00000000',
  "AAAAFKYL" decimal(22,0) NOT NULL default '0',
  "AAAAFKYN" decimal(22,0) NOT NULL default '0',
  "AAAAFKYP" decimal(22,0) NOT NULL default '0',
  "AAABLEIY" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABLEJK","AAABKIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKYH" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFKYL" decimal(22,0) NOT NULL default '0',
  "AAAAFKYM" decimal(22,0) NOT NULL default '0',
  "AAAAFKYN" decimal(22,0) NOT NULL default '0',
  "AAAAFKYO" decimal(22,0) NOT NULL default '0',
  "AAAAFKYP" decimal(22,0) NOT NULL default '0',
  "AAABLEIY" decimal(22,0) NOT NULL default '0',
  "AAAAFKYR" decimal(22,0) NOT NULL default '0',
  "AAAAFKYS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABLEJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKYW" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABKIUE" varchar(8) NOT NULL default '00000000',
  "AAAAFKZK" decimal(22,0) NOT NULL default '0',
  "AAAAFKZM" decimal(22,0) NOT NULL default '0',
  "AAAAFKZO" decimal(22,0) NOT NULL default '0',
  "AAAAFKZQ" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABKIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKZH" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAAAFKZK" decimal(22,0) NOT NULL default '0',
  "AAAAFKZL" decimal(22,0) NOT NULL default '0',
  "AAAAFKZM" decimal(22,0) NOT NULL default '0',
  "AAAAFKZN" decimal(22,0) NOT NULL default '0',
  "AAAAFKZO" decimal(22,0) NOT NULL default '0',
  "AAAAFKZP" decimal(22,0) NOT NULL default '0',
  "AAAAFKZQ" decimal(22,0) NOT NULL default '0',
  "AAAAFKZR" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEIT","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFKZU" (
  "AAABLEJI" varchar(5) NOT NULL default ' ',
  "AAABLRTW" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEQO" varchar(14) NOT NULL default ' ',
  "AAABLEJE" int(11) NOT NULL default '0',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJG" varchar(255) NOT NULL default ' ',
  "AAABLEJH" varchar(255) NOT NULL default ' ',
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABLRTW","AAABLEJK"),
  KEY "AAAAFLAH" ("AAABHWXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLAJ" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAABAIXH" varchar(50) NOT NULL default ' ',
  "AAAAFLRF" decimal(22,0) NOT NULL default '0',
  "AAAAFLRI" varchar(14) NOT NULL default ' ',
  "AAABHAUB" varchar(20) NOT NULL default ' ',
  "AAAAFLRD" varchar(15) NOT NULL default ' ',
  "AAABDSAH" varchar(32) NOT NULL default ' ',
  "AAABGARW" varchar(5) NOT NULL default ' ',
  "AAABIMZM" varchar(20) NOT NULL default ' ',
  "AAAAFLAY" varchar(20) NOT NULL default ' ',
  "AAAAFLAZ" varchar(20) NOT NULL default ' ',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAFLRN" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  "AAAAFLSA" decimal(22,0) NOT NULL default '0',
  "AAAAFLMO" decimal(22,0) NOT NULL default '0',
  "AAAAFLRO" decimal(22,0) NOT NULL default '0',
  "AAAAFLMS" decimal(22,0) NOT NULL default '0',
  "AAAAFLMT" decimal(22,0) NOT NULL default '0',
  "AAAAFLMR" decimal(22,0) NOT NULL default '0',
  "AAAAFLBK" decimal(22,0) NOT NULL default '0',
  "AAAAFLMW" decimal(22,0) NOT NULL default '0',
  "AAAAFLMX" decimal(22,0) NOT NULL default '0',
  "AAAAFLMY" decimal(22,0) NOT NULL default '0',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLXV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFLXZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYB" decimal(22,0) NOT NULL default '0',
  "AAAAFLYC" decimal(22,0) NOT NULL default '0',
  "AAAAFLNJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNK" decimal(22,0) NOT NULL default '0',
  "AAAAFLNL" decimal(22,0) NOT NULL default '0',
  "AAAAFLNM" decimal(22,0) NOT NULL default '0',
  "AAAAFLNN" decimal(22,0) NOT NULL default '0',
  "AAAAFLNO" decimal(22,0) NOT NULL default '0',
  "AAAAFLNP" decimal(22,0) NOT NULL default '0',
  "AAAAFLNQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAC" decimal(22,0) NOT NULL default '0',
  "AAAAFMAF" decimal(22,0) NOT NULL default '0',
  "AAAAFLNU" decimal(22,0) NOT NULL default '0',
  "AAAAFLNV" decimal(22,0) NOT NULL default '0',
  "AAAAFLNW" decimal(22,0) NOT NULL default '0',
  "AAAAFLNX" decimal(22,0) NOT NULL default '0',
  "AAAAFLNY" decimal(22,0) NOT NULL default '0',
  "AAAAFLNZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLOA" decimal(22,0) NOT NULL default '0',
  "AAAAFLOB" decimal(22,0) NOT NULL default '0',
  "AAAAFLCP" decimal(22,0) NOT NULL default '0',
  "AAAAFLCQ" varchar(50) NOT NULL default ' ',
  "AAAAFLCR" varchar(14) NOT NULL default ' ',
  "AAAAFLCS" varchar(14) NOT NULL default ' ',
  "AAAAFLCT" varchar(14) NOT NULL default ' ',
  "AAAAFLCU" decimal(22,0) NOT NULL default '0',
  "AAAAFLCV" decimal(22,0) NOT NULL default '0',
  "AAAAFLCW" decimal(22,0) NOT NULL default '0',
  "AAAAFLCX" decimal(22,0) NOT NULL default '0',
  "AAAAFLCY" varchar(14) NOT NULL default ' ',
  "AAAAFLCZ" varchar(14) NOT NULL default ' ',
  "AAAAFMCE" varchar(14) NOT NULL default ' ',
  "AAAAFLDB" decimal(22,0) NOT NULL default '0',
  "AAAAFLYO" decimal(22,0) NOT NULL default '0',
  "AAAAFLYP" decimal(22,0) NOT NULL default '0',
  "AAAAFLYQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYR" decimal(22,0) NOT NULL default '0',
  "AAAAFLDG" decimal(22,0) NOT NULL default '0',
  "AAAAFLDH" decimal(22,0) NOT NULL default '0',
  "AAAAFLDI" decimal(22,0) NOT NULL default '0',
  "AAAAFLOS" decimal(22,0) NOT NULL default '0',
  "AAAAFLDK" decimal(22,0) NOT NULL default '0',
  "AAAAFLDL" decimal(22,0) NOT NULL default '0',
  "AAAAFLDM" decimal(22,0) NOT NULL default '0',
  "AAAAFLDN" decimal(22,0) NOT NULL default '0',
  "AAAAFMAD" decimal(22,0) NOT NULL default '0',
  "AAAAFMBO" decimal(22,0) NOT NULL default '0',
  "AAAAFMBN" decimal(22,0) NOT NULL default '0',
  "AAAAFLYG" decimal(22,0) NOT NULL default '0',
  "AAAAFLYH" decimal(22,0) NOT NULL default '0',
  "AAAAFLYI" decimal(22,0) NOT NULL default '0',
  "AAAAFLYJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYK" decimal(22,0) NOT NULL default '0',
  "AAAAFLYL" decimal(22,0) NOT NULL default '0',
  "AAAAFLDX" decimal(22,0) NOT NULL default '0',
  "AAAAFLDY" decimal(22,0) NOT NULL default '0',
  "AAAAFLDZ" varchar(20) NOT NULL default ' ',
  "AAAAFLEA" varchar(32) NOT NULL default ' ',
  "AAAAFLEB" decimal(22,0) NOT NULL default '0',
  "AAAAFLEC" varchar(25) NOT NULL default ' ',
  "AAAAFLED" varchar(25) NOT NULL default ' ',
  "AAAAFLEE" int(11) NOT NULL default '0',
  "AAAAFLPN" decimal(22,0) NOT NULL default '0',
  "AAAAFLPO" decimal(22,0) NOT NULL default '0',
  "AAAAFLPP" decimal(22,0) NOT NULL default '0',
  "AAAAFLPQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLPR" decimal(22,0) NOT NULL default '0',
  "AAAAFLPS" decimal(22,0) NOT NULL default '0',
  "AAAAFLEL" decimal(22,0) NOT NULL default '0',
  "AAAAFLPV" decimal(22,0) NOT NULL default '0',
  "AAAAFLJP" decimal(22,0) NOT NULL default '0',
  "AAAAFLVS" decimal(22,0) NOT NULL default '0',
  "AAAAFLEP" decimal(22,0) NOT NULL default '0',
  "AAAAFLEQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLER" varchar(25) NOT NULL default ' ',
  "AAAAFLQB" decimal(22,0) NOT NULL default '0',
  "AAAAFLET" decimal(22,0) NOT NULL default '0',
  "AAAAFLQC" decimal(22,0) NOT NULL default '0',
  "AAAAFLQD" decimal(22,0) NOT NULL default '0',
  "AAAAFLSD" decimal(22,0) NOT NULL default '0',
  "AAAAFLEX" varchar(32) NOT NULL default ' ',
  "AAAAFLEY" varchar(32) NOT NULL default ' ',
  "AAAAFLEZ" int(11) NOT NULL default '0',
  "AAAAFLQE" decimal(22,0) NOT NULL default '0',
  "AAAAFLQF" decimal(22,0) NOT NULL default '0',
  "AAAAFLQG" decimal(22,0) NOT NULL default '0',
  "AAAAFLQH" decimal(22,0) NOT NULL default '0',
  "AAAAFLFE" decimal(22,0) NOT NULL default '0',
  "AAAAFLFF" decimal(22,0) NOT NULL default '0',
  "AAAAFLYS" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLFP" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLFX" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLGD" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLTO" varchar(1) NOT NULL default ' ',
  "AAAAFLTP" decimal(22,0) NOT NULL default '0',
  "AAAAFLTQ" varchar(5) NOT NULL default ' ',
  "AAAAFLTR" varchar(25) NOT NULL default ' ',
  "AAAAFLTS" varchar(14) NOT NULL default ' ',
  "AAABLTOK" varchar(20) NOT NULL default ' ',
  "AAAAFMBT" varchar(20) NOT NULL default ' ',
  "AAAAFLZA" varchar(20) NOT NULL default ' ',
  "AAAAFLTW" decimal(22,0) NOT NULL default '0',
  "AAAAFLTX" varchar(32) NOT NULL default ' ',
  "AAAAFLTY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLHB" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLHJ" int(11) NOT NULL default '0',
  "AAAAFLHK" int(11) NOT NULL default '0',
  "AAAAFLHL" int(11) NOT NULL default '0',
  "AAAAFLHM" decimal(22,0) NOT NULL default '0',
  "AAAAFLHN" decimal(22,0) NOT NULL default '0',
  "AAAAFLHO" decimal(22,0) NOT NULL default '0',
  "AAAAFLHP" decimal(22,0) NOT NULL default '0',
  "AAAAFLHQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLHR" int(11) NOT NULL default '0',
  "AAAAFLHS" int(11) NOT NULL default '0',
  "AAAAFLHT" int(11) NOT NULL default '0',
  "AAAAFLHU" decimal(22,0) NOT NULL default '0',
  "AAAAFLHV" decimal(22,0) NOT NULL default '0',
  "AAAAFLHW" decimal(22,0) NOT NULL default '0',
  "AAAAFLHX" decimal(22,0) NOT NULL default '0',
  "AAAAFLHY" decimal(22,0) NOT NULL default '0',
  "AAAAFMBO" decimal(22,0) NOT NULL default '0',
  "AAAAFMBN" decimal(22,0) NOT NULL default '0',
  "AAAAFLJP" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLIH" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLIP" int(11) NOT NULL default '0',
  "AAAAFLIQ" varchar(25) NOT NULL default ' ',
  "AAAAFLIR" varchar(50) NOT NULL default ' ',
  "AAAAFLIS" decimal(22,0) NOT NULL default '0',
  "AAAAFLIT" decimal(22,0) NOT NULL default '0',
  "AAAAFLIU" decimal(22,0) NOT NULL default '0',
  "AAAAFLIV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAH" decimal(22,0) NOT NULL default '0',
  "AAAAFLMX" decimal(22,0) NOT NULL default '0',
  "AAAAFLMY" decimal(22,0) NOT NULL default '0',
  "AAAAFMBO" decimal(22,0) NOT NULL default '0',
  "AAAAFMBN" decimal(22,0) NOT NULL default '0',
  "AAAAFMAC" decimal(22,0) NOT NULL default '0',
  "AAAAFMAF" decimal(22,0) NOT NULL default '0',
  "AAAAFMAD" decimal(22,0) NOT NULL default '0',
  "AAAAFLJE" decimal(22,0) NOT NULL default '0',
  "AAAAFLJF" varchar(14) NOT NULL default ' ',
  "AAAAFLJG" varchar(14) NOT NULL default ' ',
  "AAAAFLJH" varchar(20) NOT NULL default ' ',
  "AAAAZOEQ" varchar(20) NOT NULL default ' ',
  "AAAAFLJJ" varchar(20) NOT NULL default ' ',
  "AAAAFLJK" decimal(22,0) NOT NULL default '0',
  "AAAAFLJL" decimal(22,0) NOT NULL default '0',
  "AAAAFLJM" decimal(22,0) NOT NULL default '0',
  "AAAAFLJN" decimal(22,0) NOT NULL default '0',
  "AAAAFLJO" decimal(22,0) NOT NULL default '0',
  "AAAAFLJP" decimal(22,0) NOT NULL default '0',
  "AAAAFLPW" decimal(22,0) NOT NULL default '0',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  "AAAAFLJZ" varchar(10) NOT NULL default ' ',
  "AAAAFLKA" decimal(22,0) NOT NULL default '0',
  "AAAAFLKB" decimal(22,0) NOT NULL default '0',
  "AAAAFMAX" smallint(6) NOT NULL default '0',
  "AAAAFMAY" text,
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLKJ" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABDIOX" varchar(20) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLXV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFLXZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYB" decimal(22,0) NOT NULL default '0',
  "AAAAFLYC" decimal(22,0) NOT NULL default '0',
  "AAAAFLYM" decimal(22,0) NOT NULL default '0',
  "AAAAFLLC" decimal(22,0) NOT NULL default '0',
  "AAAAFLXW" decimal(22,0) NOT NULL default '0',
  "AAAAFLYA" decimal(22,0) NOT NULL default '0',
  "AAAAFLYD" decimal(22,0) NOT NULL default '0',
  "AAAAFLYE" decimal(22,0) NOT NULL default '0',
  "AAAAFLYF" decimal(22,0) NOT NULL default '0',
  "AAAAFLYG" decimal(22,0) NOT NULL default '0',
  "AAAAFLYH" decimal(22,0) NOT NULL default '0',
  "AAAAFLYI" decimal(22,0) NOT NULL default '0',
  "AAAAFLYJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYK" decimal(22,0) NOT NULL default '0',
  "AAAAFLYL" decimal(22,0) NOT NULL default '0',
  "AAAAFLYN" decimal(22,0) NOT NULL default '0',
  "AAAAFLYO" decimal(22,0) NOT NULL default '0',
  "AAAAFLYP" decimal(22,0) NOT NULL default '0',
  "AAAAFLYQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYR" decimal(22,0) NOT NULL default '0',
  "AAAAFLYS" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  "AAAAFLYX" decimal(22,0) NOT NULL default '0',
  "AAAAFLLZ" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABDIOX","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLME" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  "AAAAFLSA" decimal(22,0) NOT NULL default '0',
  "AAAAFLMO" decimal(22,0) NOT NULL default '0',
  "AAAAFLRO" decimal(22,0) NOT NULL default '0',
  "AAAAFLMQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLMR" decimal(22,0) NOT NULL default '0',
  "AAAAFLMS" decimal(22,0) NOT NULL default '0',
  "AAAAFLMT" decimal(22,0) NOT NULL default '0',
  "AAAAFLSD" decimal(22,0) NOT NULL default '0',
  "AAAAFLUK" decimal(22,0) NOT NULL default '0',
  "AAAAFLMW" decimal(22,0) NOT NULL default '0',
  "AAAAFLMX" decimal(22,0) NOT NULL default '0',
  "AAAAFLMY" decimal(22,0) NOT NULL default '0',
  "AAAAFLMZ" decimal(22,0) NOT NULL default '0',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLXV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFLXZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYB" decimal(22,0) NOT NULL default '0',
  "AAAAFLYC" decimal(22,0) NOT NULL default '0',
  "AAAAFLYM" decimal(22,0) NOT NULL default '0',
  "AAAAFLNJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNK" decimal(22,0) NOT NULL default '0',
  "AAAAFLNL" decimal(22,0) NOT NULL default '0',
  "AAAAFLNM" decimal(22,0) NOT NULL default '0',
  "AAAAFLNN" decimal(22,0) NOT NULL default '0',
  "AAAAFLNO" decimal(22,0) NOT NULL default '0',
  "AAAAFLNP" decimal(22,0) NOT NULL default '0',
  "AAAAFLNQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLNR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAC" decimal(22,0) NOT NULL default '0',
  "AAAAFMAF" decimal(22,0) NOT NULL default '0',
  "AAAAFLNU" decimal(22,0) NOT NULL default '0',
  "AAAAFLNV" decimal(22,0) NOT NULL default '0',
  "AAAAFLNW" decimal(22,0) NOT NULL default '0',
  "AAAAFLNX" decimal(22,0) NOT NULL default '0',
  "AAAAFLNY" decimal(22,0) NOT NULL default '0',
  "AAAAFLNZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLOA" decimal(22,0) NOT NULL default '0',
  "AAAAFLOB" decimal(22,0) NOT NULL default '0',
  "AAAAFMCD" varchar(14) NOT NULL default ' ',
  "AAAAFMCE" varchar(14) NOT NULL default ' ',
  "AAAAFLOE" varchar(14) NOT NULL default ' ',
  "AAAAFLOF" decimal(22,0) NOT NULL default '0',
  "AAAAFLSB" decimal(22,0) NOT NULL default '0',
  "AAAAFLOH" decimal(22,0) NOT NULL default '0',
  "AAAAFLOI" decimal(22,0) NOT NULL default '0',
  "AAAAFLOJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLOK" decimal(22,0) NOT NULL default '0',
  "AAAAFLOL" decimal(22,0) NOT NULL default '0',
  "AAAAFLYN" decimal(22,0) NOT NULL default '0',
  "AAAAFLYO" decimal(22,0) NOT NULL default '0',
  "AAAAFLYP" decimal(22,0) NOT NULL default '0',
  "AAAAFLYQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLOQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLOR" varchar(25) NOT NULL default ' ',
  "AAAAFLOS" decimal(22,0) NOT NULL default '0',
  "AAAAFLOT" decimal(22,0) NOT NULL default '0',
  "AAAAFLOU" decimal(22,0) NOT NULL default '0',
  "AAAAFLXU" decimal(22,0) NOT NULL default '0',
  "AAAAFLXW" decimal(22,0) NOT NULL default '0',
  "AAAAFLYA" decimal(22,0) NOT NULL default '0',
  "AAAAFLYD" decimal(22,0) NOT NULL default '0',
  "AAAAFLYE" decimal(22,0) NOT NULL default '0',
  "AAAAFLYF" decimal(22,0) NOT NULL default '0',
  "AAAAFLPB" decimal(22,0) NOT NULL default '0',
  "AAAAFLPC" decimal(22,0) NOT NULL default '0',
  "AAAAFLPD" decimal(22,0) NOT NULL default '0',
  "AAAAFLYG" decimal(22,0) NOT NULL default '0',
  "AAAAFLYH" decimal(22,0) NOT NULL default '0',
  "AAAAFLYI" decimal(22,0) NOT NULL default '0',
  "AAAAFLYJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYK" decimal(22,0) NOT NULL default '0',
  "AAAAFLYL" decimal(22,0) NOT NULL default '0',
  "AAAAFMAD" decimal(22,0) NOT NULL default '0',
  "AAAAFLPL" decimal(22,0) NOT NULL default '0',
  "AAAAFLPM" decimal(22,0) NOT NULL default '0',
  "AAAAFLPN" decimal(22,0) NOT NULL default '0',
  "AAAAFLPO" decimal(22,0) NOT NULL default '0',
  "AAAAFLPP" decimal(22,0) NOT NULL default '0',
  "AAAAFLPQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLPR" decimal(22,0) NOT NULL default '0',
  "AAAAFLPS" decimal(22,0) NOT NULL default '0',
  "AAAAFLPT" decimal(22,0) NOT NULL default '0',
  "AAAAFLPU" decimal(22,0) NOT NULL default '0',
  "AAAAFLPV" decimal(22,0) NOT NULL default '0',
  "AAAAFLPW" decimal(22,0) NOT NULL default '0',
  "AAAAFLVQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLPY" varchar(25) NOT NULL default ' ',
  "AAAAFLPZ" varchar(25) NOT NULL default ' ',
  "AAAAFLQA" varchar(32) NOT NULL default ' ',
  "AAAAFLQB" decimal(22,0) NOT NULL default '0',
  "AAAAFLQC" decimal(22,0) NOT NULL default '0',
  "AAAAFLQD" decimal(22,0) NOT NULL default '0',
  "AAAAFLQE" decimal(22,0) NOT NULL default '0',
  "AAAAFLQF" decimal(22,0) NOT NULL default '0',
  "AAAAFLQG" decimal(22,0) NOT NULL default '0',
  "AAAAFLQH" decimal(22,0) NOT NULL default '0',
  "AAAAFLQI" decimal(22,0) NOT NULL default '0',
  "AAAAFLQJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYS" decimal(22,0) NOT NULL default '0',
  "AAAAFLQL" decimal(22,0) NOT NULL default '0',
  "AAAAFLQM" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  "AAAAFLYX" decimal(22,0) NOT NULL default '0',
  "AAAAFLQS" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLQW" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLRD" varchar(15) NOT NULL default ' ',
  "AAABAIXH" varchar(50) NOT NULL default ' ',
  "AAAAFLRF" decimal(22,0) NOT NULL default '0',
  "AAAAFLSA" decimal(22,0) NOT NULL default '0',
  "AAAAFLRH" decimal(22,0) NOT NULL default '0',
  "AAAAFLRI" varchar(14) NOT NULL default ' ',
  "AAABDSAH" varchar(32) NOT NULL default ' ',
  "AAABGARW" varchar(5) NOT NULL default ' ',
  "AAABHAUB" varchar(20) NOT NULL default ' ',
  "AAABIMZM" varchar(20) NOT NULL default ' ',
  "AAAAFLRN" varchar(20) NOT NULL default ' ',
  "AAAAFLRO" decimal(22,0) NOT NULL default '0',
  "AAAAFLRP" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLRU" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLSA" decimal(22,0) NOT NULL default '0',
  "AAAAFLSB" decimal(22,0) NOT NULL default '0',
  "AAAAFLSC" decimal(22,0) NOT NULL default '0',
  "AAAAFLSD" decimal(22,0) NOT NULL default '0',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLSK" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLTO" varchar(1) NOT NULL default ' ',
  "AAAAFMBL" decimal(22,0) NOT NULL default '0',
  "AAAAFLTR" varchar(25) NOT NULL default ' ',
  "AAAAFLTQ" varchar(5) NOT NULL default ' ',
  "AAAAFLSW" varchar(25) NOT NULL default ' ',
  "AAAAFLSX" decimal(22,0) NOT NULL default '0',
  "AAABLTOK" varchar(20) NOT NULL default ' ',
  "AAAAFMBT" varchar(20) NOT NULL default ' ',
  "AAAAFLZA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLTG" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABAUGL" decimal(22,0) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLTO" varchar(1) NOT NULL default ' ',
  "AAAAFLTP" decimal(22,0) NOT NULL default '0',
  "AAAAFLTQ" varchar(5) NOT NULL default ' ',
  "AAAAFLTR" varchar(25) NOT NULL default ' ',
  "AAAAFLTS" varchar(14) NOT NULL default ' ',
  "AAABLTOK" varchar(20) NOT NULL default ' ',
  "AAAAFMBT" varchar(20) NOT NULL default ' ',
  "AAAAFLZA" varchar(20) NOT NULL default ' ',
  "AAAAFLTW" decimal(22,0) NOT NULL default '0',
  "AAAAFLTX" varchar(32) NOT NULL default ' ',
  "AAAAFLTY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABAUGL","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLUE" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLUK" decimal(22,0) NOT NULL default '0',
  "AAAAFLUL" decimal(22,0) NOT NULL default '0',
  "AAAAFLUM" decimal(22,0) NOT NULL default '0',
  "AAAAFLUN" decimal(22,0) NOT NULL default '0',
  "AAAAFLUO" varchar(14) NOT NULL default ' ',
  "AAAAFMCE" varchar(14) NOT NULL default ' ',
  "AAAAFLUQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLUR" decimal(22,0) NOT NULL default '0',
  "AAAAFLUS" decimal(22,0) NOT NULL default '0',
  "AAAAFLUT" decimal(22,0) NOT NULL default '0',
  "AAAAFLUU" decimal(22,0) NOT NULL default '0',
  "AAAAFLUV" decimal(22,0) NOT NULL default '0',
  "AAAAFLUW" decimal(22,0) NOT NULL default '0',
  "AAAAFLUX" varchar(5) NOT NULL default ' ',
  "AAAAFLUY" varchar(14) NOT NULL default ' ',
  "AAAAFLUZ" varchar(5) NOT NULL default ' ',
  "AAAAFLVA" varchar(14) NOT NULL default ' ',
  "AAAAFLVB" varchar(5) NOT NULL default ' ',
  "AAAAFLVC" varchar(14) NOT NULL default ' ',
  "AAAAFLVD" varchar(5) NOT NULL default ' ',
  "AAAAFLVE" varchar(14) NOT NULL default ' ',
  "AAAAFLVF" varchar(5) NOT NULL default ' ',
  "AAAAFLVG" varchar(14) NOT NULL default ' ',
  "AAAAFLVH" varchar(5) NOT NULL default ' ',
  "AAAAFLVI" varchar(14) NOT NULL default ' ',
  "AAAAFLVJ" varchar(25) NOT NULL default ' ',
  "AAAAFLVK" decimal(22,0) NOT NULL default '0',
  "AAAAFLVL" decimal(22,0) NOT NULL default '0',
  "AAAAFLVM" decimal(22,0) NOT NULL default '0',
  "AAAAFLVN" decimal(22,0) NOT NULL default '0',
  "AAAAFLVO" decimal(22,0) NOT NULL default '0',
  "AAAAFLVP" decimal(22,0) NOT NULL default '0',
  "AAAAFLVQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLVR" decimal(22,0) NOT NULL default '0',
  "AAAAFLVS" decimal(22,0) NOT NULL default '0',
  "AAAAFLVT" decimal(22,0) NOT NULL default '0',
  "AAAAQXEG" varchar(32) NOT NULL default ' ',
  "AAAAFLVV" varchar(20) NOT NULL default ' ',
  "AAAAFLVW" varchar(8) NOT NULL default ' ',
  "AAAAFLVX" decimal(22,0) NOT NULL default '0',
  "AAAAFLVY" decimal(22,0) NOT NULL default '0',
  "AAAAFLVZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLWA" decimal(22,0) NOT NULL default '0',
  "AAAAFLWB" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLWF" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLWL" decimal(22,0) NOT NULL default '0',
  "AAAAFLWM" decimal(22,0) NOT NULL default '0',
  "AAAAFLWN" decimal(22,0) NOT NULL default '0',
  "AAAAFLWO" decimal(22,0) NOT NULL default '0',
  "AAAAFLWP" decimal(22,0) NOT NULL default '0',
  "AAAAFLWQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLWR" decimal(22,0) NOT NULL default '0',
  "AAAAFLWS" decimal(22,0) NOT NULL default '0',
  "AAAAFLWT" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLWX" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFLXJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLXE" varchar(25) NOT NULL default ' ',
  "AAAAFLXF" decimal(22,0) NOT NULL default '0',
  "AAAAFLXG" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFLXJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLXL" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFLZA" varchar(30) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLXU" decimal(22,0) NOT NULL default '0',
  "AAAAFLXV" decimal(22,0) NOT NULL default '0',
  "AAAAFLXW" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFLXZ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYA" decimal(22,0) NOT NULL default '0',
  "AAAAFLYB" decimal(22,0) NOT NULL default '0',
  "AAAAFLYC" decimal(22,0) NOT NULL default '0',
  "AAAAFLYD" decimal(22,0) NOT NULL default '0',
  "AAAAFLYE" decimal(22,0) NOT NULL default '0',
  "AAAAFLYF" decimal(22,0) NOT NULL default '0',
  "AAAAFLYG" decimal(22,0) NOT NULL default '0',
  "AAAAFLYH" decimal(22,0) NOT NULL default '0',
  "AAAAFLYI" decimal(22,0) NOT NULL default '0',
  "AAAAFLYJ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYK" decimal(22,0) NOT NULL default '0',
  "AAAAFLYL" decimal(22,0) NOT NULL default '0',
  "AAAAFLYM" decimal(22,0) NOT NULL default '0',
  "AAAAFLYN" decimal(22,0) NOT NULL default '0',
  "AAAAFLYO" decimal(22,0) NOT NULL default '0',
  "AAAAFLYP" decimal(22,0) NOT NULL default '0',
  "AAAAFLYQ" decimal(22,0) NOT NULL default '0',
  "AAAAFLYR" decimal(22,0) NOT NULL default '0',
  "AAAAFLYS" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  "AAAAFLYX" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFLZA","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLZC" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLZI" decimal(22,0) NOT NULL default '0',
  "AAAAFMCD" varchar(14) NOT NULL default ' ',
  "AAAAFMCE" varchar(14) NOT NULL default ' ',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFLZR" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFLZY" decimal(22,0) NOT NULL default '0',
  "AAAAFLZZ" decimal(22,0) NOT NULL default '0',
  "AAAAFMAA" decimal(22,0) NOT NULL default '0',
  "AAAAFMAB" decimal(22,0) NOT NULL default '0',
  "AAAAFMAC" decimal(22,0) NOT NULL default '0',
  "AAAAFMAD" decimal(22,0) NOT NULL default '0',
  "AAAAFMBO" decimal(22,0) NOT NULL default '0',
  "AAAAFMAF" decimal(22,0) NOT NULL default '0',
  "AAAAFMBN" decimal(22,0) NOT NULL default '0',
  "AAAAFMAH" decimal(22,0) NOT NULL default '0',
  "AAAAFMAI" decimal(22,0) NOT NULL default '0',
  "AAAAFMAJ" decimal(22,0) NOT NULL default '0',
  "AAAAFMAK" decimal(22,0) NOT NULL default '0',
  "AAAAFMAL" decimal(22,0) NOT NULL default '0',
  "AAAAFMAM" decimal(22,0) NOT NULL default '0',
  "AAAAFMAN" decimal(22,0) NOT NULL default '0',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMAP" decimal(22,0) NOT NULL default '0',
  "AAAAFMAQ" decimal(22,0) NOT NULL default '0',
  "AAAAFMAR" decimal(22,0) NOT NULL default '0',
  "AAAAFMAS" decimal(22,0) NOT NULL default '0',
  "AAAAFMAT" decimal(22,0) NOT NULL default '0',
  "AAAAFMAU" decimal(22,0) NOT NULL default '0',
  "AAAAFMAV" decimal(22,0) NOT NULL default '0',
  "AAAAFMAW" decimal(22,0) NOT NULL default '0',
  "AAAAFMAX" smallint(6) NOT NULL default '0',
  "AAAAFMAY" text,
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMBD" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFMBT" varchar(20) NOT NULL default ' ',
  "AAABLTOK" varchar(20) NOT NULL default ' ',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMBL" decimal(22,0) NOT NULL default '0',
  "AAABJTGL" varchar(25) NOT NULL default ' ',
  "AAAAFMBN" decimal(22,0) NOT NULL default '0',
  "AAAAFMBO" decimal(22,0) NOT NULL default '0',
  "AAAAFMBP" decimal(22,0) NOT NULL default '0',
  "AAAAFMBQ" decimal(22,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFMBT","AAABLTOK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMBW" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMCC" decimal(22,0) NOT NULL default '0',
  "AAAAFMCD" varchar(14) NOT NULL default ' ',
  "AAAAFMCE" varchar(14) NOT NULL default ' ',
  "AAAAFMCF" varchar(20) NOT NULL default ' ',
  "AAAAIXXH" varchar(20) NOT NULL default ' ',
  "AAAAFMCH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMCL" (
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAAAVJYL" decimal(22,0) NOT NULL default '0',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY","AAABLEIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMCT" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABKNBI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAAAFMDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMCZ" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAAAFMDJ" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAAAFMDG" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAAAFMDJ","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMDL" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABKIUE" varchar(8) NOT NULL default '00000000',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIT","AAABMIRY","AAABKIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMDT" (
  "AAAAFNPR" varchar(20) NOT NULL default ' ',
  "AAAAFMEH" varchar(18) NOT NULL default ' ',
  "AAAAFMDW" varchar(18) NOT NULL default ' ',
  "AAAAFNPP" int(11) NOT NULL default '0',
  "AAAAFMDY" varchar(3) NOT NULL default ' ',
  "AAAAFMDZ" varchar(3) NOT NULL default ' ',
  "AAAAFMEA" varchar(3) NOT NULL default ' ',
  "AAAAFMEB" varchar(3) NOT NULL default ' ',
  "AAAAFMEC" varchar(26) NOT NULL default ' ',
  "AAAAFMED" varchar(6) NOT NULL default '000000',
  "AAAAFMEE" varchar(8) NOT NULL default '00000000',
  "AAAAFMEF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFNPR","AAAAFMEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMEI" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABLFYC" varchar(64) NOT NULL default ' ',
  "AAAAFMES" varchar(15) NOT NULL default ' ',
  "AAAAFMET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMFB" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMHN" varchar(4) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMFL" varchar(4) NOT NULL default ' ',
  "AAAAFMIL" varchar(30) NOT NULL default ' ',
  "AAAAFMFN" varchar(1) NOT NULL default ' ',
  "AAAAFMFO" varchar(1) NOT NULL default ' ',
  "AAAAIKTM" varchar(14) NOT NULL default ' ',
  "AAAAIKTO" varchar(14) NOT NULL default ' ',
  "AAAAFMFR" varchar(6) NOT NULL default ' ',
  "AAAAFMFS" varchar(6) NOT NULL default ' ',
  "AAABIYVQ" varchar(10) NOT NULL default ' ',
  "AAAAFMHA" varchar(4) NOT NULL default '0000',
  "AAAAFMHB" varchar(10) NOT NULL default ' ',
  "AAAAFMHC" varchar(16) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default ' ',
  "AAABLEJX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA","AAAAFMHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMGI" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMHN" varchar(4) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMGS" varchar(1) NOT NULL default ' ',
  "AAAAFMGT" varchar(6) NOT NULL default ' ',
  "AAABGFQV" varchar(14) NOT NULL default ' ',
  "AAAAFMGV" varchar(16) NOT NULL default ' ',
  "AAAAFMGW" varchar(4) NOT NULL default ' ',
  "AAAAIKTM" varchar(14) NOT NULL default ' ',
  "AAAAIKTO" varchar(14) NOT NULL default ' ',
  "AAABIYVQ" varchar(10) NOT NULL default ' ',
  "AAAAFMHA" varchar(4) NOT NULL default '0000',
  "AAAAFMHB" varchar(10) NOT NULL default ' ',
  "AAAAFMHC" varchar(16) NOT NULL default ' ',
  "AAAAVSPF" varchar(255) NOT NULL default ' ',
  "AAAAREWU" varchar(12) NOT NULL default ' ',
  "AAABLEJX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA","AAAAFMHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMHO" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAAAVIRM" varchar(30) NOT NULL default ' ',
  "AAAAFMHZ" varchar(30) NOT NULL default ' ',
  "AAABFVDB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABAKWH","AAABLESS","AAAAVIRM","AAAAFMHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMIA" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAABICFV" varchar(4) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAAAFMIL" varchar(30) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAAAFMIO" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA","AAABICFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMIX" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  "AAAAFMJC" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABAKWH","AAABLESS","AAABLENB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMJH" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMJQ" varchar(80) NOT NULL default ' ',
  "AAAAFMQR" varchar(12) NOT NULL default ' ',
  "AAAAFMQS" varchar(12) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMKB" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSF" varchar(20) NOT NULL default ' ',
  "AAAAFMKP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABAKWH","AAABLESS","AAAAFMSA","AAAAFMSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMKN" (
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMKP" varchar(80) NOT NULL default ' ',
  "AAAAFMSF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMKS" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAAAFMSF" varchar(20) NOT NULL default ' ',
  "AAABLTOK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABAKWH","AAABLESS","AAAAFMSF","AAABLTOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMLD" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAAAPJCL" varchar(255) NOT NULL default ' ',
  "AAABMRFO" varchar(30) NOT NULL default ' ',
  "AAAAFMSQ" varchar(14) NOT NULL default ' ',
  "AAAAFMSR" varchar(14) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  "AAABLEJX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA"),
  KEY "AAAAFMMB" ("AAAAFMSZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMME" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAABLGCM" varchar(100) NOT NULL default ' ',
  "AAAAFMPG" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMMP" varchar(10) NOT NULL default '0000000000',
  "AAAAFMMQ" varchar(10) NOT NULL default '0000000000',
  "AAABLFYC" varchar(64) NOT NULL default ' ',
  "AAABMRFO" varchar(30) NOT NULL default ' ',
  "AAAAFMSQ" varchar(14) NOT NULL default ' ',
  "AAAAFMSR" varchar(14) NOT NULL default ' ',
  "AAABKJAU" varchar(1) NOT NULL default ' ',
  "AAAAFMMW" varchar(30) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA","AAABLGCM","AAAAFMPG"),
  KEY "AAAAFMNH" ("AAAAFMSZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMNK" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABLENA" varchar(30) NOT NULL default ' ',
  "AAAAFMNU" varchar(30) NOT NULL default ' ',
  "AAAAFMNV" varchar(30) NOT NULL default ' ',
  "AAAAUFCV" varchar(12) NOT NULL default ' ',
  "AAAAFMNX" varchar(12) NOT NULL default ' ',
  "AAAAFMNY" varchar(12) NOT NULL default ' ',
  "AAAAFMNZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMOH" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAABLGCM" varchar(100) NOT NULL default ' ',
  "AAAAFMPG" varchar(10) NOT NULL default '0000000000',
  "AAAAFMPH" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMOT" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(30) NOT NULL default ' ',
  "AAAAFMSQ" varchar(14) NOT NULL default ' ',
  "AAAAFMSR" varchar(14) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA","AAABLGCM","AAAAFMPG","AAAAFMPH"),
  KEY "AAAAFMPI" ("AAAAFMSZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMPL" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAABAKWH" varchar(30) NOT NULL default ' ',
  "AAAAFMPV" varchar(30) NOT NULL default ' ',
  "AAAAFMPW" varchar(255) NOT NULL default ' ',
  "AAAAFMSQ" varchar(14) NOT NULL default ' ',
  "AAAAFMSR" varchar(14) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLESS","AAAAFMSX","AAAAFMTH","AAAAFMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMQG" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABLENA" varchar(30) NOT NULL default ' ',
  "AAABLFYC" varchar(64) NOT NULL default ' ',
  "AAAAFMQR" varchar(12) NOT NULL default ' ',
  "AAAAFMQS" varchar(12) NOT NULL default ' ',
  "AAAAFMQT" varchar(12) NOT NULL default ' ',
  "AAAAFMQU" varchar(12) NOT NULL default ' ',
  "AAAAFMQV" varchar(12) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAAAFMQX" varchar(30) NOT NULL default ' ',
  "AAAAFMQY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMRG" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMRZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSA" varchar(3) NOT NULL default ' ',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAABLENA" varchar(30) NOT NULL default ' ',
  "AAABLFYC" varchar(64) NOT NULL default ' ',
  "AAAAFMRR" varchar(12) NOT NULL default ' ',
  "AAABDBLY" varchar(12) NOT NULL default '000000000000',
  "AAAAFMRT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX","AAAAFMTH","AAAAFMRZ","AAAAFMSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMSB" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAUHQS" varchar(80) NOT NULL default ' ',
  "AAAAFMSF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABLTPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMSI" (
  "AAABLWPW" varchar(8) NOT NULL default ' ',
  "AAAAFMSZ" varchar(14) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMSX" varchar(10) NOT NULL default '0000000000',
  "AAAAFMSN" varchar(14) NOT NULL default ' ',
  "AAAAFMSO" varchar(255) NOT NULL default ' ',
  "AAABMRFO" varchar(30) NOT NULL default ' ',
  "AAAAFMSQ" varchar(14) NOT NULL default ' ',
  "AAAAFMSR" varchar(14) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  "AAABLEJX" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLWPW","AAAAFMSZ","AAABLTPE","AAAAFMSX"),
  KEY "AAAAFMSY" ("AAAAFMSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMTA" (
  "AAABFVMJ" varchar(10) NOT NULL default ' ',
  "AAABLTPE" varchar(30) NOT NULL default ' ',
  "AAAAFMTH" varchar(10) NOT NULL default ' ',
  "AAAAFMTE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABLTPE","AAAAFMTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMTI" (
  "AAAAXLFP" varchar(4) NOT NULL default ' ',
  "AAAATMPF" varchar(50) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAFMUP" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAAAFMTS" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFMYL" varchar(50) NOT NULL default ' ',
  "AAAAFMTV" varchar(1) NOT NULL default ' ',
  "AAAAFMTW" varchar(1) NOT NULL default ' ',
  "AAAAFMYO" varchar(80) NOT NULL default ' ',
  "AAAAFMYP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXLFP","AAAATMPF","AAABLEES","AAABLEER","AAABINIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMUE" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAABMKFH" varchar(50) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAAAFMUK" varchar(4) NOT NULL default ' ',
  "AAABILZT" varchar(12) NOT NULL default ' ',
  "AAABMKAI" varchar(1) NOT NULL default ' ',
  "AAAAFMUN" varchar(5) NOT NULL default ' ',
  "AAAAFMUO" varchar(1) NOT NULL default ' ',
  "AAAAFMUP" varchar(1) NOT NULL default ' ',
  "AAABLEER" varchar(14) NOT NULL default ' ',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABBZHR" varchar(10) NOT NULL default ' ',
  "AAAAFMUT" varchar(1) NOT NULL default ' ',
  "AAAAFMUU" varchar(132) NOT NULL default ' ',
  "AAAAFMUV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMJWN","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMUZ" (
  "AAAAHQBH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFMVD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHQBH","AAAAFMVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMVE" (
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABLEKR" varchar(3) NOT NULL default ' ',
  "AAABLEKS" varchar(30) NOT NULL default ' ',
  "AAABLEKT" varchar(255) NOT NULL default ' ',
  "AAABLEKU" varchar(255) NOT NULL default ' ',
  "AAAAFMVK" varchar(1) NOT NULL default ' ',
  "AAAAFMVL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFMVM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLEKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMVO" (
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABLFXL" varchar(3) NOT NULL default ' ',
  "AAABLEKS" varchar(30) NOT NULL default ' ',
  "AAABLEKT" varchar(255) NOT NULL default ' ',
  "AAAAFMVT" varchar(1) NOT NULL default ' ',
  "AAABEGBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKV","AAABLFXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMVX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAFMWK" varchar(120) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAFMWK","AAABLZKF","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMWN" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAAAFMWW" varchar(2) NOT NULL default '00',
  "AAABLUZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABLOSZ","AAABLPJD","AAAAFMWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMWX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUE","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMXF" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAFMXK" varchar(12) NOT NULL default ' ',
  "AAAAFMXL" varchar(1) NOT NULL default ' ',
  "AAAAFMXM" varchar(40) NOT NULL default ' ',
  "AAAAFMXN" varchar(50) NOT NULL default ' ',
  "AAABFVFF" varchar(80) NOT NULL default ' ',
  "AAABIOAK" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLGCM","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMXT" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(60) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMYA" (
  "AAAAXLFP" varchar(4) NOT NULL default ' ',
  "AAAATMPF" varchar(50) NOT NULL default ' ',
  "AAAAFNSE" varchar(8) NOT NULL default '00000000',
  "AAAAFNSF" varchar(6) NOT NULL default '000000',
  "AAABHBTO" varchar(5) NOT NULL default '00000',
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAAAFMYH" varchar(50) NOT NULL default ' ',
  "AAAAFMYI" varchar(50) NOT NULL default ' ',
  "AAAAFMYJ" varchar(50) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFMYL" varchar(50) NOT NULL default ' ',
  "AAABMKAI" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAAAFMYO" varchar(80) NOT NULL default ' ',
  "AAAAFMYP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXLFP","AAAATMPF","AAAAFNSE","AAAAFNSF","AAABHBTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFMYV" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAABMKFH" varchar(50) NOT NULL default ' ',
  "AAABLTIZ" varchar(14) NOT NULL default ' ',
  "AAABLEKC" int(11) NOT NULL default '0',
  "AAABLEJR" int(11) NOT NULL default '0',
  "AAABLEJS" varchar(255) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAABLEJV" varchar(31) NOT NULL default ' ',
  "AAABLEJW" varchar(255) NOT NULL default ' ',
  "AAABLEJX" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMJWN","AAABMKFH","AAABLTIZ","AAABLEKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNUJ" (
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAABIWUT" varchar(30) NOT NULL default ' ',
  "AAAALPVJ" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAAAFMZS" varchar(1) NOT NULL default ' ',
  "AAAAFMZT" varchar(1) NOT NULL default ' ',
  "AAAAFMZU" varchar(1) NOT NULL default ' ',
  "AAAAURUV" varchar(1) NOT NULL default ' ',
  "AAAAFMZW" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY","AAABIWUT","AAAALPVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAQBJR" varchar(10) NOT NULL default '0000000000',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXVP","AAABMLCZ"),
  UNIQUE KEY "AAAAFNAN" ("AAABMRIL","AAAAQBJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAAAQAUT" varchar(1) NOT NULL default ' ',
  "AAABMCRP" varchar(2) NOT NULL default '00',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABLXVP","AAAAQAUT","AAABMCRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAQBGF" varchar(1) NOT NULL default ' ',
  "AAAAQBGG" varchar(1) NOT NULL default ' ',
  "AAAAQBGH" varchar(1) NOT NULL default ' ',
  "AAAAQBGI" varchar(1) NOT NULL default ' ',
  "AAAAQBGJ" varchar(1) NOT NULL default ' ',
  "AAAAQBGK" varchar(1) NOT NULL default ' ',
  "AAAAQBGL" varchar(1) NOT NULL default ' ',
  "AAAAQBGM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURO","AAABLXVP","AAABMLCZ","AAABLUII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNBZ" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(50) NOT NULL default ' ',
  "AAAAFNSE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNCF" (
  "AAAAMTVX" varchar(8) NOT NULL default '00000000',
  "AAAAMTVY" varchar(6) NOT NULL default '000000',
  "AAAAIXXJ" varchar(16) NOT NULL default ' ',
  "AAAAFNCJ" decimal(16,0) NOT NULL default '0',
  "AAABHDAD" decimal(16,0) NOT NULL default '0',
  "AAAAFNCL" decimal(16,0) NOT NULL default '0',
  "AAAAFNCM" decimal(16,0) NOT NULL default '0',
  "AAAAFNCN" decimal(16,0) NOT NULL default '0',
  "AAAAFNCO" decimal(16,0) NOT NULL default '0',
  "AAAAFNCP" decimal(16,0) NOT NULL default '0',
  "AAAAFNCQ" decimal(16,0) NOT NULL default '0',
  "AAAAFNCR" decimal(16,0) NOT NULL default '0',
  "AAAAFNCS" decimal(16,0) NOT NULL default '0',
  "AAAAFNCT" decimal(16,0) NOT NULL default '0',
  "AAAAFNCU" decimal(16,0) NOT NULL default '0',
  "AAAAFNCV" decimal(16,0) NOT NULL default '0',
  "AAAAFNCW" decimal(16,0) NOT NULL default '0',
  "AAAAFNCX" decimal(16,0) NOT NULL default '0',
  "AAAAFNCY" decimal(16,0) NOT NULL default '0',
  "AAAAFNCZ" decimal(16,0) NOT NULL default '0',
  "AAAAFNDA" decimal(16,0) NOT NULL default '0',
  "AAAAFNDB" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAMTVX","AAAAMTVY","AAAAIXXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNDF" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABAELT" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNKC" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABAELT","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNDP" (
  "AAABMLPV" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAAAFNKK" varchar(18) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNKM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNKN" double NOT NULL default '0',
  "AAAAFNKO" double NOT NULL default '0',
  "AAAAFNKP" int(11) NOT NULL default '0',
  "AAAAFNKQ" smallint(6) NOT NULL default '0',
  "AAAAFNKR" double NOT NULL default '0',
  "AAAAFNKS" double NOT NULL default '0',
  "AAAAFNKT" int(11) NOT NULL default '0',
  "AAAAFNKU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAAAZOEQ","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNEH" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAAAFNLB" decimal(11,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAVBWM" varchar(6) NOT NULL default ' ',
  "AAAAFNLI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNEV" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABAELT" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNMD" decimal(11,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNMF" decimal(15,0) NOT NULL default '0',
  "AAAAFNMG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABAELT","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNFH" (
  "AAAAFNUJ" varchar(30) NOT NULL default ' ',
  "AAABAMRT" varchar(2) NOT NULL default ' ',
  "AAAAFNUL" varchar(20) NOT NULL default ' ',
  "AAABFXBZ" varchar(10) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAAAYEKQ" varchar(10) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNUE" varchar(1) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABKXWZ" varchar(8) NOT NULL default '00000000',
  "AAAAYGKN" varchar(6) NOT NULL default '000000',
  "AAAAFNUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFNUJ","AAABAMRT","AAAAFNUL","AAABFXBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNGC" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNRK" decimal(15,0) NOT NULL default '0',
  "AAAAFNNP" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNGL" (
  "AAABMLPV" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNNZ" double NOT NULL default '0',
  "AAAAFNOA" int(11) NOT NULL default '0',
  "AAAAFNOB" smallint(6) NOT NULL default '0',
  "AAAAFNOC" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAAAZOEQ","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNGZ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAABIGOG" decimal(11,0) NOT NULL default '0',
  "AAAAFNOK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOM" decimal(15,0) NOT NULL default '0',
  "AAAAFNON" decimal(15,0) NOT NULL default '0',
  "AAAAFNOO" decimal(15,0) NOT NULL default '0',
  "AAAAFNOP" decimal(15,0) NOT NULL default '0',
  "AAAAFNOQ" decimal(15,0) NOT NULL default '0',
  "AAAAFNOR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOS" decimal(15,0) NOT NULL default '0',
  "AAAAFNOT" decimal(15,0) NOT NULL default '0',
  "AAAAFNOU" decimal(15,0) NOT NULL default '0',
  "AAAAFNOV" decimal(15,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAVBWM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLVJK"),
  KEY "AAAAFNHY" ("AAABLVJK","AAAAFNTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNIB" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAFNPR" varchar(8) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNPM" varchar(10) NOT NULL default ' ',
  "AAAAFNPN" int(11) NOT NULL default '0',
  "AAAAFNPO" int(11) NOT NULL default '0',
  "AAAAFNPP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAAAFNPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNIO" (
  "AAABJCAB" int(11) NOT NULL default '0',
  "AAABMREL" varchar(18) NOT NULL default ' ',
  "AAAAFNRD" varchar(12) NOT NULL default ' ',
  "AAAAFNPW" varchar(12) NOT NULL default ' ',
  "AAAAFNPX" int(11) NOT NULL default '0',
  "AAAAFNPY" int(11) NOT NULL default '0',
  "AAABHAHM" smallint(6) NOT NULL default '0',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAABMMJT" varchar(4) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNQE" int(11) NOT NULL default '0',
  "AAAAFNQF" int(11) NOT NULL default '0',
  "AAAAFNQG" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABJCAB","AAABMREL","AAAAFNRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNJG" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNRK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRL" decimal(11,0) NOT NULL default '0',
  "AAAAFNRM" decimal(11,0) NOT NULL default '0',
  "AAAAFNRN" int(11) NOT NULL default '0',
  "AAABHFND" decimal(15,0) NOT NULL default '0',
  "AAAAFNRP" decimal(7,0) NOT NULL default '0',
  "AAAAFNRQ" decimal(11,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNJW" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABAELT" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNKC" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABAELT","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNKG" (
  "AAABMLPV" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAAAFNKK" varchar(18) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNKM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNKN" double NOT NULL default '0',
  "AAAAFNKO" double NOT NULL default '0',
  "AAAAFNKP" int(11) NOT NULL default '0',
  "AAAAFNKQ" smallint(6) NOT NULL default '0',
  "AAAAFNKR" double NOT NULL default '0',
  "AAAAFNKS" double NOT NULL default '0',
  "AAAAFNKT" int(11) NOT NULL default '0',
  "AAAAFNKU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNKX" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAAAFNLB" decimal(11,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAFNPB" varchar(6) NOT NULL default ' ',
  "AAAAFNLI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNLK" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAFNPR" varchar(20) NOT NULL default ' ',
  "AAABAELT" varchar(18) NOT NULL default ' ',
  "AAAAFNLX" varchar(20) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNLR" int(11) NOT NULL default '0',
  "AAAAFNLS" smallint(6) NOT NULL default '0',
  "AAAAFNLT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAAAFNPR","AAABAELT","AAAAFNLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNLY" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABAELT" varchar(18) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNMD" decimal(11,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNMF" decimal(15,0) NOT NULL default '0',
  "AAAAFNMG" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABAELT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNMJ" (
  "AAAAFNUJ" varchar(30) NOT NULL default ' ',
  "AAABAMRT" varchar(2) NOT NULL default ' ',
  "AAAAFNUL" varchar(20) NOT NULL default ' ',
  "AAABFXBZ" varchar(10) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAAAYEKQ" varchar(10) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNUE" varchar(1) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABKXWZ" varchar(8) NOT NULL default '00000000',
  "AAAAYGKN" varchar(6) NOT NULL default '000000',
  "AAAAFNUI" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(30) NOT NULL default ' ',
  "AAAAFNNB" varchar(100) NOT NULL default ' ',
  "AAAAFNNC" varchar(4) NOT NULL default ' ',
  "AAAAFNND" varchar(2) NOT NULL default ' ',
  "AAAAFNNE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFNUJ","AAABAMRT","AAAAFNUL","AAABFXBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNNJ" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNRK" decimal(15,0) NOT NULL default '0',
  "AAAAFNNP" decimal(11,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAABLVJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNNS" (
  "AAABMLPV" varchar(18) NOT NULL default ' ',
  "AAAAZOEQ" varchar(8) NOT NULL default ' ',
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNNZ" double NOT NULL default '0',
  "AAAAFNOA" int(11) NOT NULL default '0',
  "AAAAFNOB" smallint(6) NOT NULL default '0',
  "AAAAFNOC" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNOF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(1) NOT NULL default ' ',
  "AAABIGOG" decimal(11,0) NOT NULL default '0',
  "AAAAFNOK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOM" decimal(15,0) NOT NULL default '0',
  "AAAAFNON" decimal(15,0) NOT NULL default '0',
  "AAAAFNOO" decimal(15,0) NOT NULL default '0',
  "AAAAFNOP" decimal(15,0) NOT NULL default '0',
  "AAAAFNOQ" decimal(15,0) NOT NULL default '0',
  "AAAAFNOR" decimal(15,0) NOT NULL default '0',
  "AAAAFNOS" decimal(15,0) NOT NULL default '0',
  "AAAAFNOT" decimal(15,0) NOT NULL default '0',
  "AAAAFNOU" decimal(15,0) NOT NULL default '0',
  "AAAAFNOV" decimal(15,0) NOT NULL default '0',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default ' ',
  "AAAAFNOY" varchar(8) NOT NULL default '00000000',
  "AAAAFNOZ" varchar(6) NOT NULL default ' ',
  "AAAAFNPA" varchar(8) NOT NULL default '00000000',
  "AAAAFNPB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL"),
  KEY "AAAAFNPD" ("AAAAFNTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNPF" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAFNPR" varchar(20) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNPM" varchar(10) NOT NULL default ' ',
  "AAAAFNPN" int(11) NOT NULL default '0',
  "AAAAFNPO" int(11) NOT NULL default '0',
  "AAAAFNPP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV","AAAAFNPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNPS" (
  "AAABJCAB" int(11) NOT NULL default '0',
  "AAABMREL" varchar(18) NOT NULL default ' ',
  "AAAAFNRD" varchar(12) NOT NULL default ' ',
  "AAAAFNPW" varchar(12) NOT NULL default ' ',
  "AAAAFNPX" int(11) NOT NULL default '0',
  "AAAAFNPY" int(11) NOT NULL default '0',
  "AAABHAHM" smallint(6) NOT NULL default '0',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAABMMJT" varchar(4) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAAFNQE" int(11) NOT NULL default '0',
  "AAAAFNQF" int(11) NOT NULL default '0',
  "AAAAFNQG" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABJCAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNQI" (
  "AAABJCAB" int(11) NOT NULL default '0',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFNRD" varchar(12) NOT NULL default ' ',
  "AAAAFNQM" int(11) NOT NULL default '0',
  "AAAAFNRB" varchar(12) NOT NULL default ' ',
  "AAAAFNQO" int(11) NOT NULL default '0',
  "AAAAHQBH" varchar(1) NOT NULL default ' ',
  "AAAAFNRK" decimal(15,0) NOT NULL default '0',
  "AAAAFNQR" int(11) NOT NULL default '0',
  "AAABILMR" int(11) NOT NULL default '0',
  "AAABDBLY" int(11) NOT NULL default '0',
  "AAAAFNQU" int(11) NOT NULL default '0',
  "AAAAFNQV" int(11) NOT NULL default '0',
  "AAAAFNQW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABJCAB"),
  KEY "AAAAFNQY" ("AAABMREL"),
  KEY "AAAAFNRA" ("AAAAFNRB"),
  KEY "AAAAFNRC" ("AAAAFNRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNRE" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABLVJK" varchar(12) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNRK" decimal(15,0) NOT NULL default '0',
  "AAAAFNRL" decimal(11,0) NOT NULL default '0',
  "AAAAFNRM" decimal(11,0) NOT NULL default '0',
  "AAAAFNRN" int(11) NOT NULL default '0',
  "AAABHFND" decimal(15,0) NOT NULL default '0',
  "AAAAFNRP" decimal(7,0) NOT NULL default '0',
  "AAAAFNRQ" decimal(11,0) NOT NULL default '0',
  "AAAAFNRR" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUAV"),
  KEY "AAAAFNRT" ("AAAAFNTX","AAAATQNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNRW" (
  "AAAAFNSE" varchar(8) NOT NULL default '00000000',
  "AAAAFNSF" varchar(6) NOT NULL default '000000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAAAFNSC" varchar(4) NOT NULL default ' ',
  "AAAAFNSD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFNSE","AAAAFNSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNSG" (
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAABIGIZ" varchar(18) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFNSL" varchar(250) NOT NULL default ' ',
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAAAGPSM" varchar(1) NOT NULL default ' ',
  "AAABKNFD" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAAAFNSU" blob,
  PRIMARY KEY ("AAABIGLE","AAABIGLF","AAABIGIZ"),
  KEY "AAAAFNSY" ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNTA" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAHGJK" varchar(86) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(4) NOT NULL default ' ',
  "AAAAVLST" varchar(8) NOT NULL default ' ',
  "AAAAGPSM" varchar(1) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAAAPTWY" blob,
  PRIMARY KEY ("AAABMREL","AAABIITH","AAABIITI","AAAAHGJK","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNTS" (
  "AAAAFNUJ" varchar(30) NOT NULL default ' ',
  "AAABAMRT" varchar(2) NOT NULL default ' ',
  "AAAAFNUL" varchar(20) NOT NULL default ' ',
  "AAABFXBZ" varchar(10) NOT NULL default ' ',
  "AAAAFNTX" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAAAYEKQ" varchar(10) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABKYCI" tinyint(3) unsigned NOT NULL default '0',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(5) NOT NULL default ' ',
  "AAAAFNUE" varchar(1) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  "AAABKXWZ" varchar(8) NOT NULL default '00000000',
  "AAAAYGKN" varchar(6) NOT NULL default '000000',
  "AAAAFNUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFNUJ","AAABAMRT","AAAAFNUL","AAABFXBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRQB" varchar(1) NOT NULL default ' ',
  "AAABKQXG" varchar(3) NOT NULL default '000',
  "AAABDHAN" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAAAMLQB" varchar(1) NOT NULL default ' ',
  "AAAAMLQC" varchar(1) NOT NULL default ' ',
  "AAAAWUYL" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAAAPDVA" varchar(1) NOT NULL default ' ',
  "AAAAMLQG" varchar(1) NOT NULL default ' ',
  "AAAAMLQH" varchar(8) NOT NULL default '00000000',
  "AAAAMLQI" decimal(15,0) NOT NULL default '0',
  "AAAAMLQJ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDRQB","AAABKQXG","AAABDHAN","AAABMLCZ","AAABLXVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRQB" varchar(1) NOT NULL default ' ',
  "AAABDHAN" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXVP" varchar(10) NOT NULL default ' ',
  "AAAAMLQB" varchar(1) NOT NULL default ' ',
  "AAAAMLQC" varchar(1) NOT NULL default ' ',
  "AAAAWUYL" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAAAPDVA" varchar(1) NOT NULL default ' ',
  "AAAAMLQG" varchar(1) NOT NULL default ' ',
  "AAAAMLQH" varchar(8) NOT NULL default '00000000',
  "AAAAMLQI" decimal(15,0) NOT NULL default '0',
  "AAAAMLQJ" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDRQB","AAABDHAN","AAABMLCZ","AAABLXVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNWD" (
  "AAAAMTVX" varchar(8) NOT NULL default '00000000',
  "AAAAMTVY" varchar(6) NOT NULL default '000000',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAAAFNWH" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFNWI" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFNWJ" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFNWK" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAMTVX","AAAAMTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNWN" (
  "AAAAHQBH" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABIGXI" varchar(3) NOT NULL default '000',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABKNBI" varchar(5) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABJVWR" varchar(100) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAHQBH","AAABMOHM","AAABMLPV","AAABLUSV","AAABIGXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNXD" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAFNXG" int(11) NOT NULL default '0',
  "AAAAFNXH" int(11) NOT NULL default '0',
  "AAAAFNXI" int(11) NOT NULL default '0',
  "AAAAFNXJ" int(11) NOT NULL default '0',
  "AAAAFNXK" varchar(8) NOT NULL default '00000000',
  "AAAAFPRN" varchar(8) NOT NULL default '00000000',
  "AAAAFNXM" varchar(8) NOT NULL default '00000000',
  "AAAALOOW" varchar(12) NOT NULL default ' ',
  "AAABFWKD" varchar(8) NOT NULL default '00000000',
  "AAAALOOY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNXR" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAAMMRA" varchar(1) NOT NULL default ' ',
  "AAAATKMD" varchar(1) NOT NULL default ' ',
  "AAABEGEB" varchar(1) NOT NULL default ' ',
  "AAAAFQHH" varchar(30) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABGYPG" varchar(20) NOT NULL default ' ',
  "AAAAUNWY" varchar(4) NOT NULL default '0000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFNYN" varchar(1) NOT NULL default ' ',
  "AAAAFNYO" varchar(4) NOT NULL default ' ',
  "AAAAFNYP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABIIUG","AAABIIUH"),
  KEY "AAAAFNYT" ("AAAAFQHH","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNYW" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNZG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAAAFNZK" varchar(20) NOT NULL default ' ',
  "AAAAFNZL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFNZM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNZP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAAAFNZT" varchar(1) NOT NULL default ' ',
  "AAABHBWQ" varchar(8) NOT NULL default ' ',
  "AAABFVMJ" varchar(8) NOT NULL default ' ',
  "AAAAFNZW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFNZZ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOAG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABHBDC" varchar(30) NOT NULL default ' ',
  "AAABEQXX" varchar(10) NOT NULL default '0000000000',
  "AAABEQXY" varchar(10) NOT NULL default '0000000000',
  "AAABEQZP" varchar(4) NOT NULL default '0000',
  "AAABEQZQ" varchar(4) NOT NULL default '0000',
  "AAABCDJJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOAS" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIRTT" varchar(8) NOT NULL default ' ',
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAAAFOAY" varchar(10) NOT NULL default '0000000000',
  "AAAAFOAZ" varchar(10) NOT NULL default '0000000000',
  "AAAAFOBA" varchar(10) NOT NULL default '0000000000',
  "AAABKNBP" varchar(1) NOT NULL default ' ',
  "AAABALMY" varchar(1) NOT NULL default ' ',
  "AAAAFOBD" varchar(1) NOT NULL default ' ',
  "AAAANACO" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAARXGW" varchar(1) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAAAFOBO" varchar(4) NOT NULL default ' ',
  "AAAAFOWY" varchar(1) NOT NULL default ' ',
  "AAAAFOBQ" varchar(3) NOT NULL default '000',
  "AAAAPYOK" varchar(1) NOT NULL default ' ',
  "AAAAMZVG" varchar(1) NOT NULL default ' ',
  "AAAAFOBT" varchar(1) NOT NULL default ' ',
  "AAAAFOBU" varchar(1) NOT NULL default ' ',
  "AAAAFPGE" varchar(1) NOT NULL default ' ',
  "AAAAFOBW" varchar(1) NOT NULL default '0',
  "AAAAFOBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUG","AAABIIUH"),
  KEY "AAAAFOCB" ("AAABIIUD","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOCE" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOCK" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOCU" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAAAFODE" varchar(1) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABHCYR" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAAAFODK" varchar(10) NOT NULL default ' ',
  "AAAALGUP" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAAAFODQ" varchar(1) NOT NULL default ' ',
  "AAAAXFPL" varchar(1) NOT NULL default ' ',
  "AAABHWBK" varchar(2) NOT NULL default '00',
  "AAAAVQDY" varchar(1) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAAAFODV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABIIUG","AAABIIUH","AAABILRT"),
  UNIQUE KEY "AAAAFOEB" ("AAABMREL","AAABIIUG","AAABILRT","AAABIIUH"),
  KEY "AAAAFOEG" ("AAABHUGE","AAABIIUG"),
  KEY "AAAAFOEJ" ("AAABJEDB","AAABIIUG"),
  KEY "AAAAFOEM" ("AAABCXXG","AAABMKXX","AAABIIUG"),
  KEY "AAAAFOEQ" ("AAABMREL","AAABIIUG","AAABMREF"),
  KEY "AAAAFOEU" ("AAABHCYR"),
  KEY "AAAAFOEW" ("AAABIGEG","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOEZ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABEZYX","AAABIIUG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOFJ" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAAAFOFO" varchar(6) NOT NULL default '000000',
  "AAABBVNZ" varchar(20) NOT NULL default ' ',
  "AAAAUNWW" varchar(1) NOT NULL default ' ',
  "AAABDSOB" varchar(2) NOT NULL default '00',
  "AAAAUNXB" varchar(2) NOT NULL default '00',
  "AAAAUNXC" varchar(2) NOT NULL default '00',
  "AAAAUNXD" varchar(2) NOT NULL default '00',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFOGB" varchar(1) NOT NULL default ' ',
  "AAAAFOGC" varchar(4) NOT NULL default ' ',
  "AAAAFOGD" varchar(1) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAAANBII" varchar(30) NOT NULL default ' ',
  "AAAAFOGG" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAAAMMRA" varchar(1) NOT NULL default ' ',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  "AAABEGEB" varchar(1) NOT NULL default ' ',
  "AAAAFQHH" varchar(30) NOT NULL default ' ',
  "AAAAFOGQ" varchar(1) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAAAFPGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE","AAABIIUG","AAABIIUH"),
  KEY "AAAAFOGW" ("AAABIGEG","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOGZ" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOHN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAFOHZ" varchar(4) NOT NULL default '0000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOHT" varchar(30) NOT NULL default ' ',
  "AAAAFOHU" varchar(30) NOT NULL default ' ',
  "AAAAFOHV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUG","AAABMREF","AAAAFOHZ","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOIB" (
  "AAABIIUD" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOIF" varchar(8) NOT NULL default ' ',
  "AAAAFOIG" varchar(3) NOT NULL default '000',
  "AAAAFOIH" varchar(5) NOT NULL default '00000',
  "AAAAFOII" varchar(11) NOT NULL default '00000000000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAAFOIQ" varchar(4) NOT NULL default ' ',
  "AAAAJQPO" VARBINARY(255),
  PRIMARY KEY ("AAABIIUD","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOIV" (
  "AAABIIUD" varchar(10) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIUD","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOJF" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABDNYT" varchar(4) NOT NULL default '0000',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOJZ" varchar(10) NOT NULL default ' ',
  "AAAAFOJL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABIIUG","AAABDNYT","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOJQ" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABDNYT" varchar(4) NOT NULL default '0000',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  "AAAAFOJX" varchar(10) NOT NULL default ' ',
  "AAAAFOJY" varchar(10) NOT NULL default ' ',
  "AAAAFOJZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABEZYX","AAABIIUG","AAABDNYT","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOKF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAAAFOLB" varchar(4) NOT NULL default ' ',
  "AAAAFOKM" varchar(30) NOT NULL default ' ',
  "AAAAFOKN" varchar(10) NOT NULL default ' ',
  "AAABMMVJ" varchar(2) NOT NULL default ' ',
  "AAAAFOKP" varchar(2) NOT NULL default ' ',
  "AAAAFOKQ" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAAFOKT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUG","AAABMREF","AAABIIUH"),
  KEY "AAAAFOKY" ("AAABJEDB","AAABIIUG","AAAAFOLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOLC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  "AAAAFOLJ" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABEZYX","AAABIIUG","AAABMREF","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOLP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFOME" varchar(1) NOT NULL default ' ',
  "AAAAFOMF" varchar(3) NOT NULL default '000',
  "AAABMINC" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOLY" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABEQYB" varchar(2) NOT NULL default ' ',
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAAAFOME" varchar(1) NOT NULL default ' ',
  "AAAAFOMF" varchar(3) NOT NULL default '000',
  "AAABMINC" varchar(1) NOT NULL default ' ',
  "AAAAFOMH" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFOML" varchar(1) NOT NULL default ' ',
  "AAABILZU" varchar(1) NOT NULL default ' ',
  "AAAAFOMN" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAAAFOMP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOMT" (
  "AAAAFONA" varchar(1) NOT NULL default ' ',
  "AAAAFOMV" varchar(1) NOT NULL default ' ',
  "AAAAFOMW" int(11) NOT NULL default '0',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFONA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFONB" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAAAJORQ" varchar(8) NOT NULL default ' ',
  "AAABEQXQ" varchar(10) NOT NULL default '0000000000',
  "AAABEQXR" varchar(10) NOT NULL default '0000000000',
  "AAABGHOW" varchar(2) NOT NULL default '00',
  "AAABGHOY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFONM" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABHBWQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFONT" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABHBDC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOOA" (
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABIITF" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIITK" varchar(18) NOT NULL default ' ',
  "AAABIITL" varchar(1) NOT NULL default ' ',
  "AAABIITM" varchar(1) NOT NULL default ' ',
  "AAABIITN" varchar(3) NOT NULL default ' ',
  "AAABIITO" varchar(3) NOT NULL default ' ',
  "AAABIITP" varchar(3) NOT NULL default ' ',
  "AAABIITQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIUD","AAABIIUG","AAABIIUE","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOOW" (
  "AAABINMV" varchar(18) NOT NULL default ' ',
  "AAABLUZL" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINMV","AAABLUZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOPC" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIUD","AAABEZYX","AAABIIUG","AAABIIUE","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOPO" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABJLPN" varchar(4) NOT NULL default '0000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOPT" varchar(30) NOT NULL default ' ',
  "AAAAFOPU" varchar(4) NOT NULL default ' ',
  "AAABIXKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABJLPN","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOQA" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABGGIZ" varchar(8) NOT NULL default ' ',
  "AAABIIUD" varchar(10) NOT NULL default ' ',
  "AAABKNBP" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLGCM","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOQQ" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZYX","AAABLGCM","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFORA" (
  "AAABIIUD" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIIUD","AAABIIUG","AAABMREF","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFORP" (
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIUD","AAABIIUG","AAABIIUE","AAABILRT","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOSC" (
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAAALPZQ" varchar(30) NOT NULL default ' ',
  "AAABDAFW" varchar(10) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOST" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOUV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOTD" (
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAAAFOTJ" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABIIUG","AAABFYLB","AAABILRT","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOTQ" (
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAAAFOTY" varchar(1) NOT NULL default ' ',
  "AAAAFOTZ" varchar(1) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  "AAAAMCRB" varchar(10) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABGGED" VARBINARY(255),
  "AAAAFOUJ" varchar(10) NOT NULL default ' ',
  "AAABINMV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABIIUG","AAABFYLB","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOUP" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOUV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABEZYX","AAABIIUG","AAABFYLB","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOVB" (
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFOVZ" varchar(30) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFOWE" varchar(30) NOT NULL default ' ',
  "AAABJLPN" varchar(4) NOT NULL default '0000',
  "AAABHAGF" varchar(4) NOT NULL default '0000',
  "AAABFRJL" varchar(4) NOT NULL default '0000',
  "AAABHEIH" varchar(1) NOT NULL default '0',
  "AAAAFOVO" varchar(2) NOT NULL default '00',
  "AAAAFOVP" varchar(1) NOT NULL default ' ',
  "AAABLQDV" varchar(20) NOT NULL default ' ',
  "AAAAFOVR" varchar(1) NOT NULL default ' ',
  "AAAAFOVS" varchar(1) NOT NULL default ' ',
  "AAABGGED" VARBINARY(255),
  "AAAAFOVU" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYLA","AAABIIUG","AAABFYLB","AAAAFOVZ","AAABIIUH"),
  KEY "AAAAFOWB" ("AAABMREL","AAABIIUG","AAAAFOWE","AAABFYLA","AAABFYLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOWH" (
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAAFOWL" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAAALPZQ" varchar(30) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAPYOK" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAAFOWW" varchar(1) NOT NULL default ' ',
  "AAAAMZVG" varchar(1) NOT NULL default ' ',
  "AAAAFOWY" varchar(1) NOT NULL default ' ',
  "AAAAFOWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG","AAABIIUG","AAABIIUH"),
  KEY "AAAAFOXD" ("AAABIIUG","AAAALPZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOXG" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOXQ" (
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAAATREK" varchar(4) NOT NULL default '0000',
  "AAAAFOYI" varchar(30) NOT NULL default ' ',
  "AAAAFOYJ" varchar(30) NOT NULL default ' ',
  "AAAAFOXY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG","AAABIIUG","AAABMREL","AAABIIUH"),
  KEY "AAAAFOYD" ("AAABMREL","AAAAFOYJ","AAABIIUG"),
  KEY "AAAAFOYH" ("AAAAFOYI","AAAAFOYJ","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOYL" (
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABJLPN" varchar(4) NOT NULL default '0000',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABHNSR" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAAAFOYU" varchar(3) NOT NULL default ' ',
  "AAABHNOW" varchar(16) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAFOYX" varchar(1) NOT NULL default ' ',
  "AAAAFOYY" varchar(1) NOT NULL default ' ',
  "AAAAFPDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG","AAABIIUG","AAABJLPN","AAABIIUH"),
  KEY "AAAAFOZE" ("AAABMREL","AAABIIUG","AAABMREF"),
  KEY "AAAAFOZI" ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFOZK" (
  "AAAAFPBB" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHNSS" varchar(3) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAATLAO" varchar(38) NOT NULL default ' ',
  "AAAAFOZU" varchar(1) NOT NULL default ' ',
  "AAABHNSV" varchar(3) NOT NULL default ' ',
  "AAABHAGF" varchar(4) NOT NULL default '0000',
  "AAABFRJL" varchar(4) NOT NULL default '0000',
  "AAAAFOZY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPBB","AAABIIUG","AAABIIUH","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPAD" (
  "AAAAFPBB" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIITE" varchar(2) NOT NULL default '00',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAAAFPAM" varchar(4) NOT NULL default '0000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPBB","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPAV" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAAAFPBB" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPBB","AAABEZYX","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPBF" (
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPBI" varchar(1) NOT NULL default ' ',
  "AAAAFPBJ" varchar(1) NOT NULL default ' ',
  "AAAATJWP" varchar(1) NOT NULL default ' ',
  "AAAAFPBL" varchar(1) NOT NULL default ' ',
  "AAAAXLNL" varchar(30) NOT NULL default ' ',
  "AAAAFPBN" varchar(1) NOT NULL default ' ',
  "AAABHUHN" varchar(30) NOT NULL default ' ',
  "AAAAFPBZ" varchar(30) NOT NULL default ' ',
  "AAABFGJT" varchar(1) NOT NULL default ' ',
  "AAAAFPBR" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABFEAO","AAABIIUG"),
  KEY "AAAAFPBY" ("AAAAFPBZ"),
  KEY "AAAAFPCA" ("AAAAXLNL"),
  KEY "AAAAFPCC" ("AAABHUHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPCE" (
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEAO","AAABEZYX","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPCM" (
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAAAFPEI" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPCQ" varchar(4) NOT NULL default '0000',
  "AAAAFPCR" varchar(30) NOT NULL default ' ',
  "AAAAFPCS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEAO","AAAAFPEI","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPCW" (
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPDA" varchar(4) NOT NULL default '0000',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAFPDC" varchar(1) NOT NULL default ' ',
  "AAAAFPDD" varchar(1) NOT NULL default ' ',
  "AAAAFPDE" varchar(2) NOT NULL default '00',
  "AAAAFPDF" varchar(1) NOT NULL default ' ',
  "AAAAFPDG" varchar(2) NOT NULL default '00',
  "AAAAFPDH" varchar(1) NOT NULL default ' ',
  "AAAAVEPU" varchar(21) NOT NULL default ' ',
  "AAAAKTRJ" varchar(1) NOT NULL default ' ',
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAAAFPDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEAO","AAABMREF","AAABIIUG"),
  KEY "AAAAFPDT" ("AAABHUGE"),
  KEY "AAAAFPDV" ("AAABIIUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPDX" (
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAFPEI" varchar(30) NOT NULL default ' ',
  "AAAAZOLS" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAVEPU" varchar(21) NOT NULL default ' ',
  "AAAAKTRJ" varchar(1) NOT NULL default ' ',
  "AAAAFPEF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEAO","AAABMREF","AAAAFPEI","AAAAZOLS","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPEL" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAAAFPEQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABIIUG"),
  KEY "AAAAFPEU" ("AAABFEAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPEW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAANBII" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPFB" varchar(1) NOT NULL default ' ',
  "AAAAFPFC" varchar(30) NOT NULL default ' ',
  "AAAAFPFD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAAANBII","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPFI" (
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(4) NOT NULL default ' ',
  "AAAAFPGI" varchar(30) NOT NULL default ' ',
  "AAAAFPFN" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(4) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMNMM" varchar(6) NOT NULL default '000000',
  "AAABDMST" varchar(1) NOT NULL default ' ',
  "AAAAFPFS" varchar(1) NOT NULL default ' ',
  "AAAAFPFT" varchar(1) NOT NULL default ' ',
  "AAAAFPFU" varchar(4) NOT NULL default '0000',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAAAFPFW" varchar(6) NOT NULL default '000000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFPGA" varchar(1) NOT NULL default ' ',
  "AAAAFPGB" varchar(30) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAABHLQD" varchar(5) NOT NULL default '00000',
  "AAAAFPGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAABIIUG"),
  KEY "AAAAFPGH" ("AAAAFPGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPGJ" (
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAABEZYX","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPGR" (
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAAAFPGY" varchar(4) NOT NULL default '0000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPGV" varchar(4) NOT NULL default '0000',
  "AAABMLPN" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO","AAAAFPGY","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPHA" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAAAXLNL" varchar(30) NOT NULL default ' ',
  "AAAAFPHF" varchar(30) NOT NULL default ' ',
  "AAABHJFS" varchar(30) NOT NULL default ' ',
  "AAAAFPHH" varchar(30) NOT NULL default ' ',
  "AAAAFPHI" varchar(20) NOT NULL default ' ',
  "AAAARINI" varchar(30) NOT NULL default ' ',
  "AAAAFPHK" varchar(1) NOT NULL default ' ',
  "AAAAFPHL" varchar(1) NOT NULL default ' ',
  "AAAAFPHM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  "AAAAPINY" int(11) NOT NULL default '0',
  "AAAAFPHP" varchar(2) NOT NULL default '00',
  "AAAAFPHQ" varchar(2) NOT NULL default '00',
  "AAAAFPHR" varchar(3) NOT NULL default '000',
  "AAAAFPHS" varchar(70) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFPHW" varchar(1) NOT NULL default ' ',
  "AAAAFPHX" varchar(1) NOT NULL default ' ',
  "AAAAFPHY" varchar(1) NOT NULL default ' ',
  "AAAAFPHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPIC" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAABEZYX","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPIK" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPJZ" varchar(4) NOT NULL default '0000',
  "AAAAFPIP" varchar(6) NOT NULL default '000000',
  "AAAAFPIQ" varchar(6) NOT NULL default '000000',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAABMLPN","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPIV" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAAAFPJE" varchar(25) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPJZ" varchar(4) NOT NULL default '0000',
  "AAAAFPJC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAAAFPJE","AAABHEIG","AAABMREF","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPJI" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPJZ" varchar(4) NOT NULL default '0000',
  "AAABLEEK" varchar(1) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABIKZE" varchar(1) NOT NULL default ' ',
  "AAAAFPJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAABGFET","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPJU" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPJZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAFPKK","AAABMLQA","AAABMRIL","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPKE" (
  "AAAAFPKK" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETPM" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFPKJ" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKK","AAABMREL","AAABETPM","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPKO" (
  "AAAAFPKU" varchar(30) NOT NULL default ' ',
  "AAABMMRH" varchar(3) NOT NULL default '000',
  "AAABLOSZ" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPKT" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPKU","AAABMMRH","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPKX" (
  "AAABINFD" varchar(3) NOT NULL default ' ',
  "AAABLUFA" varchar(3) NOT NULL default ' ',
  "AAAAFPLH" varchar(4) NOT NULL default ' ',
  "AAAAFPLI" varchar(2) NOT NULL default '00',
  "AAAAMQSM" varchar(6) NOT NULL default '000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABILZU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD","AAABLUFA","AAAAFPLH","AAAAFPLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPLJ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEBCL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPLN" (
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAAAFTIZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABFRPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPLR" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPLT" varchar(1) NOT NULL default ' ',
  "AAAAFPLU" varchar(1) NOT NULL default ' ',
  "AAAAFPLV" varchar(3) NOT NULL default '000',
  "AAAAFPLW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPLY" (
  "AAAAFPSN" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAFPMD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFPSN","AAABJSAN","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPMH" (
  "AAABETOB" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPMK" varchar(30) NOT NULL default ' ',
  "AAAAFPML" varchar(40) NOT NULL default ' ',
  "AAAAFPMM" varchar(30) NOT NULL default ' ',
  "AAAAFPMN" varchar(40) NOT NULL default ' ',
  "AAAAFPMO" varchar(30) NOT NULL default ' ',
  "AAAAFPMP" varchar(40) NOT NULL default ' ',
  "AAAAFPMQ" varchar(30) NOT NULL default ' ',
  "AAAAFPMR" varchar(40) NOT NULL default ' ',
  "AAAAFPMS" varchar(30) NOT NULL default ' ',
  "AAAAFPMT" varchar(40) NOT NULL default ' ',
  "AAAAFPMU" varchar(30) NOT NULL default ' ',
  "AAAAFPMV" varchar(40) NOT NULL default ' ',
  "AAAAFPMW" varchar(30) NOT NULL default ' ',
  "AAAAFPMX" varchar(40) NOT NULL default ' ',
  "AAAAFPMY" varchar(30) NOT NULL default ' ',
  "AAAAFPMZ" varchar(40) NOT NULL default ' ',
  "AAAAFPNA" varchar(30) NOT NULL default ' ',
  "AAAAFPNB" varchar(40) NOT NULL default ' ',
  "AAAAFPNC" varchar(30) NOT NULL default ' ',
  "AAAAFPND" varchar(40) NOT NULL default ' ',
  "AAAAFPNE" varchar(30) NOT NULL default ' ',
  "AAAAFPNF" varchar(40) NOT NULL default ' ',
  "AAABLEJH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABETOB","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPNJ" (
  "AAABETOA" varchar(50) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETON" varchar(40) NOT NULL default ' ',
  "AAAAFQKX" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABETOB" varchar(10) NOT NULL default ' ',
  "AAABETOM" int(11) NOT NULL default '0',
  "AAABIGDF" int(11) NOT NULL default '0',
  "AAAAFQLC" int(11) NOT NULL default '0',
  "AAAAFQLD" int(11) NOT NULL default '0',
  "AAAAFQLE" int(11) NOT NULL default '0',
  "AAAAFQLF" int(11) NOT NULL default '0',
  "AAAAFQLG" int(11) NOT NULL default '0',
  "AAAAFQLH" int(11) NOT NULL default '0',
  "AAAAKASE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABETOA","AAABLMKH","AAABLIRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPOE" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABILFO" varchar(1) NOT NULL default ' ',
  "AAAAJQRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPOM" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(5) NOT NULL default '00000',
  "AAAAFPPR" varchar(10) NOT NULL default ' ',
  "AAABJUQY" smallint(6) NOT NULL default '0',
  "AAAAFPOR" varchar(4) NOT NULL default '0000',
  "AAAAFPOS" varchar(4) NOT NULL default '0000',
  "AAAAFPOT" varchar(4) NOT NULL default '0000',
  "AAAAFPOU" varchar(4) NOT NULL default '0000',
  "AAAAFPOV" varchar(4) NOT NULL default '0000',
  "AAAAFPOW" varchar(4) NOT NULL default '0000',
  "AAAAFPOX" varchar(4) NOT NULL default '0000',
  "AAAAFPOY" varchar(4) NOT NULL default '0000',
  "AAAAFPOZ" varchar(4) NOT NULL default '0000',
  "AAAAFPPA" varchar(4) NOT NULL default '0000',
  "AAAAFPPB" varchar(4) NOT NULL default '0000',
  "AAAAFPPC" varchar(4) NOT NULL default '0000',
  "AAAAFPPD" varchar(4) NOT NULL default '0000',
  "AAAAFPPE" varchar(4) NOT NULL default '0000',
  "AAAAFPPF" varchar(4) NOT NULL default '0000',
  "AAAAFPPG" varchar(4) NOT NULL default '0000',
  "AAAAFPPH" varchar(4) NOT NULL default '0000',
  "AAAAFPPI" varchar(4) NOT NULL default '0000',
  "AAAAFPPJ" varchar(4) NOT NULL default '0000',
  "AAAAFPPK" varchar(4) NOT NULL default '0000',
  "AAAAFPPL" varchar(4) NOT NULL default '0000',
  "AAAAFPPM" varchar(4) NOT NULL default '0000',
  "AAAAFPPN" varchar(4) NOT NULL default '0000',
  "AAAAFPPO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMREL","AAABMOIB","AAAAFPPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPPS" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABINMV" varchar(3) NOT NULL default ' ',
  "AAABINMW" varchar(1) NOT NULL default '0',
  "AAABINMX" varchar(3) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAFPPZ" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREL","AAABINMV","AAABINMW","AAABINMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPQF" (
  "AAAAFPSN" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAFPQI" varchar(8) NOT NULL default '00000000',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABCEDB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPSN","AAABJSAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPQQ" (
  "AAAAFPSN" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPSN","AAABJSAN","AAABEZYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPQY" (
  "AAAAFPSN" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPRC" varchar(30) NOT NULL default ' ',
  "AAAAFPVH" varchar(40) NOT NULL default ' ',
  "AAAAFPVI" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPSN","AAABJSAN","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPRI" (
  "AAAAFPRP" varchar(5) NOT NULL default ' ',
  "AAAAROOP" varchar(30) NOT NULL default ' ',
  "AAABETPA" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAFPRN" varchar(8) NOT NULL default '00000000',
  "AAAAFPRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAFPRP","AAAAROOP","AAABETPA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPRT" (
  "AAABETOA" varchar(50) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAROPT" int(11) NOT NULL default '0',
  "AAABLPQZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABETOA","AAABLMKH","AAABLIRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPSD" (
  "AAAAFPSN" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPSQ" varchar(8) NOT NULL default '00000000',
  "AAAAFPSR" varchar(6) NOT NULL default '000000',
  "AAAAFPSS" varchar(8) NOT NULL default '00000000',
  "AAAAFPST" varchar(6) NOT NULL default '000000',
  "AAABMMRM" varchar(14) NOT NULL default ' ',
  "AAAAFPSM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAFPSN","AAABJSAN","AAABMREL","AAAAFPSQ","AAAAFPSR","AAAAFPSS","AAAAFPST","AAABMMRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPSV" (
  "AAAAFPTA" varchar(30) NOT NULL default ' ',
  "AAAAFPSX" varchar(30) NOT NULL default ' ',
  "AAABLEBM" varchar(10) NOT NULL default ' ',
  "AAABBKEI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAFPTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPTB" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABDCIZ" varchar(2) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMONG","AAABLZKF","AAABFEAO","AAABDCIZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPTQ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAAUNXB" varchar(2) NOT NULL default '00',
  "AAAAUNXC" varchar(2) NOT NULL default '00',
  "AAAAUNXD" varchar(2) NOT NULL default '00',
  "AAABDSOB" varchar(2) NOT NULL default '00',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABHHTI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABEZYX"),
  KEY "AAAAFPUJ" ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPUL" (
  "AAAAFPUP" varchar(10) NOT NULL default ' ',
  "AAAAFPUQ" varchar(3) NOT NULL default '000',
  "AAABEJLB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPUP","AAAAFPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPUR" (
  "AAAAFPUU" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFPUV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFPUU","AAAAFPUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPUW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFPVD" varchar(1) NOT NULL default ' ',
  "AAAAFPUZ" varchar(8) NOT NULL default '00000000',
  "AAAAFPVA" varchar(6) NOT NULL default '000000',
  "AAABKGMO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL","AAAAFPVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPVE" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAAAFPVH" varchar(40) NOT NULL default ' ',
  "AAAAFPVI" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJSAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPVL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMREL","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPVU" (
  "AAABMIRX" varchar(40) NOT NULL default ' ',
  "AAABMIRY" varchar(5) NOT NULL default ' ',
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  "AAAARZQV" varchar(1) NOT NULL default ' ',
  "AAAAFPVZ" int(11) NOT NULL default '0',
  "AAAAFPWA" int(11) NOT NULL default '0',
  "AAAAFPWB" varchar(16) NOT NULL default '0000000000000000',
  "AAAAFPWC" int(11) NOT NULL default '0',
  "AAAAFPWD" int(11) NOT NULL default '0',
  "AAAAFPWE" double NOT NULL default '0',
  "AAAAFPWF" double NOT NULL default '0',
  "AAAAFPWG" double NOT NULL default '0',
  PRIMARY KEY ("AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPWI" (
  "AAABAMMI" varchar(8) NOT NULL default '00000000',
  "AAABMKFU" varchar(6) NOT NULL default '000000',
  "AAAAFPWL" varchar(10) NOT NULL default ' ',
  "AAAAFPWM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABAMMI","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPWP" (
  "AAABCKMB" int(11) NOT NULL auto_increment,
  "AAABMKFU" varchar(26) NOT NULL default ' ',
  "AAABJELB" varchar(22) NOT NULL default ' ',
  "AAAAFPWT" smallint(6) NOT NULL default '0',
  "AAAAVRPC" blob,
  PRIMARY KEY ("AAABCKMB","AAABMKFU"),
  KEY "AAABCKMB" ("AAABCKMB"),
  KEY "AAAAFPWZ" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPXB" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
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
  PRIMARY KEY ("AAABMREL","AAABJPZB","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPYH" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
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
  "AAAAFQUM" varchar(1) NOT NULL default ' ',
  "AAAAFQSZ" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJPZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPZO" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  "AAAAFPZR" varchar(1) NOT NULL default ' ',
  "AAAAFPZS" varchar(1) NOT NULL default ' ',
  "AAAAFPZT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFPZV" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAQLFL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFQAN" decimal(5,0) NOT NULL default '0',
  "AAABLEKH" blob,
  PRIMARY KEY ("AAABMREL","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQAC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAQLFL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFQAN" decimal(5,0) NOT NULL default '0',
  "AAABLEKH" blob,
  PRIMARY KEY ("AAABMREL","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQAJ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFQDO" varchar(14) NOT NULL default ' ',
  "AAAAQLFL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAFQAN" decimal(5,0) NOT NULL default '0',
  "AAABLEKH" blob,
  PRIMARY KEY ("AAABMREL","AAAAFQDO","AAAAQLFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQAS" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLROU" VARBINARY(255),
  "AAAAFQDO" varchar(14) NOT NULL default ' ',
  "AAAAFQCV" varchar(14) NOT NULL default ' ',
  "AAAAFQCW" varchar(14) NOT NULL default ' ',
  "AAAAVBIK" smallint(6) NOT NULL default '0',
  "AAAAKOBR" int(11) NOT NULL default '0',
  "AAAAJQOY" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDFD" smallint(6) NOT NULL default '0',
  "AAAAFQDB" tinyint(3) unsigned NOT NULL default '0',
  "AAABJCAA" varchar(1) NOT NULL default ' ',
  "AAABIGOB" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAAAFQDF" VARBINARY(255),
  "AAABDQKU" varchar(1) NOT NULL default ' ',
  "AAAAJQPL" smallint(6) NOT NULL default '0',
  "AAABIIZZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPP" smallint(6) NOT NULL default '0',
  "AAAAJQPQ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL"),
  KEY "AAABIGOB" ("AAABIGOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQBS" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLROU" VARBINARY(255),
  "AAAAFQDO" varchar(14) NOT NULL default ' ',
  "AAAAFQCV" varchar(14) NOT NULL default ' ',
  "AAAAFQCW" varchar(14) NOT NULL default ' ',
  "AAAAVBIK" smallint(6) NOT NULL default '0',
  "AAAAKOBR" int(11) NOT NULL default '0',
  "AAAAJQOY" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDFD" smallint(6) NOT NULL default '0',
  "AAAAFQDB" tinyint(3) unsigned NOT NULL default '0',
  "AAABJCAA" varchar(1) NOT NULL default ' ',
  "AAABIGOB" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAAAFQDF" VARBINARY(255),
  "AAABDQKU" varchar(1) NOT NULL default ' ',
  "AAAAJQPL" smallint(6) NOT NULL default '0',
  "AAABIIZZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPP" smallint(6) NOT NULL default '0',
  "AAAAJQPQ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQCQ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFQDO" varchar(14) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLROU" VARBINARY(255),
  "AAAAFQCV" varchar(14) NOT NULL default ' ',
  "AAAAFQCW" varchar(14) NOT NULL default ' ',
  "AAAAVBIK" smallint(6) NOT NULL default '0',
  "AAAAKOBR" int(11) NOT NULL default '0',
  "AAAAJQOY" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDFD" smallint(6) NOT NULL default '0',
  "AAAAFQDB" tinyint(3) unsigned NOT NULL default '0',
  "AAABJCAA" varchar(1) NOT NULL default ' ',
  "AAABIGOB" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAAAFQDF" VARBINARY(255),
  "AAABDQKU" varchar(1) NOT NULL default ' ',
  "AAAAJQPL" smallint(6) NOT NULL default '0',
  "AAABIIZZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAJQPP" smallint(6) NOT NULL default '0',
  "AAAAJQPQ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL","AAAAFQDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQDP" (
  "AAAAFQEB" varchar(10) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABINMV" varchar(3) NOT NULL default ' ',
  "AAABINMW" varchar(1) NOT NULL default '0',
  "AAABINMX" varchar(3) NOT NULL default ' ',
  "AAABINMN" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABIISE" varchar(12) NOT NULL default ' ',
  "AAAAFQEA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFQEB","AAABMKFH","AAABMREL","AAABINMV","AAABINMW","AAABINMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQEH" (
  "AAABFNVZ" varchar(15) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMAYY" varchar(1) NOT NULL default ' ',
  "AAABEQYH" varchar(30) NOT NULL default ' ',
  "AAAAFQEM" varchar(30) NOT NULL default ' ',
  "AAAAFQEN" varchar(30) NOT NULL default ' ',
  "AAAAFQEO" varchar(30) NOT NULL default ' ',
  "AAAAFQEP" varchar(30) NOT NULL default ' ',
  "AAABGYKT" varchar(30) NOT NULL default ' ',
  "AAAAFQER" varchar(30) NOT NULL default ' ',
  "AAAAFQES" varchar(30) NOT NULL default ' ',
  "AAAAFQET" varchar(30) NOT NULL default ' ',
  "AAAAFQEU" varchar(30) NOT NULL default ' ',
  "AAABDQKL" varchar(15) NOT NULL default ' ',
  "AAABDQKM" varchar(15) NOT NULL default ' ',
  "AAABDQKN" varchar(15) NOT NULL default ' ',
  "AAABDQKO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQFA" (
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAFQFF" decimal(5,0) NOT NULL default '0',
  "AAAAFQFG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKDLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQFI" (
  "AAABLEKR" varchar(10) NOT NULL default ' ',
  "AAABLJJN" varchar(10) NOT NULL default ' ',
  "AAAAGECV" varchar(12) NOT NULL default ' ',
  "AAABGZSJ" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  "AAAAFQFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKR","AAABLJJN","AAAAGECV","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQFT" (
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  "AAAAFQFY" decimal(5,0) NOT NULL default '0',
  "AAABMMRH" varchar(160) NOT NULL default ' ',
  PRIMARY KEY ("AAABKDLP","AAAAFQFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGDB" varchar(10) NOT NULL default ' ',
  "AAAAFQGC" varchar(30) NOT NULL default ' ',
  "AAAAFQGD" varchar(14) NOT NULL default ' ',
  "AAAAFQGE" varchar(21) NOT NULL default ' ',
  "AAAAFQGF" varchar(23) NOT NULL default ' ',
  "AAAAFQGG" varchar(6) NOT NULL default ' ',
  "AAAAFQGH" varchar(17) NOT NULL default ' ',
  "AAAAFQGI" varchar(20) NOT NULL default ' ',
  "AAAAFQGJ" varchar(20) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQGP" (
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAAAFQGR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAAAFQGX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABFEAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQHB" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAAAFQHG" varchar(1) NOT NULL default ' ',
  "AAAAFQHH" varchar(30) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQHK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAFQHT" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABDUBC" VARBINARY(255),
  "AAABKCAK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAAAFQHT","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQHV" (
  "AAABKNFD" varchar(2) NOT NULL default '00',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABKNFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAFQIK" varchar(61) NOT NULL default ' ',
  "AAABDCIZ" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABJCYJ" varchar(1) NOT NULL default ' ',
  "AAABDUBC" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAAAFQIK","AAABDCIZ","AAABMLZS","AAABJCYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFEAO" varchar(61) NOT NULL default ' ',
  "AAABDCIZ" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABAUXD" varchar(1) NOT NULL default ' ',
  "AAABDUBC" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABFEAO","AAABDCIZ","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQJB" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD","AAAAMUET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQJP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD","AAAAMUET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQKD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD","AAAAMUET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQKR" (
  "AAABETOA" varchar(50) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETON" varchar(40) NOT NULL default ' ',
  "AAAAFQKX" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABETOB" varchar(10) NOT NULL default ' ',
  "AAABETOM" int(11) NOT NULL default '0',
  "AAABIGDF" int(11) NOT NULL default '0',
  "AAAAFQLC" int(11) NOT NULL default '0',
  "AAAAFQLD" int(11) NOT NULL default '0',
  "AAAAFQLE" int(11) NOT NULL default '0',
  "AAAAFQLF" int(11) NOT NULL default '0',
  "AAAAFQLG" int(11) NOT NULL default '0',
  "AAAAFQLH" int(11) NOT NULL default '0',
  "AAAAKASE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABETOA","AAABLMKH","AAABLIRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQLM" (
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAAAMUET","AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQMA" (
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAAAMUET","AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQMO" (
  "AAAAMUET" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAAAMUEW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" decimal(5,0) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABJECS" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAAAMUET","AAABMREL","AAABIIUE","AAAAMUEW","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQNC" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAAAFQNE" varchar(1) NOT NULL default ' ',
  "AAAAFQNF" varchar(10) NOT NULL default ' ',
  "AAAAFQNG" varchar(1) NOT NULL default ' ',
  "AAABCPET" varchar(2) NOT NULL default '00',
  "AAAAFQNI" varchar(1) NOT NULL default ' ',
  "AAAAFQNJ" varchar(1) NOT NULL default ' ',
  "AAAAFQNK" varchar(1) NOT NULL default ' ',
  "AAAAFQNL" varchar(1) NOT NULL default ' ',
  "AAAAFQNM" varchar(1) NOT NULL default ' ',
  "AAAAFQNN" varchar(1) NOT NULL default ' ',
  "AAAAFQNO" varchar(1) NOT NULL default ' ',
  "AAAAFQNP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQNR" (
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAVHIH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQNW" (
  "AAAATUST" varchar(5) NOT NULL default ' ',
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATUST","AAABEZYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQOC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
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
  PRIMARY KEY ("AAABMREL","AAABJPZB","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQPI" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
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
  PRIMARY KEY ("AAABMREL","AAABJPZB","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQQO" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
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
  "AAAAFQUM" varchar(1) NOT NULL default ' ',
  "AAAAFQSZ" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJPZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQRV" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
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
  "AAAAFQUM" varchar(1) NOT NULL default ' ',
  "AAAAFQSZ" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJPZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQTC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(4) NOT NULL default '0000',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMREL","AAABJPZB","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQUJ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJPZB" varchar(1) NOT NULL default ' ',
  "AAAAFQUM" varchar(1) NOT NULL default ' ',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMREL","AAABJPZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQVQ" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFQWT" varchar(10) NOT NULL default ' ',
  "AAAAFQWW" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAAFQWV" varchar(10) NOT NULL default ' ',
  "AAAAFQWE" varchar(2) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMKXF" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFQWT"),
  KEY "AAAAFQWU" ("AAAAFQWV","AAAAFQWW","AAABJTNV"),
  KEY "AAAAFQWY" ("AAABMKXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQJM","AAABLQJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQXI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAGGOH" varchar(30) NOT NULL default ' ',
  "AAABLRFA" varchar(5) NOT NULL default '00000',
  "AAABIIDV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABEEUL" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAGGNU" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(255) NOT NULL default ' ',
  "AAAAGGNW" varchar(255) NOT NULL default ' ',
  "AAAAXLZS" VARBINARY(255),
  "AAABMBXY" VARBINARY(255),
  "AAAAGGNZ" VARBINARY(255),
  "AAAAGGOA" varchar(5) NOT NULL default '00000',
  "AAAAGGOB" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABFNHW","AAAAGGOH","AAABLRFA","AAABIIDV","AAABLFZK","AAABLEHK","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQYP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAGGOH" varchar(30) NOT NULL default ' ',
  "AAABLRFA" varchar(5) NOT NULL default '00000',
  "AAABIIDV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGQZW" varchar(1) NOT NULL default ' ',
  "AAAAGGNU" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(255) NOT NULL default ' ',
  "AAAAGGNW" varchar(255) NOT NULL default ' ',
  "AAAAXLZS" VARBINARY(255),
  "AAABMBXY" VARBINARY(255),
  "AAAAGGNZ" VARBINARY(255),
  "AAAAGGOA" varchar(5) NOT NULL default '00000',
  "AAAAGGOB" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABJDPL","AAABFNHW","AAAAGGOH","AAABLRFA","AAABIIDV","AAABLFZK","AAABLEHK","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFQZX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGGQG" VARBINARY(255) NOT NULL,
  "AAAAGGQH" smallint(6) NOT NULL default '0',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAAAGGPT" varchar(8) NOT NULL default ' ',
  "AAABAHCE" VARBINARY(255),
  "AAABKNGX" VARBINARY(255),
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAAAGGPY" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAGGQA" varchar(12) NOT NULL default ' ',
  "AAAAGGQB" decimal(15,0) NOT NULL default '0',
  "AAAAGGQC" varchar(12) NOT NULL default ' ',
  "AAAAGGQJ" decimal(15,0) NOT NULL default '0',
  "AAAAGGQD" varchar(10) NOT NULL default ' ',
  "AAAAFRBO" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGGQG"(255),"AAAAGGQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRAU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGGQG" VARBINARY(255) NOT NULL,
  "AAAAGGQH" smallint(6) NOT NULL default '0',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAGGQJ" decimal(15,0) NOT NULL default '0',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAAAGGPT" varchar(8) NOT NULL default ' ',
  "AAABAHCE" VARBINARY(255),
  "AAABKNGX" VARBINARY(255),
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAAAGGPY" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAGGQA" varchar(12) NOT NULL default ' ',
  "AAAAGGQB" decimal(15,0) NOT NULL default '0',
  "AAAAGGQC" varchar(12) NOT NULL default ' ',
  "AAAAGGQD" varchar(10) NOT NULL default ' ',
  "AAAAFRBO" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGGQG"(255),"AAAAGGQH","AAABJASN","AAAAGGQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAAAFRDY" varchar(10) NOT NULL default ' ',
  "AAAAFRDZ" varchar(4) NOT NULL default '0000',
  "AAAAFRDU" varchar(3) NOT NULL default '000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAAAWMEP" decimal(15,2) NOT NULL default '0.00',
  "AAABHFBB" decimal(15,2) NOT NULL default '0.00',
  "AAABJKYU" decimal(13,2) NOT NULL default '0.00',
  "AAABJKYT" decimal(13,2) NOT NULL default '0.00',
  "AAAAFRCN" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCO" decimal(13,2) NOT NULL default '0.00',
  "AAAAFRCP" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCR" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCS" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCT" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCU" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCV" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCW" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCX" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCY" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRCZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRDA" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRDB" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRDC" decimal(15,2) NOT NULL default '0.00',
  "AAAAFRDD" decimal(15,2) NOT NULL default '0.00',
  "AAAAGRZD" varchar(5) NOT NULL default ' ',
  "AAAAWCQH" varchar(1) NOT NULL default ' ',
  "AAAAFREK" varchar(8) NOT NULL default '00000000',
  "AAAAFRDH" varchar(8) NOT NULL default '00000000',
  "AAAAFRDI" varchar(6) NOT NULL default ' ',
  "AAABMKXF" varchar(20) NOT NULL default ' ',
  "AAABFWOU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABMCQQ","AAABENPM","AAAAFRDY","AAAAFRDZ","AAAAFRDU"),
  KEY "AAAAFRDV" ("AAABMRIL","AAABENPM","AAAAFRDY","AAAAFRDZ","AAABFWOU"),
  KEY "AAAAFREB" ("AAABMRIL","AAABFWOU","AAABMREX","AAABMDBA","AAABMDBB"),
  KEY "AAAAFREH" ("AAABMRIL","AAABMKXF","AAAAFREK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFREL" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCBJY" varchar(1) NOT NULL default ' ',
  "AAABCBJZ" varchar(1) NOT NULL default ' ',
  "AAABCBKA" varchar(1) NOT NULL default ' ',
  "AAABDEFZ" varchar(1) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABCBKD" varchar(1) NOT NULL default ' ',
  "AAABCBKE" varchar(30) NOT NULL default ' ',
  "AAABCBKF" varchar(30) NOT NULL default ' ',
  "AAABCBKG" varchar(30) NOT NULL default ' ',
  "AAABHCEC" varchar(5) NOT NULL default '00000',
  "AAABCBKI" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABLWWP","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABJTNV" varchar(77) NOT NULL default ' ',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAAAGCCC" varchar(10) NOT NULL default '0000000000',
  "AAAAFRFM" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLMHB","AAABJTNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRFW" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAAAFRFW" varchar(200) NOT NULL default ' ',
  "AAAAFRFX" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRFZ" (
  "AAAAFRGE" varchar(40) NOT NULL default ' ',
  "AAAAFRGB" varchar(40) NOT NULL default ' ',
  "AAAAFRGC" varchar(14) NOT NULL default ' ',
  "AAAAFRGD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRGF" (
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRGJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMNYU" varchar(15) NOT NULL default ' ',
  "AAABJXDX" varchar(30) NOT NULL default ' ',
  "AAAAVEEK" varchar(10) NOT NULL default ' ',
  "AAAAVEEL" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(25) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAAAVEEP" varchar(30) NOT NULL default ' ',
  "AAAATWEX" varchar(1) NOT NULL default ' ',
  "AAABIXVF" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGRXB" varchar(40) NOT NULL default ' ',
  "AAAATQKZ" varchar(25) NOT NULL default ' ',
  "AAAAFRHG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV"),
  KEY "AAAAFRHJ" ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRHM" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAFRHO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRHQ" (
  "AAABKNBX" varchar(30) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABHRMU" varchar(75) NOT NULL default ' ',
  "AAAAFRHU" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABLUNI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNBX","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRIA" (
  "AAABKNBX" varchar(30) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  "AAAAFRIE" varchar(75) NOT NULL default ' ',
  "AAAAFRIF" varchar(2) NOT NULL default ' ',
  "AAAAFRIG" varchar(75) NOT NULL default ' ',
  "AAAAFRIH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNBX","AAABGWVP","AAABGWVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRIL" (
  "AAABHEZT" varchar(4) NOT NULL default ' ',
  "AAABDZHO" varchar(10) NOT NULL default ' ',
  "AAABHYAG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRIQ" (
  "AAABHEZT" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEZT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRIW" (
  "AAAAVJHP" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAAAFRJI" varchar(10) NOT NULL default ' ',
  "AAAAUXIU" varchar(32) NOT NULL default ' ',
  "AAAAFRJK" varchar(1) NOT NULL default ' ',
  "AAAAFRJL" varchar(20) NOT NULL default ' ',
  "AAAAFSMN" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAVJHP","AAABIIUG"),
  KEY "AAAAFRJS" ("AAAAFRWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRJU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAVJHP" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAVJHP","AAABIIUG"),
  KEY "AAAAFRKF" ("AAAAVJHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRKH" (
  "AAABHECV" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAFRKQ" varchar(20) NOT NULL default ' ',
  "AAAAFSPH" varchar(1) NOT NULL default ' ',
  "AAAAFRKS" varchar(1) NOT NULL default ' ',
  "AAAAFSPI" varchar(1) NOT NULL default ' ',
  "AAAAFRKU" varchar(1) NOT NULL default ' ',
  "AAAAFSPJ" varchar(1) NOT NULL default ' ',
  "AAAAFRKW" varchar(1) NOT NULL default ' ',
  "AAABHLCF" varchar(1) NOT NULL default ' ',
  "AAAAFSOE" varchar(1) NOT NULL default ' ',
  "AAAAMUEJ" varchar(1) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABGUFO" varchar(1) NOT NULL default ' ',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAFRLD" varchar(4) NOT NULL default ' ',
  "AAAAFRLE" varchar(40) NOT NULL default ' ',
  "AAABDDPD" varchar(10) NOT NULL default ' ',
  "AAAAFRLG" varchar(32) NOT NULL default ' ',
  "AAABLECD" varchar(10) NOT NULL default ' ',
  "AAAAFRVG" varchar(1) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAAAFRLL" varchar(1) NOT NULL default ' ',
  "AAAAFRLM" varchar(8) NOT NULL default ' ',
  "AAAAFRVF" varchar(1) NOT NULL default ' ',
  "AAAAFSMN" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  "AAAAFRLS" varchar(1) NOT NULL default ' ',
  "AAAAUGRQ" VARBINARY(255),
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHECV","AAABIIUG"),
  KEY "AAAAFRLX" ("AAAAFRWZ"),
  KEY "AAAAFRLZ" ("AAAAUGRQ"(255)),
  KEY "AAAAFRMB" ("AAABHECV","AAABIIUG","AAAAUGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRMF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABHECV" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAABHECV","AAABIIUG"),
  KEY "AAAAFRMQ" ("AAABHECV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRMS" (
  "AAAAFRNS" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAAAFSAW" varchar(1) NOT NULL default ' ',
  "AAAAFSAX" varchar(20) NOT NULL default ' ',
  "AAAAFSBD" varchar(1) NOT NULL default ' ',
  "AAAAFSBE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRNS","AAABIIUG"),
  KEY "AAAAFRNK" ("AAAAFRWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRNM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFRNS" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFRNS","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRNU" (
  "AAAAFROW" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABDDPD" varchar(10) NOT NULL default ' ',
  "AAAAFROP" varchar(26) NOT NULL default ' ',
  "AAAAFROF" varchar(1) NOT NULL default ' ',
  "AAAAFROG" varchar(1) NOT NULL default ' ',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFROW","AAABIIUG"),
  KEY "AAAAFROM" ("AAAAFRWZ"),
  KEY "AAAAFROO" ("AAAAFROP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFROQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFROW" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFROW","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFROY" (
  "AAAARPDR" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAARPDR","AAABIIUG"),
  KEY "AAAAFRPM" ("AAAAFRWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRPO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARPDR" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARPDR","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRPW" (
  "AAAAFRRC" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAAAFSAW" varchar(1) NOT NULL default ' ',
  "AAAAFSAX" varchar(20) NOT NULL default ' ',
  "AAAAFSBD" varchar(1) NOT NULL default ' ',
  "AAAAFSBE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRRC","AAABIIUG"),
  KEY "AAAAFRQO" ("AAAAFRWZ"),
  KEY "AAAAFRQQ" ("AAAAFSAW","AAAAFSAX"),
  KEY "AAAAFRQT" ("AAAAFSBD","AAAAFSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRQW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFRRC" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFRRC","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRRE" (
  "AAAAUSEW" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAAAFRRW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUSEW","AAABIIUG"),
  KEY "AAAAFRRT" ("AAAAFRWZ"),
  KEY "AAAAFRRV" ("AAAAFRRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRRX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUSEW" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAAAFRSC" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUSEW","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRSG" (
  "AAAAFRTI" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAFRSP" varchar(20) NOT NULL default ' ',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAABFDXL" varchar(1) NOT NULL default ' ',
  "AAAAFRTA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRTI","AAABIIUG"),
  KEY "AAAAFRSX" ("AAAAFRWZ"),
  KEY "AAAAFRSZ" ("AAAAFRTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRTB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFRTI" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFRTI","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRTK" (
  "AAAAFRUH" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABFDXL" varchar(1) NOT NULL default ' ',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRUH","AAABIIUG"),
  KEY "AAAAFRTZ" ("AAAAFRWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRUB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFRUH" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFRUH","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRUJ" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFRUQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAABIIUG"),
  KEY "AAAAFRUP" ("AAAAFRUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRUR" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  "AAABHRMU" varchar(24) NOT NULL default ' ',
  "AAAAFRVX" varchar(20) NOT NULL default ' ',
  "AAAAFRVF" varchar(1) NOT NULL default ' ',
  "AAAAFRVG" varchar(1) NOT NULL default ' ',
  "AAAAFRVH" varchar(1) NOT NULL default ' ',
  "AAAAFRVI" varchar(1) NOT NULL default ' ',
  "AAABGPVW" varchar(4) NOT NULL default ' ',
  "AAAAFRVK" varchar(1) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  "AAABEFZP" varchar(12) NOT NULL default ' ',
  "AAABEFZT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAABIIUG"),
  KEY "AAAAFRVS" ("AAAAFRWZ"),
  KEY "AAAAFRVU" ("AAABHRMU"),
  KEY "AAAAFRVW" ("AAAAFRVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRVY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAABEPTB","AAABIIUG"),
  KEY "AAAAFRWI" ("AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRWK" (
  "AAAAFRXG" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFRWZ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFRXG","AAABIIUG"),
  KEY "AAAAFRWY" ("AAAAFRWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRXA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFRXG" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFRXG","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRXI" (
  "AAABIFGN" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAABLQKV" varchar(2) NOT NULL default ' ',
  "AAABJZVA" varchar(20) NOT NULL default ' ',
  "AAAAFSMN" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABIFGN","AAABIIUG"),
  KEY "AAAAFRYC" ("AAABJZVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRYE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIFGN" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIFGN","AAABIIUG"),
  KEY "AAAAFRYN" ("AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRYP" (
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRZD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMLXR"),
  KEY "AAAAFRZK" ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFRZM" (
  "AAAAFSAO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAFSAO","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSAC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAFSAO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAFSAO","AAABIIUG"),
  KEY "AAAAFSAN" ("AAAAFSAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSAP" (
  "AAAAFSAW" varchar(4) NOT NULL default ' ',
  "AAAAFSAX" varchar(30) NOT NULL default ' ',
  "AAAAFSBD" varchar(4) NOT NULL default ' ',
  "AAAAFSBE" varchar(30) NOT NULL default ' ',
  "AAAAFSHZ" varchar(3) NOT NULL default '000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSAW","AAAAFSAX","AAAAFSBD","AAAAFSBE","AAAAFSHZ","AAABIIUG"),
  KEY "AAAAFSBC" ("AAAAFSBD","AAAAFSBE","AAAAFSHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSBG" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAABKMWI" varchar(2) NOT NULL default '00',
  "AAABLENG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAABKMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSBM" (
  "AAABMRHK" varchar(10) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAFSIX" varchar(2) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAAFSLG" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(200) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRHK","AAAAVIRJ","AAAAGGKJ","AAAAFSIX","AAABIIUG","AAABKMLY","AAABKMLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSCJ" (
  "AAAAFSDA" varchar(32) NOT NULL default ' ',
  "AAAAFSFB" varchar(4) NOT NULL default ' ',
  "AAAAFSFC" varchar(32) NOT NULL default ' ',
  "AAAAFSIX" varchar(4) NOT NULL default ' ',
  "AAAARKJB" varchar(32) NOT NULL default ' ',
  "AAAAFSEZ" varchar(4) NOT NULL default ' ',
  "AAAAFSFA" varchar(32) NOT NULL default ' ',
  "AAAAFSEI" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAFSEJ" varchar(10) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSDA","AAAAFSFB","AAAAFSFC","AAAAFSIX","AAAARKJB","AAAAFSEZ","AAAAFSFA"),
  KEY "AAAAFSDH" ("AAAAFSFB","AAAAFSIX","AAAARKJB"),
  KEY "AAAAFSDL" ("AAAAFSFB","AAAAFSFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSDO" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(20) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLRQO" varchar(32) NOT NULL default ' ',
  "AAABKWRQ" varchar(10) NOT NULL default ' ',
  "AAABEURS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLTWR","AAABLOZE","AAABLRQO","AAABKWRQ","AAABEURS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSEB" (
  "AAAAFSIX" varchar(4) NOT NULL default ' ',
  "AAAARKJB" varchar(20) NOT NULL default ' ',
  "AAAAFSEZ" varchar(4) NOT NULL default ' ',
  "AAAAFSFA" varchar(20) NOT NULL default ' ',
  "AAAAFSFB" varchar(4) NOT NULL default ' ',
  "AAAAFSFC" varchar(32) NOT NULL default ' ',
  "AAAAFSEI" varchar(1) NOT NULL default ' ',
  "AAAAFSEJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSIX","AAAARKJB","AAAAFSEZ","AAAAFSFA","AAAAFSFB","AAAAFSFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSEQ" (
  "AAAAFSIX" varchar(4) NOT NULL default ' ',
  "AAAARKJB" varchar(20) NOT NULL default ' ',
  "AAAAFSEZ" varchar(4) NOT NULL default ' ',
  "AAAAFSFA" varchar(20) NOT NULL default ' ',
  "AAAAFSFB" varchar(4) NOT NULL default ' ',
  "AAAAFSFC" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSIX","AAAARKJB","AAAAFSEZ","AAAAFSFA","AAAAFSFB","AAAAFSFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSFD" (
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFSIX" varchar(2) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAAAFSFP" varchar(1) NOT NULL default ' ',
  "AAAAFSFQ" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(1) NOT NULL default ' ',
  "AAAAXLXX" varchar(1) NOT NULL default ' ',
  "AAAAREAZ" varchar(1) NOT NULL default ' ',
  "AAAAFSFU" varchar(1) NOT NULL default ' ',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAAAFSFW" varchar(1) NOT NULL default ' ',
  "AAABIIDD" varchar(1) NOT NULL default ' ',
  "AAAAFSMN" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABMLXR" VARBINARY(255),
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  "AAAAUGRQ" VARBINARY(255),
  "AAABMRHK" VARBINARY(255),
  "AAABGAQW" varchar(20) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGGKJ","AAAAVIRJ","AAABIIUG","AAAAFSIX"),
  KEY "AAAAFSGK" ("AAABGAQW"),
  KEY "AAAAFSGM" ("AAABMRHK"(255),"AAAAUGRQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSGP" (
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFSIX" varchar(2) NOT NULL default ' ',
  "AAABMRHK" varchar(10) NOT NULL default ' ',
  "AAAAFSLG" varchar(20) NOT NULL default ' ',
  "AAAAFSGW" varchar(10) NOT NULL default ' ',
  "AAAAFSHQ" varchar(2) NOT NULL default ' ',
  "AAAAFSIA" varchar(1) NOT NULL default ' ',
  "AAAATXWY" varchar(10) NOT NULL default ' ',
  "AAABDPTA" varchar(5) NOT NULL default '00000',
  "AAABDPTC" varchar(5) NOT NULL default '00000',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAAAFSOE" varchar(1) NOT NULL default ' ',
  "AAABIYCS" VARBINARY(255),
  PRIMARY KEY ("AAAAGGKJ","AAAAVIRJ","AAABIIUG","AAAAFSIX","AAABMRHK"),
  KEY "AAAAFSHL" ("AAABIYCS"(255)),
  KEY "AAAAFSHN" ("AAAAFSIX","AAAAFSLG","AAAAFSHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSHR" (
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFSIX" varchar(2) NOT NULL default ' ',
  "AAAAFSIY" varchar(10) NOT NULL default ' ',
  "AAAAFSLG" varchar(20) NOT NULL default ' ',
  "AAAAFSHY" varchar(10) NOT NULL default ' ',
  "AAAAFSHZ" varchar(3) NOT NULL default '000',
  "AAAAFSIA" varchar(1) NOT NULL default ' ',
  "AAAAVLZN" varchar(10) NOT NULL default ' ',
  "AAAAVHTM" varchar(10) NOT NULL default ' ',
  "AAABDPTA" varchar(5) NOT NULL default '00000',
  "AAABDPTC" varchar(5) NOT NULL default '00000',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGGKJ","AAAAVIRJ","AAABIIUG","AAAAFSIX","AAAAFSIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSIM" (
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAFSIX" varchar(2) NOT NULL default ' ',
  "AAAAFSIY" varchar(10) NOT NULL default ' ',
  "AAAAFSIZ" varchar(5) NOT NULL default '00000',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABDPTA" varchar(5) NOT NULL default '00000',
  "AAABDPTC" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAVIRJ","AAAAGGKJ","AAAAFSIX","AAAAFSIY","AAAAFSIZ","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSJB" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABMLXR" VARBINARY(255),
  "AAAAUGRQ" VARBINARY(255),
  "AAABMRHK" VARBINARY(255),
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAFSJW" varchar(20) NOT NULL default ' ',
  "AAAAFSJY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGZO","AAAAFTAZ","AAABIIUG"),
  KEY "AAAAFSJV" ("AAAAFSJW"),
  KEY "AAAAFSJX" ("AAAAFSJY"),
  KEY "AAAAFSJZ" ("AAABMRHK"(255)),
  KEY "AAAAFSKB" ("AAAAUGRQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSKD" (
  "AAAAFSLO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFSKG" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFSKQ" varchar(1) NOT NULL default ' ',
  "AAAAFSKR" VARBINARY(255),
  "AAAAFSKS" varchar(1) NOT NULL default ' ',
  "AAAAFSKT" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSLO","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSKY" (
  "AAAAFSLO" varchar(20) NOT NULL default ' ',
  "AAAALBQC" varchar(1) NOT NULL default ' ',
  "AAAAFSLG" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAARNJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSLO","AAAALBQC","AAAAFSLG","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSLI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFSLO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFSLO","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSLQ" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMNMP" int(11) NOT NULL default '0',
  "AAAAFSLU" VARBINARY(255),
  PRIMARY KEY ("AAABHGZO","AAABIIUG","AAABMNMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSLY" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABJZVA" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAFSML" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAAAFSMN" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGZO","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSMR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGZO","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSNA" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFSNV" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAAAFSNW" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  "AAAAVEPU" varchar(1) NOT NULL default ' ',
  "AAAAFSOG" varchar(1) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGZO","AAAAFSNV","AAAAFTAZ","AAAAFSNW","AAABIIUG"),
  KEY "AAAAFSNP" ("AAAAFTAZ","AAAAFSNV","AAAAFSNW"),
  KEY "AAAAFSNT" ("AAABHGZO","AAAAFSNV","AAAAFSNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSNX" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAAAJNNT" varchar(2) NOT NULL default ' ',
  "AAABGLID" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABHSZC" varchar(1) NOT NULL default ' ',
  "AAAAFSOE" varchar(1) NOT NULL default ' ',
  "AAAAVEPU" varchar(1) NOT NULL default ' ',
  "AAAAFSOG" varchar(1) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGZO","AAAAFTAZ","AAAAJNNT","AAABGLID","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSON" (
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABGAQW" varchar(20) NOT NULL default ' ',
  "AAAAQGUG" varchar(4) NOT NULL default ' ',
  "AAABHETT" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAAAFSPC" varchar(20) NOT NULL default ' ',
  "AAAAFSPD" varchar(20) NOT NULL default ' ',
  "AAAAFSPE" varchar(2) NOT NULL default ' ',
  "AAAAFSPF" varchar(4) NOT NULL default ' ',
  "AAAAFSPG" varchar(20) NOT NULL default ' ',
  "AAAAFSPH" varchar(1) NOT NULL default ' ',
  "AAAAFSPI" varchar(1) NOT NULL default ' ',
  "AAAAFSPJ" varchar(1) NOT NULL default ' ',
  "AAABHLCF" varchar(1) NOT NULL default ' ',
  "AAAAMUEJ" varchar(1) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAFSPO" varchar(3) NOT NULL default '000',
  "AAAAFSPP" varchar(3) NOT NULL default '000',
  "AAAAFSPQ" varchar(3) NOT NULL default '000',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  "AAAAUGRQ" VARBINARY(255),
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGZO","AAAAFTAZ","AAABIIUG"),
  KEY "AAAAFSPY" ("AAABGAQW","AAAAQGUG","AAABHGZO"),
  KEY "AAAAFSQC" ("AAABMLXR"),
  KEY "AAAAFSQE" ("AAAAUGRQ"(255)),
  KEY "AAAAFSQG" ("AAABHGZO","AAAAFTAZ","AAABIIUG","AAAAUGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSQL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(100) NOT NULL default ' ',
  "AAABHEAD" varchar(1) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAABHGZO","AAAAFTAZ","AAABIIUG"),
  KEY "AAAAFSQZ" ("AAAAUQRO","AAABHGZO","AAAAFTAZ","AAABIIUG"),
  KEY "AAAAFSRE" ("AAABHGZO","AAAAFTAZ","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSRI" (
  "AAAAFSRO" varchar(32) NOT NULL default ' ',
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLTWR" varchar(30) NOT NULL default ' ',
  "AAAAFSRN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFSRO","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSRQ" (
  "AAABLITM" varchar(64) NOT NULL default ' ',
  "AAABAHXF" varchar(1) NOT NULL default ' ',
  "AAAAFSRT" varchar(255) NOT NULL default ' ',
  "AAABILUK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM","AAABAHXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSRX" (
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABEMDR" varchar(3) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLKIV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPLT","AAABEMDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSSE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(40) NOT NULL default ' ',
  "AAABJTGS" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSSM" (
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGGKJ","AAAAVIRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSSU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAAAFSSY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVJ","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAAAFSTR" varchar(4) NOT NULL default ' ',
  "AAAAFSTS" varchar(3) NOT NULL default '000',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAAAFSTU" varchar(3) NOT NULL default '000',
  "AAAAFSTV" varchar(5) NOT NULL default '00000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAFSTN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAAAWUOF","AAAAFSTR","AAAAFSTS","AAABEPKB","AAAAFSTU","AAAAFSTV","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLTXI" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAAWUOF","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAAAGJHX" varchar(10) NOT NULL default ' ',
  "AAAAFSYO" varchar(10) NOT NULL default ' ',
  "AAABKSUB" varchar(12) NOT NULL default ' ',
  "AAAAFSYP" varchar(20) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAAAFSUW" varchar(10) NOT NULL default ' ',
  "AAAAFSUX" varchar(10) NOT NULL default ' ',
  "AAABEPKH" varchar(30) NOT NULL default ' ',
  "AAABEPKI" varchar(30) NOT NULL default ' ',
  "AAABEPGQ" varchar(4) NOT NULL default ' ',
  "AAAAYGXV" varchar(32) NOT NULL default ' ',
  "AAABEPGR" varchar(4) NOT NULL default ' ',
  "AAAAYGXW" varchar(32) NOT NULL default ' ',
  "AAABEPGS" varchar(4) NOT NULL default ' ',
  "AAAATPAC" varchar(32) NOT NULL default ' ',
  "AAABEPGT" varchar(4) NOT NULL default ' ',
  "AAAATPAD" varchar(32) NOT NULL default ' ',
  "AAAAFSVI" varchar(50) NOT NULL default ' ',
  "AAAAFSVJ" varchar(1) NOT NULL default ' ',
  "AAAAFSVK" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABLXEL" varchar(1) NOT NULL default ' ',
  "AAAAFSXQ" varchar(8) NOT NULL default '00000000',
  "AAAAFSXO" varchar(8) NOT NULL default '00000000',
  "AAAAFSXP" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFSXV" varchar(5) NOT NULL default ' ',
  "AAAAFSXW" decimal(13,2) NOT NULL default '0.00',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJMGZ" varchar(4) NOT NULL default ' ',
  "AAAAFSYD" varchar(8) NOT NULL default '00000000',
  "AAAAFSYE" varchar(6) NOT NULL default ' ',
  "AAAAFSWA" varchar(1) NOT NULL default ' ',
  "AAAAFSWB" varchar(1) NOT NULL default ' ',
  "AAAAPNPY" varchar(1) NOT NULL default ' ',
  "AAAAFSWD" varchar(3) NOT NULL default ' ',
  "AAAAFSWE" varchar(6) NOT NULL default ' ',
  "AAAAFSWF" varchar(4) NOT NULL default ' ',
  "AAAAFSWG" varchar(10) NOT NULL default ' ',
  "AAAAFSWH" varchar(4) NOT NULL default '0000',
  "AAAAFSWI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAAAWUOF"),
  KEY "AAAAFSWM" ("AAABEPQQ","AAAAYGXV"),
  KEY "AAAAFSWP" ("AAABMRIL","AAAAGJHX","AAABKSUB","AAABEPQQ"),
  KEY "AAAAFSWU" ("AAABMRIL","AAABKSUB","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAABEDJB" varchar(50) NOT NULL default ' ',
  "AAAAFSXC" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWUOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAAAGJHX" varchar(10) NOT NULL default ' ',
  "AAABKSUB" varchar(12) NOT NULL default ' ',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAAAFSYO" varchar(10) NOT NULL default ' ',
  "AAAAFSYP" varchar(20) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAFSXO" varchar(8) NOT NULL default '00000000',
  "AAAAFSXP" varchar(6) NOT NULL default '000000',
  "AAAAFSXQ" varchar(8) NOT NULL default '00000000',
  "AAAAFSYS" varchar(4) NOT NULL default '0000',
  "AAABEPOT" varchar(1) NOT NULL default ' ',
  "AAAAYBHY" varchar(2) NOT NULL default '00',
  "AAAAFSXU" varchar(1) NOT NULL default ' ',
  "AAAAFSXV" varchar(5) NOT NULL default ' ',
  "AAAAFSXW" decimal(13,2) NOT NULL default '0.00',
  "AAAAFSXX" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPNH" varchar(2) NOT NULL default ' ',
  "AAABEPGX" varchar(1) NOT NULL default ' ',
  "AAAAFSYB" varchar(8) NOT NULL default '00000000',
  "AAAAFSYC" varchar(8) NOT NULL default '00000000',
  "AAAAFSYD" varchar(8) NOT NULL default '00000000',
  "AAAAFSYE" varchar(6) NOT NULL default ' ',
  "AAAAPNPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAAAGJHX","AAABKSUB","AAAAWUOF"),
  KEY "AAAAFSYL" ("AAABEPQQ","AAABKSUB","AAAAFSYO","AAAAFSYP"),
  KEY "AAAAFSYQ" ("AAABEPQQ","AAAAFSYS","AAABEPOT","AAAAYBHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAAAFSYY" varchar(8) NOT NULL default '00000000',
  "AAAAFSYZ" varchar(8) NOT NULL default '00000000',
  "AAAAFSZA" varchar(4) NOT NULL default ' ',
  "AAAAZHVU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAWUOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAAAWUOF" varchar(36) NOT NULL default ' ',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAAAWUOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFSZN" (
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
CREATE TABLE "AAAAFTAF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(20) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGGKJ","AAAAVIRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTAP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHGZO" varchar(20) NOT NULL default ' ',
  "AAAAFTAZ" varchar(20) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLCOV" varchar(3) NOT NULL default ' ',
  "AAABLCOW" varchar(2) NOT NULL default ' ',
  "AAABLCOX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHGZO","AAAAFTAZ","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAAAQUSX" varchar(1) NOT NULL default ' ',
  "AAABHKSE" int(11) NOT NULL default '0',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAFTDW" varchar(1) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAAAFTBP" int(11) NOT NULL default '0',
  "AAAAFTBQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTHM","AAABLTHN","AAAAQUSX","AAABHKSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAAAQUSX" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(8) NOT NULL default '00000000',
  "AAABFLGW" varchar(1) NOT NULL default ' ',
  "AAABCLZF" varchar(4) NOT NULL default '0000',
  "AAAAPJYD" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTHM","AAABLTHN","AAAAQUSX","AAABMRCU","AAABFLGW","AAABCLZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFTDB" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAAAQUSX" varchar(1) NOT NULL default ' ',
  "AAABEQNC" varchar(2) NOT NULL default ' ',
  "AAAAFTCV" varchar(24) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFTDB","AAABLUSV"),
  KEY "AAAAFTDA" ("AAAAFTDB","AAABLFEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAFTDW" varchar(1) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(8) NOT NULL default '00000000',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP","AAABENPM","AAABJWRR","AAABMGVL","AAABMCTJ","AAABKWCV","AAAAFTDW","AAABJRIC","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTDZ" (
  "AAABKYIH" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKYIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTED" (
  "AAABMKFH" varchar(6) NOT NULL default ' ',
  "AAABLTYE" varchar(80) NOT NULL default ' ',
  "AAABLTYF" varchar(20) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAFTEI" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMKFH","AAABLTYE","AAABLTYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTEM" (
  "AAAAFTES" varchar(4) NOT NULL default ' ',
  "AAAAFTEO" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJSPH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFTES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTET" (
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
  KEY "AAAAFTGJ" ("AAABJAAI"),
  KEY "AAAAFTGL" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAFTGO" ("AAABLMUZ","AAABMRHK"),
  KEY "AAAAFTGR" ("AAABLMUZ","AAABLFYJ"),
  KEY "AAAAFTGU" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAFTGX" ("AAABHZOJ","AAABHZOR"),
  KEY "AAAAFTHA" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTHD" (
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
  KEY "AAAAFTHZ" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTIC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAFTIQ" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAFTIT" ("AAABMRHS","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTIX" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  "AAAAFTIZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAAAFTXI" varchar(1) NOT NULL default ' ',
  "AAAAFTXJ" varchar(1) NOT NULL default ' ',
  "AAABDZYW" varchar(3) NOT NULL default ' ',
  "AAABDZWY" varchar(4) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAAAFTXP" varchar(6) NOT NULL default ' ',
  "AAAAFTXQ" varchar(4) NOT NULL default '0000',
  "AAABERTL" varchar(4) NOT NULL default '0000',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  "AAAAFTXV" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTXW" varchar(3) NOT NULL default ' ',
  "AAAAFTXX" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTXY" varchar(3) NOT NULL default ' ',
  "AAAAFTXZ" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYA" varchar(3) NOT NULL default ' ',
  "AAAAFTYB" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYC" varchar(3) NOT NULL default ' ',
  "AAAAFTYD" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYE" varchar(3) NOT NULL default ' ',
  "AAABDZNC" varchar(1) NOT NULL default ' ',
  "AAABDZQX" varchar(4) NOT NULL default ' ',
  "AAABDZRK" varchar(4) NOT NULL default ' ',
  "AAAAFTYS" varchar(7) NOT NULL default ' ',
  "AAAAFTYT" varchar(7) NOT NULL default ' ',
  "AAAAFTYU" varchar(7) NOT NULL default ' ',
  "AAAAFTYV" varchar(7) NOT NULL default ' ',
  "AAAAFTYW" varchar(4) NOT NULL default ' ',
  "AAAAFTYX" varchar(4) NOT NULL default ' ',
  "AAAAFTYY" varchar(3) NOT NULL default '000',
  "AAAAFTYZ" varchar(3) NOT NULL default '000',
  "AAAAFTZA" varchar(3) NOT NULL default '000',
  "AAAAFTZB" varchar(3) NOT NULL default '000',
  "AAAAFTZC" varchar(3) NOT NULL default '000',
  "AAAAFTZD" varchar(3) NOT NULL default '000',
  "AAAAFTZE" varchar(3) NOT NULL default '000',
  "AAAAFTZF" varchar(3) NOT NULL default '000',
  "AAAAFTZG" varchar(3) NOT NULL default '000',
  "AAAAFTZH" varchar(3) NOT NULL default '000',
  "AAAAFTZI" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTZJ" varchar(3) NOT NULL default ' ',
  "AAABESAI" varchar(25) NOT NULL default ' ',
  "AAABERYE" varchar(3) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  "AAAAFTZN" varchar(4) NOT NULL default ' ',
  "AAAAFTZO" varchar(4) NOT NULL default ' ',
  "AAAAFTZP" varchar(4) NOT NULL default ' ',
  "AAAAFTZQ" varchar(4) NOT NULL default ' ',
  "AAAAFTZR" varchar(4) NOT NULL default ' ',
  "AAAAFTZS" varchar(4) NOT NULL default ' ',
  "AAAAFTZT" varchar(4) NOT NULL default ' ',
  "AAAAFTZU" varchar(7) NOT NULL default ' ',
  "AAAAFTZV" varchar(7) NOT NULL default ' ',
  "AAAAFTZW" varchar(7) NOT NULL default ' ',
  "AAAAFTZX" varchar(7) NOT NULL default ' ',
  "AAAAFTZY" varchar(7) NOT NULL default ' ',
  "AAAAFTZZ" varchar(7) NOT NULL default ' ',
  "AAAAFUAA" varchar(7) NOT NULL default ' ',
  "AAAAFUAB" varchar(7) NOT NULL default ' ',
  "AAAAFUAC" varchar(7) NOT NULL default ' ',
  "AAAAFUAD" varchar(7) NOT NULL default ' ',
  "AAAAGWRW" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRX" varchar(3) NOT NULL default ' ',
  "AAAAGWRY" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRZ" varchar(3) NOT NULL default ' ',
  "AAAAFUAX" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUAY" varchar(3) NOT NULL default ' ',
  "AAAAFUAZ" varchar(1) NOT NULL default ' ',
  "AAAAFUBA" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBB" varchar(3) NOT NULL default ' ',
  "AAAAFUBC" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBD" varchar(3) NOT NULL default ' ',
  "AAAAGWRP" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRQ" varchar(3) NOT NULL default ' ',
  "AAAAGWRR" varchar(4) NOT NULL default '0000',
  "AAAAFUBH" varchar(1) NOT NULL default ' ',
  "AAAAFUBI" varchar(1) NOT NULL default ' ',
  "AAAAFUBJ" varchar(1) NOT NULL default ' ',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAAAGWTI" varchar(1) NOT NULL default ' ',
  "AAAAFUBM" varchar(4) NOT NULL default ' ',
  "AAAAFUBN" varchar(10) NOT NULL default ' ',
  "AAAAFUBO" decimal(12,2) NOT NULL default '0.00',
  "AAAAFUBP" varchar(3) NOT NULL default ' ',
  "AAAAFUBQ" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBR" varchar(3) NOT NULL default ' ',
  "AAAAFUBS" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBT" varchar(3) NOT NULL default ' ',
  "AAAAFUBU" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBV" varchar(3) NOT NULL default ' ',
  "AAAAFUBW" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBX" varchar(3) NOT NULL default ' ',
  "AAAAFUBY" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBZ" varchar(3) NOT NULL default ' ',
  "AAAAFUCA" decimal(9,2) NOT NULL default '0.00',
  "AAAAFUCB" varchar(3) NOT NULL default ' ',
  "AAAAFUCC" decimal(10,2) NOT NULL default '0.00',
  "AAAAFUCD" varchar(3) NOT NULL default ' ',
  "AAAAFUCF" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCG" varchar(3) NOT NULL default ' ',
  "AAAAFUCH" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCI" varchar(3) NOT NULL default ' ',
  "AAAAFUCJ" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCK" varchar(3) NOT NULL default ' ',
  "AAABERYF" varchar(1) NOT NULL default ' ',
  "AAAAGWRV" varchar(5) NOT NULL default ' ',
  "AAAAFUCN" varchar(2) NOT NULL default ' ',
  "AAAAGWRT" varchar(25) NOT NULL default ' ',
  "AAAAGWSA" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSB" varchar(3) NOT NULL default ' ',
  "AAAAGWSC" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSD" varchar(3) NOT NULL default ' ',
  "AAAAMJSD" varchar(40) NOT NULL default ' ',
  "AAAAFUCU" varchar(1) NOT NULL default ' ',
  "AAABAWWL" varchar(1) NOT NULL default ' ',
  "AAAAFUCZ" varchar(4) NOT NULL default ' ',
  "AAAAFTSO" varchar(21) NOT NULL default ' ',
  "AAAAHHJY" varchar(21) NOT NULL default ' ',
  "AAAAHHJZ" varchar(21) NOT NULL default ' ',
  "AAAAFTSR" varchar(21) NOT NULL default ' ',
  "AAAAFTSS" varchar(21) NOT NULL default ' ',
  "AAAAFTST" varchar(21) NOT NULL default ' ',
  "AAAAFTSU" varchar(21) NOT NULL default ' ',
  "AAAAFTSV" varchar(21) NOT NULL default ' ',
  "AAAAFTSW" varchar(21) NOT NULL default ' ',
  "AAAAFTSX" varchar(21) NOT NULL default ' ',
  "AAAAFTSY" varchar(21) NOT NULL default ' ',
  "AAAAFTSZ" varchar(21) NOT NULL default ' ',
  "AAAAFTTA" varchar(21) NOT NULL default ' ',
  "AAAAFTTB" varchar(21) NOT NULL default ' ',
  "AAAAFTTC" varchar(21) NOT NULL default ' ',
  "AAAAFTTD" varchar(21) NOT NULL default ' ',
  "AAAAFTTE" varchar(21) NOT NULL default ' ',
  "AAAAFTTF" varchar(21) NOT NULL default ' ',
  "AAAAFTTG" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTTH" varchar(3) NOT NULL default ' ',
  "AAAAFTTI" varchar(21) NOT NULL default ' ',
  "AAAAFTTJ" varchar(21) NOT NULL default ' ',
  "AAAAFTTK" varchar(21) NOT NULL default ' ',
  "AAAAFTTL" varchar(21) NOT NULL default ' ',
  "AAAAFTTM" varchar(21) NOT NULL default ' ',
  "AAAAFTTN" varchar(21) NOT NULL default ' ',
  "AAAAFTTO" varchar(21) NOT NULL default ' ',
  "AAAAFTTP" varchar(21) NOT NULL default ' ',
  "AAAAFTTQ" varchar(21) NOT NULL default ' ',
  "AAAAFTTR" varchar(21) NOT NULL default ' ',
  "AAAAFTTS" varchar(21) NOT NULL default ' ',
  "AAAAFTTT" varchar(21) NOT NULL default ' ',
  "AAAAFTTU" varchar(21) NOT NULL default ' ',
  "AAAAFTTV" varchar(21) NOT NULL default ' ',
  "AAAAFTTW" varchar(21) NOT NULL default ' ',
  "AAAAFTTX" varchar(21) NOT NULL default ' ',
  "AAAAFTTY" varchar(21) NOT NULL default ' ',
  "AAAAFTTZ" varchar(4) NOT NULL default ' ',
  "AAAAFTUA" varchar(4) NOT NULL default ' ',
  "AAAAFTUB" varchar(4) NOT NULL default ' ',
  "AAAAFTUC" varchar(4) NOT NULL default ' ',
  "AAAAFTUD" varchar(4) NOT NULL default ' ',
  "AAAAFTUE" varchar(4) NOT NULL default ' ',
  "AAAAFTUF" varchar(4) NOT NULL default ' ',
  "AAAAFTUG" varchar(4) NOT NULL default ' ',
  "AAAAFTUH" varchar(4) NOT NULL default ' ',
  "AAAAFTUI" varchar(4) NOT NULL default ' ',
  "AAAAFTUJ" varchar(4) NOT NULL default ' ',
  "AAAAFTUK" varchar(4) NOT NULL default ' ',
  "AAAAFTUL" varchar(4) NOT NULL default ' ',
  "AAAAFTUM" varchar(4) NOT NULL default ' ',
  "AAAAFTUN" varchar(4) NOT NULL default ' ',
  "AAAAFTUO" varchar(4) NOT NULL default ' ',
  "AAAAFTUP" varchar(4) NOT NULL default ' ',
  "AAAAFTUQ" varchar(6) NOT NULL default ' ',
  "AAAAFTUR" varchar(6) NOT NULL default ' ',
  "AAAAFTUS" varchar(6) NOT NULL default ' ',
  "AAAAFTUT" varchar(6) NOT NULL default ' ',
  "AAAAFTUU" varchar(6) NOT NULL default ' ',
  "AAAAFTUV" varchar(6) NOT NULL default ' ',
  "AAAAFTUW" varchar(6) NOT NULL default ' ',
  "AAAAFTUX" varchar(6) NOT NULL default ' ',
  "AAAAFTUY" varchar(6) NOT NULL default ' ',
  "AAAAFTUZ" varchar(6) NOT NULL default ' ',
  "AAAAFTVA" varchar(4) NOT NULL default ' ',
  "AAAAFTVB" varchar(4) NOT NULL default ' ',
  "AAAAFTVC" varchar(4) NOT NULL default ' ',
  "AAABESFT" varchar(4) NOT NULL default ' ',
  "AAABESFU" varchar(4) NOT NULL default ' ',
  "AAAAFTVF" varchar(4) NOT NULL default ' ',
  "AAAAFTVG" varchar(21) NOT NULL default ' ',
  "AAAAFTVH" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTVI" varchar(3) NOT NULL default ' ',
  "AAAAFTVJ" varchar(2) NOT NULL default ' ',
  "AAAAFTVK" varchar(2) NOT NULL default ' ',
  "AAAAFTVL" varchar(2) NOT NULL default ' ',
  "AAAAFTVM" varchar(2) NOT NULL default ' ',
  "AAAAFTVN" varchar(2) NOT NULL default ' ',
  "AAAAFTVO" varchar(2) NOT NULL default ' ',
  "AAAAFTVP" varchar(2) NOT NULL default ' ',
  "AAAAFTVQ" varchar(2) NOT NULL default ' ',
  "AAAAFTVR" varchar(2) NOT NULL default ' ',
  "AAAAFTVS" varchar(2) NOT NULL default ' ',
  "AAAAFTVT" varchar(2) NOT NULL default ' ',
  "AAAAFTVU" varchar(2) NOT NULL default ' ',
  "AAAAFTVV" varchar(2) NOT NULL default ' ',
  "AAAAFTVW" varchar(2) NOT NULL default ' ',
  "AAAAFTVX" varchar(2) NOT NULL default ' ',
  "AAAAFUFO" varchar(1) NOT NULL default ' ',
  "AAAAFTVZ" varchar(21) NOT NULL default ' ',
  "AAAAFTWA" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWB" varchar(3) NOT NULL default ' ',
  "AAAAFTWC" decimal(6,3) NOT NULL default '0.000',
  "AAAAFTWD" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWE" varchar(3) NOT NULL default ' ',
  "AAAAFTWF" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWG" varchar(3) NOT NULL default ' ',
  "AAAAFTWH" varchar(1) NOT NULL default ' ',
  "AAAAFTWI" varchar(1) NOT NULL default ' ',
  "AAAAFTWJ" varchar(1) NOT NULL default ' ',
  "AAAAFTWK" varchar(1) NOT NULL default ' ',
  "AAAAFTWL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFTSI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFTSO" varchar(21) NOT NULL default ' ',
  "AAAAHHJY" varchar(21) NOT NULL default ' ',
  "AAAAHHJZ" varchar(21) NOT NULL default ' ',
  "AAAAFTSR" varchar(21) NOT NULL default ' ',
  "AAAAFTSS" varchar(21) NOT NULL default ' ',
  "AAAAFTST" varchar(21) NOT NULL default ' ',
  "AAAAFTSU" varchar(21) NOT NULL default ' ',
  "AAAAFTSV" varchar(21) NOT NULL default ' ',
  "AAAAFTSW" varchar(21) NOT NULL default ' ',
  "AAAAFTSX" varchar(21) NOT NULL default ' ',
  "AAAAFTSY" varchar(21) NOT NULL default ' ',
  "AAAAFTSZ" varchar(21) NOT NULL default ' ',
  "AAAAFTTA" varchar(21) NOT NULL default ' ',
  "AAAAFTTB" varchar(21) NOT NULL default ' ',
  "AAAAFTTC" varchar(21) NOT NULL default ' ',
  "AAAAFTTD" varchar(21) NOT NULL default ' ',
  "AAAAFTTE" varchar(21) NOT NULL default ' ',
  "AAAAFTTF" varchar(21) NOT NULL default ' ',
  "AAAAFTTG" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTTH" varchar(3) NOT NULL default ' ',
  "AAAAFTTI" varchar(21) NOT NULL default ' ',
  "AAAAFTTJ" varchar(21) NOT NULL default ' ',
  "AAAAFTTK" varchar(21) NOT NULL default ' ',
  "AAAAFTTL" varchar(21) NOT NULL default ' ',
  "AAAAFTTM" varchar(21) NOT NULL default ' ',
  "AAAAFTTN" varchar(21) NOT NULL default ' ',
  "AAAAFTTO" varchar(21) NOT NULL default ' ',
  "AAAAFTTP" varchar(21) NOT NULL default ' ',
  "AAAAFTTQ" varchar(21) NOT NULL default ' ',
  "AAAAFTTR" varchar(21) NOT NULL default ' ',
  "AAAAFTTS" varchar(21) NOT NULL default ' ',
  "AAAAFTTT" varchar(21) NOT NULL default ' ',
  "AAAAFTTU" varchar(21) NOT NULL default ' ',
  "AAAAFTTV" varchar(21) NOT NULL default ' ',
  "AAAAFTTW" varchar(21) NOT NULL default ' ',
  "AAAAFTTX" varchar(21) NOT NULL default ' ',
  "AAAAFTTY" varchar(21) NOT NULL default ' ',
  "AAAAFTTZ" varchar(4) NOT NULL default ' ',
  "AAAAFTUA" varchar(4) NOT NULL default ' ',
  "AAAAFTUB" varchar(4) NOT NULL default ' ',
  "AAAAFTUC" varchar(4) NOT NULL default ' ',
  "AAAAFTUD" varchar(4) NOT NULL default ' ',
  "AAAAFTUE" varchar(4) NOT NULL default ' ',
  "AAAAFTUF" varchar(4) NOT NULL default ' ',
  "AAAAFTUG" varchar(4) NOT NULL default ' ',
  "AAAAFTUH" varchar(4) NOT NULL default ' ',
  "AAAAFTUI" varchar(4) NOT NULL default ' ',
  "AAAAFTUJ" varchar(4) NOT NULL default ' ',
  "AAAAFTUK" varchar(4) NOT NULL default ' ',
  "AAAAFTUL" varchar(4) NOT NULL default ' ',
  "AAAAFTUM" varchar(4) NOT NULL default ' ',
  "AAAAFTUN" varchar(4) NOT NULL default ' ',
  "AAAAFTUO" varchar(4) NOT NULL default ' ',
  "AAAAFTUP" varchar(4) NOT NULL default ' ',
  "AAAAFTUQ" varchar(6) NOT NULL default ' ',
  "AAAAFTUR" varchar(6) NOT NULL default ' ',
  "AAAAFTUS" varchar(6) NOT NULL default ' ',
  "AAAAFTUT" varchar(6) NOT NULL default ' ',
  "AAAAFTUU" varchar(6) NOT NULL default ' ',
  "AAAAFTUV" varchar(6) NOT NULL default ' ',
  "AAAAFTUW" varchar(6) NOT NULL default ' ',
  "AAAAFTUX" varchar(6) NOT NULL default ' ',
  "AAAAFTUY" varchar(6) NOT NULL default ' ',
  "AAAAFTUZ" varchar(6) NOT NULL default ' ',
  "AAAAFTVA" varchar(4) NOT NULL default ' ',
  "AAAAFTVB" varchar(4) NOT NULL default ' ',
  "AAAAFTVC" varchar(4) NOT NULL default ' ',
  "AAABESFT" varchar(4) NOT NULL default ' ',
  "AAABESFU" varchar(4) NOT NULL default ' ',
  "AAAAFTVF" varchar(4) NOT NULL default ' ',
  "AAAAFTVG" varchar(21) NOT NULL default ' ',
  "AAAAFTVH" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTVI" varchar(3) NOT NULL default ' ',
  "AAAAFTVJ" varchar(2) NOT NULL default ' ',
  "AAAAFTVK" varchar(2) NOT NULL default ' ',
  "AAAAFTVL" varchar(2) NOT NULL default ' ',
  "AAAAFTVM" varchar(2) NOT NULL default ' ',
  "AAAAFTVN" varchar(2) NOT NULL default ' ',
  "AAAAFTVO" varchar(2) NOT NULL default ' ',
  "AAAAFTVP" varchar(2) NOT NULL default ' ',
  "AAAAFTVQ" varchar(2) NOT NULL default ' ',
  "AAAAFTVR" varchar(2) NOT NULL default ' ',
  "AAAAFTVS" varchar(2) NOT NULL default ' ',
  "AAAAFTVT" varchar(2) NOT NULL default ' ',
  "AAAAFTVU" varchar(2) NOT NULL default ' ',
  "AAAAFTVV" varchar(2) NOT NULL default ' ',
  "AAAAFTVW" varchar(2) NOT NULL default ' ',
  "AAAAFTVX" varchar(2) NOT NULL default ' ',
  "AAAAFUFO" varchar(1) NOT NULL default ' ',
  "AAAAFTVZ" varchar(21) NOT NULL default ' ',
  "AAAAFTWA" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWB" varchar(3) NOT NULL default ' ',
  "AAAAFTWC" decimal(6,3) NOT NULL default '0.000',
  "AAAAFTWD" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWE" varchar(3) NOT NULL default ' ',
  "AAAAFTWF" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTWG" varchar(3) NOT NULL default ' ',
  "AAAAFTWH" varchar(1) NOT NULL default ' ',
  "AAAAFTWI" varchar(1) NOT NULL default ' ',
  "AAAAFTWJ" varchar(1) NOT NULL default ' ',
  "AAAAFTWK" varchar(1) NOT NULL default ' ',
  "AAAAFTWL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFTWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABDHOQ" varchar(1) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABDHOV" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDHOX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAAAFTXI" varchar(1) NOT NULL default ' ',
  "AAAAFTXJ" varchar(1) NOT NULL default ' ',
  "AAABDZYW" varchar(3) NOT NULL default ' ',
  "AAABDZWY" varchar(4) NOT NULL default ' ',
  "AAAAFTXM" varchar(12) NOT NULL default ' ',
  "AAAAFTXN" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAAAFTXP" varchar(6) NOT NULL default ' ',
  "AAAAFTXQ" varchar(4) NOT NULL default '0000',
  "AAABERTL" varchar(4) NOT NULL default '0000',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAAAFTXU" varchar(1) NOT NULL default ' ',
  "AAAAFTXV" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTXW" varchar(3) NOT NULL default ' ',
  "AAAAFTXX" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTXY" varchar(3) NOT NULL default ' ',
  "AAAAFTXZ" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYA" varchar(3) NOT NULL default ' ',
  "AAAAFTYB" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYC" varchar(3) NOT NULL default ' ',
  "AAAAFTYD" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTYE" varchar(3) NOT NULL default ' ',
  "AAAANXWX" varchar(4) NOT NULL default ' ',
  "AAAANXWY" varchar(4) NOT NULL default ' ',
  "AAAANXWZ" varchar(4) NOT NULL default ' ',
  "AAAANXXA" varchar(4) NOT NULL default ' ',
  "AAAANXXB" varchar(4) NOT NULL default ' ',
  "AAAANXXC" varchar(4) NOT NULL default ' ',
  "AAAANXXD" varchar(4) NOT NULL default ' ',
  "AAAANXXE" varchar(4) NOT NULL default ' ',
  "AAAANXXF" varchar(4) NOT NULL default ' ',
  "AAAAFTYO" varchar(4) NOT NULL default ' ',
  "AAABDZNC" varchar(1) NOT NULL default ' ',
  "AAABDZQX" varchar(4) NOT NULL default ' ',
  "AAABDZRK" varchar(4) NOT NULL default ' ',
  "AAAAFTYS" varchar(7) NOT NULL default ' ',
  "AAAAFTYT" varchar(7) NOT NULL default ' ',
  "AAAAFTYU" varchar(7) NOT NULL default ' ',
  "AAAAFTYV" varchar(7) NOT NULL default ' ',
  "AAAAFTYW" varchar(4) NOT NULL default ' ',
  "AAAAFTYX" varchar(4) NOT NULL default ' ',
  "AAAAFTYY" varchar(3) NOT NULL default '000',
  "AAAAFTYZ" varchar(3) NOT NULL default '000',
  "AAAAFTZA" varchar(3) NOT NULL default '000',
  "AAAAFTZB" varchar(3) NOT NULL default '000',
  "AAAAFTZC" varchar(3) NOT NULL default '000',
  "AAAAFTZD" varchar(3) NOT NULL default '000',
  "AAAAFTZE" varchar(3) NOT NULL default '000',
  "AAAAFTZF" varchar(3) NOT NULL default '000',
  "AAAAFTZG" varchar(3) NOT NULL default '000',
  "AAAAFTZH" varchar(3) NOT NULL default '000',
  "AAAAFTZI" decimal(8,2) NOT NULL default '0.00',
  "AAAAFTZJ" varchar(3) NOT NULL default ' ',
  "AAABESAI" varchar(25) NOT NULL default ' ',
  "AAABERYE" varchar(3) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  "AAAAFTZN" varchar(4) NOT NULL default ' ',
  "AAAAFTZO" varchar(4) NOT NULL default ' ',
  "AAAAFTZP" varchar(4) NOT NULL default ' ',
  "AAAAFTZQ" varchar(4) NOT NULL default ' ',
  "AAAAFTZR" varchar(4) NOT NULL default ' ',
  "AAAAFTZS" varchar(4) NOT NULL default ' ',
  "AAAAFTZT" varchar(4) NOT NULL default ' ',
  "AAAAFTZU" varchar(7) NOT NULL default ' ',
  "AAAAFTZV" varchar(7) NOT NULL default ' ',
  "AAAAFTZW" varchar(7) NOT NULL default ' ',
  "AAAAFTZX" varchar(7) NOT NULL default ' ',
  "AAAAFTZY" varchar(7) NOT NULL default ' ',
  "AAAAFTZZ" varchar(7) NOT NULL default ' ',
  "AAAAFUAA" varchar(7) NOT NULL default ' ',
  "AAAAFUAB" varchar(7) NOT NULL default ' ',
  "AAAAFUAC" varchar(7) NOT NULL default ' ',
  "AAAAFUAD" varchar(7) NOT NULL default ' ',
  "AAAAFUAE" varchar(70) NOT NULL default ' ',
  "AAAAFUAF" varchar(70) NOT NULL default ' ',
  "AAAAFUAG" varchar(70) NOT NULL default ' ',
  "AAAAFUAH" varchar(70) NOT NULL default ' ',
  "AAAAFUAI" varchar(70) NOT NULL default ' ',
  "AAAAFUAJ" varchar(70) NOT NULL default ' ',
  "AAAAPLBF" varchar(70) NOT NULL default ' ',
  "AAAAFUAL" varchar(70) NOT NULL default ' ',
  "AAAAFUAM" varchar(70) NOT NULL default ' ',
  "AAAAFUAN" varchar(70) NOT NULL default ' ',
  "AAAAFUAO" varchar(70) NOT NULL default ' ',
  "AAAAFUAP" varchar(70) NOT NULL default ' ',
  "AAAAFUAQ" varchar(70) NOT NULL default ' ',
  "AAAAFUAR" varchar(70) NOT NULL default ' ',
  "AAAAFUAS" varchar(70) NOT NULL default ' ',
  "AAAAGWRW" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRX" varchar(3) NOT NULL default ' ',
  "AAAAGWRY" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRZ" varchar(3) NOT NULL default ' ',
  "AAAAFUAX" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUAY" varchar(3) NOT NULL default ' ',
  "AAAAFUAZ" varchar(1) NOT NULL default ' ',
  "AAAAFUBA" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBB" varchar(3) NOT NULL default ' ',
  "AAAAFUBC" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBD" varchar(3) NOT NULL default ' ',
  "AAAAGWRP" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRQ" varchar(3) NOT NULL default ' ',
  "AAAAGWRR" varchar(4) NOT NULL default '0000',
  "AAAAFUBH" varchar(1) NOT NULL default ' ',
  "AAAAFUBI" varchar(1) NOT NULL default ' ',
  "AAAAFUBJ" varchar(1) NOT NULL default ' ',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAAAGWTI" varchar(1) NOT NULL default ' ',
  "AAAAFUBM" varchar(4) NOT NULL default ' ',
  "AAAAFUBN" varchar(10) NOT NULL default ' ',
  "AAAAFUBO" decimal(12,2) NOT NULL default '0.00',
  "AAAAFUBP" varchar(3) NOT NULL default ' ',
  "AAAAFUBQ" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBR" varchar(3) NOT NULL default ' ',
  "AAAAFUBS" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBT" varchar(3) NOT NULL default ' ',
  "AAAAFUBU" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBV" varchar(3) NOT NULL default ' ',
  "AAAAFUBW" decimal(11,2) NOT NULL default '0.00',
  "AAAAFUBX" varchar(3) NOT NULL default ' ',
  "AAAAFUBY" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUBZ" varchar(3) NOT NULL default ' ',
  "AAAAFUCA" decimal(9,2) NOT NULL default '0.00',
  "AAAAFUCB" varchar(3) NOT NULL default ' ',
  "AAAAFUCC" decimal(10,2) NOT NULL default '0.00',
  "AAAAFUCD" varchar(3) NOT NULL default ' ',
  "AAAAFUCE" varchar(1) NOT NULL default ' ',
  "AAAAFUCF" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCG" varchar(3) NOT NULL default ' ',
  "AAAAFUCH" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCI" varchar(3) NOT NULL default ' ',
  "AAAAFUCJ" decimal(8,2) NOT NULL default '0.00',
  "AAAAFUCK" varchar(3) NOT NULL default ' ',
  "AAABERYF" varchar(1) NOT NULL default ' ',
  "AAAAGWRV" varchar(5) NOT NULL default ' ',
  "AAAAFUCN" varchar(2) NOT NULL default ' ',
  "AAAAGWRT" varchar(25) NOT NULL default ' ',
  "AAAAGWSA" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSB" varchar(3) NOT NULL default ' ',
  "AAAAGWSC" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSD" varchar(3) NOT NULL default ' ',
  "AAAAMJSD" varchar(40) NOT NULL default ' ',
  "AAAAFUCU" varchar(1) NOT NULL default ' ',
  "AAABAWWL" varchar(1) NOT NULL default ' ',
  "AAAAFUCW" varchar(12) NOT NULL default ' ',
  "AAAAFUCX" varchar(12) NOT NULL default ' ',
  "AAAAFUCY" varchar(12) NOT NULL default ' ',
  "AAAAFUCZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAFUDD" ("AAABMRIL","AAABMLCZ","AAABESHA"),
  KEY "AAAAFUDH" ("AAABMRIL","AAABMLCZ","AAABESGI","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABESGI","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABDHOQ" varchar(1) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABDHOV" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDHOX" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAFUFX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERZR" varchar(5) NOT NULL default ' ',
  "AAABERZB" varchar(1) NOT NULL default ' ',
  "AAABERZC" decimal(8,2) NOT NULL default '0.00',
  "AAABERZD" varchar(3) NOT NULL default ' ',
  "AAABERZA" varchar(1) NOT NULL default ' ',
  "AAAAGWPU" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWPV" varchar(3) NOT NULL default ' ',
  "AAAAGWPW" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWPX" varchar(3) NOT NULL default ' ',
  "AAABFITB" varchar(1) NOT NULL default ' ',
  "AAAAGWPZ" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWQA" varchar(3) NOT NULL default ' ',
  "AAAAGWQM" varchar(1) NOT NULL default ' ',
  "AAAAGWQN" varchar(1) NOT NULL default ' ',
  "AAAAYLJE" varchar(30) NOT NULL default ' ',
  "AAAAFUFB" varchar(30) NOT NULL default ' ',
  "AAAAFUFC" varchar(2) NOT NULL default ' ',
  "AAAAGWQB" varchar(1) NOT NULL default ' ',
  "AAAAGWQC" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWQD" varchar(3) NOT NULL default ' ',
  "AAAAGWQE" varchar(1) NOT NULL default ' ',
  "AAAAGWQF" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWQG" varchar(3) NOT NULL default ' ',
  "AAAAGWQH" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWQI" varchar(3) NOT NULL default ' ',
  "AAAAGWQJ" varchar(1) NOT NULL default ' ',
  "AAAAGWQK" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWQL" varchar(3) NOT NULL default ' ',
  "AAAAFUFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAFUFS" ("AAABERZR","AAAAFUFX"),
  KEY "AAAAFUFV" ("AAABMRIL","AAAAFUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUFY" (
  "AAABDYVX" varchar(30) NOT NULL default ' ',
  "AAABHSIQ" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAFUGC" varchar(1) NOT NULL default ' ',
  "AAAAFUGD" varchar(26) NOT NULL default ' ',
  "AAAAFUGE" varchar(74) NOT NULL default ' ',
  "AAAAFUGF" varchar(1) NOT NULL default ' ',
  "AAAAFUGG" varchar(21) NOT NULL default ' ',
  PRIMARY KEY ("AAABDYVX","AAABHSIQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABGTET" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGVLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFUHC" varchar(15) NOT NULL default ' ',
  "AAAATOIB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFUHC","AAAATOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWB","AAABGVLV","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABLMOZ" varchar(3) NOT NULL default ' ',
  "AAAAFUIX" varchar(32) NOT NULL default ' ',
  "AAAAFUHS" varchar(32) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABGSLP" varchar(1) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAAAFUIL" ("AAABGVLV"),
  KEY "AAAAFUIN" ("AAABJGWH"),
  KEY "AAAAFUIP" ("AAABJGWD"),
  KEY "AAAAFUIR" ("AAABJMEG"),
  KEY "AAAAFUIT" ("AAABGVFO"),
  KEY "AAAAFUIV" ("AAABMRIL","AAAAFUIX"),
  KEY "AAAAFUIY" ("AAABJGWI"),
  KEY "AAAAFUJA" ("AAABKZRY"(255)),
  KEY "AAAAFUJC" ("AAABGSLP"),
  KEY "AAAAFUJE" ("AAABGWDW"),
  KEY "AAAAFUJG" ("AAABLIMM"),
  KEY "AAAAFUJI" ("AAABGYJY"),
  KEY "AAAAFUJK" ("AAABJGWE"),
  KEY "AAAAFUJM" ("AAABJGWF"),
  KEY "AAAAFUJO" ("AAABGRGT"),
  KEY "AAAAFUJQ" ("AAABJGWB"),
  KEY "AAAAFUJS" ("AAABJGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUJU" (
  "AAABLUAV" varchar(45) NOT NULL default ' ',
  "AAABGRSQ" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABIRUN" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABFAHA" varchar(2) NOT NULL default '00',
  "AAABFACK" varchar(1) NOT NULL default ' ',
  "AAABIEWD" varchar(60) NOT NULL default ' ',
  "AAABFACM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCRQ" varchar(12) NOT NULL default '000000000000',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  "AAAAFUKO" varchar(30) NOT NULL default ' ',
  "AAAAFUKP" varchar(12) NOT NULL default ' ',
  "AAAAFURX" varchar(1) NOT NULL default ' ',
  "AAAAFUKR" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAAAZNRP" varchar(40) NOT NULL default ' ',
  "AAAAFUKV" varchar(4) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABKOWG" varchar(8) NOT NULL default ' ',
  "AAAAWTEV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFULB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCRQ" varchar(12) NOT NULL default '000000000000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCRQ","AAABLLIM","AAABLUSC","AAABKMJV","AAABLUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFULR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCRQ" varchar(12) NOT NULL default '000000000000',
  "AAAAFURT" varchar(12) NOT NULL default ' ',
  "AAAAFURR" varchar(8) NOT NULL default '00000000',
  "AAAAFUQT" varchar(1) NOT NULL default ' ',
  "AAAAFUQU" varchar(8) NOT NULL default ' ',
  "AAAAFUQV" varchar(2) NOT NULL default ' ',
  "AAAAFUQW" varchar(8) NOT NULL default '00000000',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAAAFUQX" varchar(8) NOT NULL default '00000000',
  "AAAAFUQY" varchar(8) NOT NULL default '00000000',
  "AAAAFUQZ" varchar(1) NOT NULL default ' ',
  "AAAAFURA" varchar(2) NOT NULL default ' ',
  "AAABHGFF" varchar(12) NOT NULL default ' ',
  "AAAAFURC" varchar(1) NOT NULL default ' ',
  "AAAAFURD" varchar(1) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAAAPRNG" varchar(1) NOT NULL default ' ',
  "AAAAFURG" varchar(1) NOT NULL default ' ',
  "AAAAFURH" varchar(1) NOT NULL default ' ',
  "AAAAFURX" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHCRQ","AAAAFURT","AAAAFURR","AAAAFUQT","AAAAFUQU","AAAAFUQV","AAAAFUQW","AAABKQWU","AAABKQWS","AAABKQWT","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAFUNO" varchar(40) NOT NULL default ' ',
  "AAAAFUNP" varchar(20) NOT NULL default ' ',
  "AAAAFUNQ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABMMVH","AAAAFUNO","AAAAFUNP","AAAAFUNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABMLCP" varchar(12) NOT NULL default ' ',
  "AAABMMJT" double NOT NULL default '0',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAFUNX" varchar(12) NOT NULL default ' ',
  "AAAAFUNY" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  "AAABKQPD" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAAAFUOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXKV","AAABKQWU","AAABKQWS","AAABKQWT","AAABKQPD","AAABMCRM","AAABMCXK","AAABMCXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWRVJ" varchar(2) NOT NULL default ' ',
  "AAAAFUOY" varchar(1) NOT NULL default ' ',
  "AAABDFWS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWRVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWRVJ" varchar(2) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWRVJ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABKNQB" varchar(8) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABKNQB","AAABMCRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQB" varchar(8) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQB","AAABLLIM","AAABLUSC","AAABKMJV","AAABLUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABHCRQ" varchar(8) NOT NULL default ' ',
  "AAAAFURT" varchar(12) NOT NULL default ' ',
  "AAAAFURR" varchar(8) NOT NULL default '00000000',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAABKQWT" varchar(2) NOT NULL default ' ',
  "AAABMLEL" varchar(8) NOT NULL default '00000000',
  "AAAAFUQT" varchar(1) NOT NULL default ' ',
  "AAAAFUQU" varchar(8) NOT NULL default ' ',
  "AAAAFUQV" varchar(2) NOT NULL default ' ',
  "AAAAFUQW" varchar(8) NOT NULL default '00000000',
  "AAAAFUQX" varchar(8) NOT NULL default '00000000',
  "AAAAFUQY" varchar(8) NOT NULL default '00000000',
  "AAAAFUQZ" varchar(1) NOT NULL default ' ',
  "AAAAFURA" varchar(2) NOT NULL default ' ',
  "AAABHGFF" varchar(12) NOT NULL default ' ',
  "AAAAFURC" varchar(1) NOT NULL default ' ',
  "AAAAFURD" varchar(1) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAAAPRNG" varchar(1) NOT NULL default ' ',
  "AAAAFURG" varchar(1) NOT NULL default ' ',
  "AAAAFURH" varchar(1) NOT NULL default ' ',
  "AAAAFURX" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAALFMB" decimal(15,0) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABHZQN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABHCRQ","AAAAFURT","AAAAFURR"),
  KEY "AAAAFURS" ("AAAAFURT","AAAAFURX"),
  KEY "AAAAFURV" ("AAABHGFF","AAAAFURX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFURY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAFUSG" varchar(4) NOT NULL default ' ',
  "AAAAFUSH" varchar(10) NOT NULL default ' ',
  "AAAAFUSD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAFUSG","AAAAFUSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAAAFUSQ" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAAAFUSQ","AAABKRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFUSY" varchar(12) NOT NULL default ' ',
  "AAABHCRQ" varchar(12) NOT NULL default '000000000000',
  "AAABMKFU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAFUSY","AAABHCRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFUTH" varchar(12) NOT NULL default ' ',
  "AAABLUMJ" decimal(15,0) NOT NULL default '0',
  "AAAAFUTE" varchar(40) NOT NULL default ' ',
  "AAABHAZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFUTH","AAABLUMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAFUTT" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(5) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAAAFUTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAFUTT","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLBXJ" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAAAVZXS" varchar(1) NOT NULL default ' ',
  "AAABDJRZ" varchar(6) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAGBXG" varchar(1) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAANJPU" varchar(1) NOT NULL default ' ',
  "AAABMCNB" varchar(10) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMCRW" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCPL" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAVZEC" varchar(3) NOT NULL default '000',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABEMIB" varchar(4) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQH" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPN" varchar(8) NOT NULL default '00000000',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAGBUV" varchar(8) NOT NULL default '00000000',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABMCTD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQZ" varchar(2) NOT NULL default ' ',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMCQJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCUF" varchar(6) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAGBVS" varchar(1) NOT NULL default ' ',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABJKLH" varchar(3) NOT NULL default ' ',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABMDBB","AAABMCPV","AAABLBXJ","AAABMCPS","AAAAVZXS","AAABDJRZ","AAABMQBT","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFUWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLBXJ" varchar(1) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLCDX" varchar(4) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABLCEL" varchar(1) NOT NULL default ' ',
  "AAABMCUR" varchar(2) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVK" varchar(60) NOT NULL default ' ',
  "AAABJWRJ" varchar(16) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAAAPKVG" varchar(2) NOT NULL default ' ',
  "AAABHEXF" varchar(11) NOT NULL default ' ',
  "AAABLCDH" varchar(10) NOT NULL default ' ',
  "AAABLCDD" varchar(4) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAAAFVGA" varchar(10) NOT NULL default ' ',
  "AAABLCFC" varchar(10) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHT" varchar(30) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLVHX" varchar(30) NOT NULL default ' ',
  "AAABLVHZ" varchar(14) NOT NULL default ' ',
  "AAABLVHY" varchar(15) NOT NULL default ' ',
  "AAABLCDY" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAALFII" decimal(15,2) NOT NULL default '0.00',
  "AAABLCDA" varchar(7) NOT NULL default '0000000',
  "AAABLCDB" varchar(5) NOT NULL default '00000',
  "AAABLCDE" varchar(1) NOT NULL default '0',
  "AAABLCJJ" varchar(16) NOT NULL default ' ',
  "AAABLCJK" varchar(11) NOT NULL default ' ',
  "AAABLCDG" varchar(10) NOT NULL default ' ',
  "AAABLCED" varchar(1) NOT NULL default ' ',
  "AAABLCJL" varchar(1) NOT NULL default ' ',
  "AAABMCUY" varchar(11) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLCFQ" varchar(12) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABLCFD" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAAAWDWT" decimal(15,2) NOT NULL default '0.00',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABLCFB" varchar(10) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAAAFVHO" varchar(1) NOT NULL default ' ',
  "AAAAFUZX" varchar(4) NOT NULL default ' ',
  "AAABLCFM" varchar(4) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABLCFX" decimal(13,2) NOT NULL default '0.00',
  "AAABLCFY" varchar(10) NOT NULL default ' ',
  "AAABMCVC" varchar(10) NOT NULL default ' ',
  "AAABFDMP" varchar(8) NOT NULL default '00000000',
  "AAAALEUP" varchar(10) NOT NULL default ' ',
  "AAABJKQZ" varchar(1) NOT NULL default ' ',
  "AAAAGBXN" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAH" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQS" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAK" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAL" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAM" varchar(1) NOT NULL default ' ',
  "AAAAFVAN" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAO" varchar(1) NOT NULL default ' ',
  "AAAAFVAP" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAQ" varchar(1) NOT NULL default ' ',
  "AAAAFVAR" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAS" varchar(1) NOT NULL default ' ',
  "AAAAFVAT" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAU" varchar(1) NOT NULL default ' ',
  "AAAAFVAV" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAW" varchar(1) NOT NULL default ' ',
  "AAAAFVAX" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVAY" varchar(1) NOT NULL default ' ',
  "AAAAFVAZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAFVBA" varchar(1) NOT NULL default ' ',
  "AAAAFVBB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABMDBB","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABJWRV" varchar(16) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAAAVZXS" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAAAFVBW" varchar(2) NOT NULL default '00',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCPN" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAAAGBXG" varchar(1) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAANJPU" varchar(1) NOT NULL default ' ',
  "AAABMCNB" varchar(10) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMCMV" varchar(8) NOT NULL default '00000000',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMCRY" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCRW" varchar(1) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQG" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQH" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCSN" varchar(1) NOT NULL default ' ',
  "AAABMCQZ" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCSS" varchar(10) NOT NULL default ' ',
  "AAABMCTE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAAAGBUV" varchar(8) NOT NULL default '00000000',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABMCTD" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMCQJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAMSGJ" varchar(10) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAGBVS" varchar(1) NOT NULL default ' ',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMCSY" decimal(3,0) NOT NULL default '0',
  "AAABMCTB" decimal(5,3) NOT NULL default '0.000',
  "AAABMCSZ" decimal(3,0) NOT NULL default '0',
  "AAABMCTC" decimal(5,3) NOT NULL default '0.000',
  "AAABMCTA" decimal(3,0) NOT NULL default '0',
  "AAABMCTL" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABMCTV" varchar(10) NOT NULL default ' ',
  "AAABMCTX" varchar(3) NOT NULL default '000',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABMDBB","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABJWRV" varchar(16) NOT NULL default ' ',
  "AAABLBXJ" varchar(1) NOT NULL default ' ',
  "AAAAGBXG" varchar(1) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLCDX" varchar(4) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABLCEL" varchar(1) NOT NULL default ' ',
  "AAABMCUR" varchar(2) NOT NULL default ' ',
  "AAAAVZCL" varchar(4) NOT NULL default ' ',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVK" varchar(60) NOT NULL default ' ',
  "AAABJWRJ" varchar(16) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAAAPKVG" varchar(2) NOT NULL default ' ',
  "AAABHEXF" varchar(11) NOT NULL default ' ',
  "AAABLCDH" varchar(10) NOT NULL default ' ',
  "AAABLCDD" varchar(4) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAAAFVGA" varchar(10) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHT" varchar(30) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLVHX" varchar(30) NOT NULL default ' ',
  "AAABLVHZ" varchar(14) NOT NULL default ' ',
  "AAABLVHY" varchar(15) NOT NULL default ' ',
  "AAABLCDY" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAAALFII" decimal(15,2) NOT NULL default '0.00',
  "AAABLCDA" varchar(7) NOT NULL default '0000000',
  "AAABLCDB" varchar(5) NOT NULL default '00000',
  "AAABLCDE" varchar(1) NOT NULL default '0',
  "AAABLCJJ" varchar(16) NOT NULL default ' ',
  "AAABLCJK" varchar(11) NOT NULL default ' ',
  "AAABLCDG" varchar(10) NOT NULL default ' ',
  "AAABLCED" varchar(1) NOT NULL default ' ',
  "AAABLCJL" varchar(1) NOT NULL default ' ',
  "AAABMCUY" varchar(11) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLCFQ" varchar(12) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABLCFD" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAAAWDWT" decimal(15,2) NOT NULL default '0.00',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABLCFB" varchar(10) NOT NULL default ' ',
  "AAAAFVHO" varchar(1) NOT NULL default ' ',
  "AAABLCFM" varchar(4) NOT NULL default ' ',
  "AAABMPSH" varchar(1) NOT NULL default ' ',
  "AAABLCFX" decimal(13,2) NOT NULL default '0.00',
  "AAABLCFY" varchar(10) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAABMCVC" varchar(10) NOT NULL default ' ',
  "AAABFDMP" varchar(8) NOT NULL default '00000000',
  "AAAALEUP" varchar(10) NOT NULL default ' ',
  "AAABJKQZ" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAABJWRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABJWRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALTGX" varchar(22) NOT NULL default ' ',
  "AAAAFVIO" varchar(22) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAFVIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAALTGX","AAAAFVIO","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVIR" (
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAAARXTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVIV" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFVNJ" varchar(10) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAAAFVRZ" varchar(1) NOT NULL default ' ',
  "AAAAFVJW" varchar(1) NOT NULL default ' ',
  "AAAAFVJC" varchar(1) NOT NULL default ' ',
  "AAAAFVJD" varchar(1) NOT NULL default ' ',
  "AAABMQIX" varchar(1) NOT NULL default ' ',
  "AAABEAWW" varchar(2) NOT NULL default ' ',
  "AAAAFVNN" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVJQ" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAAAFVRZ" varchar(1) NOT NULL default ' ',
  "AAAAFVJW" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVKH" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAAAFVKM" varchar(60) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVKU" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBDJ" varchar(30) NOT NULL default ' ',
  "AAABEBCS" varchar(4) NOT NULL default ' ',
  "AAABEBCT" varchar(1) NOT NULL default ' ',
  "AAABEBCU" varchar(1) NOT NULL default ' ',
  "AAABEBCV" varchar(1) NOT NULL default ' ',
  "AAABEBCW" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABEBCY" varchar(4) NOT NULL default '0000',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAAAFVLM" varchar(1) NOT NULL default ' ',
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABLEEK" varchar(1) NOT NULL default ' ',
  "AAAAFVLP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG","AAABEBDJ"),
  KEY "AAAAFVLT" ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVLV" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG"),
  KEY "AAAAFVMJ" ("AAABHEIG","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVMM" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFVMQ" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG","AAABMREL"),
  KEY "AAAAFVNB" ("AAABMREL","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVNE" (
  "AAABEBIF" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFVNH" varchar(26) NOT NULL default ' ',
  "AAABHZQN" varchar(26) NOT NULL default ' ',
  "AAAAFVNJ" varchar(10) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAAAFVNL" varchar(1) NOT NULL default ' ',
  "AAAAFVNM" varchar(1) NOT NULL default ' ',
  "AAAAFVNN" varchar(1) NOT NULL default ' ',
  "AAAAFVNO" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABFDXL" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIF","AAABIIUG"),
  KEY "AAAAFVOB" ("AAABEPTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVOD" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEBIF" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABINAW","AAABEBIF","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVOO" (
  "AAABEBIF" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLUNJ" varchar(26) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAAATXWY" varchar(3) NOT NULL default '000',
  "AAABDPTA" varchar(5) NOT NULL default '00000',
  "AAABDPTC" varchar(5) NOT NULL default '00000',
  "AAABLFTX" varchar(5) NOT NULL default '00000',
  "AAABLFTW" varchar(5) NOT NULL default '00000',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAAAFVPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIF","AAABIIUG","AAABLUNJ","AAABLLIO"),
  UNIQUE KEY "AAAAFVPK" ("AAABLUNJ","AAABLLIO","AAABIIUG","AAABEBIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVPP" (
  "AAABEBGD" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBGA" varchar(26) NOT NULL default ' ',
  "AAABEBAI" varchar(1) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAAAFVRZ" varchar(1) NOT NULL default ' ',
  "AAABEAZR" varchar(1) NOT NULL default ' ',
  "AAABEAZS" varchar(1) NOT NULL default ' ',
  "AAABEAZT" varchar(2) NOT NULL default ' ',
  "AAABEAZU" varchar(2) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBGD","AAABIIUG","AAABEBGA","AAABEBAI"),
  KEY "AAAAFVQL" ("AAABEBGA","AAABIIUG","AAABEBAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVQP" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEBGD" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBGA" varchar(26) NOT NULL default ' ',
  "AAABEBAI" varchar(1) NOT NULL default ' ',
  "AAAAFVQV" varchar(30) NOT NULL default ' ',
  "AAAAFVQW" varchar(30) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEBGD","AAABIIUG","AAABEBGA","AAABEBAI"),
  KEY "AAAAFVRG" ("AAABEBGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVRI" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(2) NOT NULL default '00',
  "AAABEBCL" varchar(60) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABIIUG","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVRU" (
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAAAFVRZ" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABEBEG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABIIUG","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVSM" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABIIUG","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVTA" (
  "AAABEBGD" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEBGA" varchar(26) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBGD","AAABIIUG","AAABEBGA","AAABEBGG"),
  KEY "AAAAFVTQ" ("AAABEBGA","AAABIIUG","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVTU" (
  "AAABEBIH" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAAAFVTX" varchar(1) NOT NULL default ' ',
  "AAABFDXL" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIH","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVUI" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEBIH" varchar(10) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEBIH","AAABIIUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVUU" (
  "AAABEBIF" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLUNJ" varchar(26) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABEAZA" varchar(2) NOT NULL default '00',
  "AAABEAZB" varchar(3) NOT NULL default '000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABEBIF","AAABIIUG","AAABLUNJ","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVVI" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(26) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABILRT" varchar(2) NOT NULL default '00',
  "AAABEBCL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABIIUG","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVVU" (
  "AAAAFVWI" varchar(12) NOT NULL default ' ',
  "AAAAGECE" varchar(1) NOT NULL default '0',
  "AAAAFVVX" varchar(1) NOT NULL default ' ',
  "AAAAFVVY" varchar(1) NOT NULL default ' ',
  "AAABKFDH" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAFVWG" varchar(1) NOT NULL default ' ',
  "AAAAFVWH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFVWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVWJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVWP" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAAAFVWU" varchar(22) NOT NULL default ' ',
  "AAAAFVXE" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABFMOO" varchar(61) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFVXD" ("AAAAFVXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVXF" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABHJRW" varchar(3) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFVXO" varchar(40) NOT NULL default ' ',
  "AAAAFVXP" varchar(20) NOT NULL default ' ',
  "AAAAFVXQ" varchar(20) NOT NULL default ' ',
  "AAAAFVXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVXT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVXZ" (
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVYD" (
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAAAFVYL" varchar(4) NOT NULL default ' ',
  "AAABHJRW" varchar(3) NOT NULL default ' ',
  "AAAAFVYH" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAAAFVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVYM" (
  "AAAALARC" varchar(22) NOT NULL default ' ',
  "AAAAFVYS" varchar(1) NOT NULL default ' ',
  "AAAAFWNB" varchar(4) NOT NULL default ' ',
  "AAAAFWNC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAALARC","AAAAFVYS","AAAAFWNB","AAAAFWNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVYV" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAAAFVZA" varchar(32) NOT NULL default ' ',
  "AAABJEHQ" varchar(10) NOT NULL default ' ',
  "AAABJEHS" varchar(30) NOT NULL default ' ',
  "AAABJEHT" varchar(30) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVZK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAABJEHO" varchar(4) NOT NULL default ' ',
  "AAABJEHP" varchar(4) NOT NULL default ' ',
  "AAAAKNHN" varchar(40) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFVZX" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAAATJNW" varchar(30) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAAARBBM" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWAJ" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABFZAB" varchar(25) NOT NULL default ' ',
  "AAABHTTC" varchar(30) NOT NULL default ' ',
  "AAAAFWAV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWAX" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAAAFWBG" varchar(22) NOT NULL default ' ',
  "AAAAFWMD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAAAFWME","AAAAFWMF","AAAAFWBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWBH" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWBJ" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAAAFWBQ" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFWBS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWBU" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAGIAY" varchar(22) NOT NULL default ' ',
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAATPHC" varchar(22) NOT NULL default ' ',
  "AAAAFWCA" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFWCH" varchar(22) NOT NULL default ' ',
  "AAAAFWCI" varchar(22) NOT NULL default ' ',
  "AAAAFWCJ" varchar(22) NOT NULL default ' ',
  "AAAAFWCK" varchar(22) NOT NULL default ' ',
  "AAAAFWNB" varchar(4) NOT NULL default ' ',
  "AAAAFWNC" varchar(4) NOT NULL default ' ',
  "AAAAFWCN" varchar(22) NOT NULL default ' ',
  "AAAAFWCO" varchar(22) NOT NULL default ' ',
  "AAABHLTE" varchar(1) NOT NULL default '0',
  "AAABLKJA" varchar(2) NOT NULL default '00',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  "AAABLFSP" varchar(4) NOT NULL default ' ',
  "AAAAFWCT" varchar(22) NOT NULL default ' ',
  "AAAAFWMO" varchar(4) NOT NULL default ' ',
  "AAAAFWCV" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  UNIQUE KEY "AAAAFWCX" ("AAABMLXR"),
  KEY "AAAAFWCZ" ("AAAAGIAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWDB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWDH" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFXFR" varchar(22) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWDT" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAAFWDV" varchar(1) NOT NULL default ' ',
  "AAAAFWDW" varchar(3) NOT NULL default '000',
  "AAAAFWDX" varchar(1) NOT NULL default ' ',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWEB" (
  "AAABKNGX" varchar(22) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNGX","AAABMLZS","AAABJSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWEI" (
  "AAABKNGX" varchar(22) NOT NULL default ' ',
  "AAAARZMC" varchar(1) NOT NULL default '0',
  "AAABIJQN" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(20) NOT NULL default ' ',
  "AAAAFWET" varchar(3) NOT NULL default '000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNGX","AAAARZMC","AAABIJQN","AAABKAFO","AAAAFWET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWEU" (
  "AAABMLXR" varchar(32) NOT NULL default ' ',
  "AAAAFWEW" varchar(1) NOT NULL default ' ',
  "AAAAFWEX" varchar(1) NOT NULL default ' ',
  "AAAAFWEY" varchar(1) NOT NULL default ' ',
  "AAAAFWEZ" varchar(1) NOT NULL default ' ',
  "AAAAFWFA" varchar(5) NOT NULL default '00000',
  "AAAAFWFB" varchar(5) NOT NULL default '00000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWFF" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWFM" varchar(20) NOT NULL default ' ',
  "AAAAFWFI" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABJESP" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLTVR","AAAAFWFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWFN" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWFX" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAAAFWFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWFY" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFWGJ" varchar(22) NOT NULL default ' ',
  "AAAAFXEC" varchar(4) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTVR","AAAAFXEG","AAAAFWGJ","AAAAFXEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWGL" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFWGO" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLKIV" varchar(2) NOT NULL default ' ',
  "AAABLEJW" varchar(128) NOT NULL default ' ',
  "AAABJENC" varchar(60) NOT NULL default ' ',
  "AAABKTAF" varchar(1) NOT NULL default ' ',
  "AAABJEND" varchar(4) NOT NULL default '0000',
  "AAABJENO" varchar(1) NOT NULL default ' ',
  "AAAAFWHA" varchar(1) NOT NULL default ' ',
  "AAABJEKZ" varchar(1) NOT NULL default ' ',
  "AAAAFWHC" varchar(5) NOT NULL default '00000',
  "AAAAFWHD" varchar(1) NOT NULL default ' ',
  "AAAAFWHE" varchar(1) NOT NULL default ' ',
  "AAAAFWHF" varchar(1) NOT NULL default ' ',
  "AAAAFWHG" varchar(1) NOT NULL default ' ',
  "AAAAFWHH" varchar(1) NOT NULL default ' ',
  "AAAAFWHI" varchar(1) NOT NULL default ' ',
  "AAAAFWHJ" varchar(1) NOT NULL default ' ',
  "AAAAFWHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWHM" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWIU" varchar(22) NOT NULL default ' ',
  "AAAAFWHP" varchar(22) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAAAFWHU" varchar(22) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABFWCT" varchar(6) NOT NULL default '000000',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAAAFWIE" varchar(6) NOT NULL default '000000',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABLDNU" varchar(32) NOT NULL default ' ',
  "AAAAFWII" varchar(1) NOT NULL default ' ',
  "AAAAFWIJ" varchar(128) NOT NULL default ' ',
  "AAAAFWIK" varchar(1) NOT NULL default ' ',
  "AAAAFWIL" varchar(1) NOT NULL default ' ',
  "AAAAFWIM" varchar(5) NOT NULL default '00000',
  "AAABJEDB" varchar(30) NOT NULL default ' ',
  "AAAAFWIO" varchar(128) NOT NULL default ' ',
  "AAABJYWC" varchar(1) NOT NULL default ' ',
  "AAAAFWIQ" varchar(4) NOT NULL default '0000',
  "AAAAJUFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWIT" ("AAAAFWIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWIV" (
  "AAAAFWJG" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(15) NOT NULL default '000000000000000',
  PRIMARY KEY ("AAAAFWJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWIZ" (
  "AAAAFWJG" varchar(22) NOT NULL default ' ',
  "AAAAFWJH" varchar(2) NOT NULL default '00',
  "AAABLDRF" varchar(2) NOT NULL default '00',
  "AAAAFWJD" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFWJG","AAAAFWJH","AAABLDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWJJ" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABGPVW" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWJY" (
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWMD" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(4) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAAAFWME","AAAAFWMF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWKL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLECD","AAAAFWME","AAAAFWMF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWKX" (
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWMD" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAFWLD" varchar(61) NOT NULL default ' ',
  "AAAAFWLE" varchar(61) NOT NULL default ' ',
  "AAAAUXDW" varchar(4) NOT NULL default ' ',
  "AAAAFWMU" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAFWME","AAAAFWMF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWLQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAFWME","AAAAFWMF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWMA" (
  "AAAAFWME" varchar(30) NOT NULL default ' ',
  "AAAAFWMF" varchar(30) NOT NULL default ' ',
  "AAAAFWMD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFWME","AAAAFWMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWMG" (
  "AAAALARC" varchar(22) NOT NULL default ' ',
  "AAABLFSP" varchar(4) NOT NULL default ' ',
  "AAAAFWMO" varchar(4) NOT NULL default ' ',
  "AAAAFWMP" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAALARC","AAABLFSP","AAAAFWMO","AAAAFWMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWMQ" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWMS" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAFWMU" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAAAFWNB" varchar(4) NOT NULL default ' ',
  "AAAAFWNC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWNE" (
  "AAAARZQI" varchar(20) NOT NULL default ' ',
  "AAAANAKS" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZQI","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWNK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAARZQI" varchar(20) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAAFWVD" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTVR","AAAARZQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWNY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAARZQI" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR","AAAARZQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWOG" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABHJRW" varchar(3) NOT NULL default ' ',
  "AAAAFXEC" varchar(3) NOT NULL default '000',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHQRQ" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABHJRW","AAAAFXEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWOV" (
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFWZJ" varchar(22) NOT NULL default ' ',
  "AAABEIJJ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFXEG","AAAAFWZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWPB" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWPD" varchar(1) NOT NULL default '0',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFWPO" varchar(22) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWPN" ("AAAAFWPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWPP" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFWPS" varchar(128) NOT NULL default ' ',
  "AAAAYVJG" varchar(128) NOT NULL default ' ',
  "AAAAFWPU" varchar(60) NOT NULL default ' ',
  "AAAAFWPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWPX" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFWQW" varchar(22) NOT NULL default ' ',
  "AAAAFWQB" varchar(1) NOT NULL default ' ',
  "AAAAFWQC" varchar(1) NOT NULL default ' ',
  "AAABJSNO" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABHJWB" varchar(1) NOT NULL default '0',
  "AAAAFWQL" varchar(1) NOT NULL default '0',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAAAFWQN" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAAAVHIH" varchar(12) NOT NULL default ' ',
  "AAAAFWQQ" varchar(20) NOT NULL default ' ',
  "AAAAVEPU" varchar(80) NOT NULL default ' ',
  "AAABFIET" varchar(1) NOT NULL default ' ',
  "AAAAFWQT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWQV" ("AAAAFWQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWQX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWRD" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABLECD" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFWRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  UNIQUE KEY "AAAAFWRP" ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWRR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWRX" (
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
CREATE TABLE "AAAAFWSP" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWSR" varchar(22) NOT NULL default ' ',
  "AAAAMRRT" varchar(22) NOT NULL default ' ',
  "AAAAFXFR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFWTB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWTD" ("AAAAMRRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWTF" (
  "AAAAFWTI" varchar(22) NOT NULL default ' ',
  "AAAAFWTJ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFWTI","AAAAFWTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWTK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWTU" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWUT" varchar(22) NOT NULL default ' ',
  "AAAAFWUR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAAAFWUF" varchar(3) NOT NULL default ' ',
  "AAAAFWUG" varchar(3) NOT NULL default ' ',
  "AAABFFIR" varchar(1) NOT NULL default ' ',
  "AAAAFWUI" varchar(1) NOT NULL default ' ',
  "AAABEDQH" varchar(1) NOT NULL default ' ',
  "AAAAFXDE" varchar(4) NOT NULL default '0000',
  "AAAAFWUL" varchar(22) NOT NULL default ' ',
  "AAAAFWUM" varchar(22) NOT NULL default ' ',
  "AAAAFWUN" varchar(22) NOT NULL default ' ',
  "AAAAFWUO" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAAAFWUT","AAAAFWUR"),
  KEY "AAAAFWUS" ("AAAAFWUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWUU" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLNRX" varchar(1) NOT NULL default '0',
  "AAABDCEQ" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAAFWVD" varchar(8) NOT NULL default '00000000',
  "AAAAFWVE" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAAAFWVU" varchar(80) NOT NULL default ' ',
  "AAABJEVT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLTVR"),
  UNIQUE KEY "AAAAFWVL" ("AAABMLXR","AAABDCEQ"),
  KEY "AAAAFWVO" ("AAABDCEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWVQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAFWVU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWVX" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWWL" varchar(22) NOT NULL default ' ',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  "AAAAFWWB" varchar(1) NOT NULL default ' ',
  "AAAAFWWC" varchar(22) NOT NULL default ' ',
  "AAAAFWWD" varchar(100) NOT NULL default ' ',
  "AAAAFWWE" varchar(1) NOT NULL default ' ',
  "AAABBULU" varchar(10) NOT NULL default '0000000000',
  "AAAAFWWG" varchar(1) NOT NULL default ' ',
  "AAAAFWWH" varchar(1) NOT NULL default ' ',
  "AAAAFWWI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWWK" ("AAAAFWWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWWM" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABJEXI" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  UNIQUE KEY "AAAAFWWX" ("AAABMLXR"),
  KEY "AAAAFWWZ" ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWXB" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWXL" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABLRPQ" varchar(22) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABLDRF" varchar(4) NOT NULL default '0000',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABMLCA" varchar(22) NOT NULL default ' ',
  "AAAAFWXS" varchar(22) NOT NULL default ' ',
  "AAAAFWXT" varchar(22) NOT NULL default ' ',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAAAFWXW" varchar(2) NOT NULL default '00',
  "AAABLECU" varchar(22) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABLRPQ","AAABLGEL"),
  KEY "AAAAFWYG" ("AAABLRPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWYI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABLRPQ" varchar(22) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLTVR","AAABLRPQ","AAABLGEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWYS" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFWZJ" varchar(22) NOT NULL default ' ',
  "AAAAFWYV" varchar(1) NOT NULL default ' ',
  "AAAAVFLL" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAAAJUFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  KEY "AAAAFWZI" ("AAAAFWZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWZK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABJEHO" varchar(4) NOT NULL default '0000',
  "AAABJEHP" varchar(4) NOT NULL default '0000',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFWZX" (
  "AAABJEHO" varchar(4) NOT NULL default '0000',
  "AAABJEHP" varchar(4) NOT NULL default '0000',
  "AAAAVOJO" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAVOIC" varchar(1) NOT NULL default ' ',
  "AAAAVOID" varchar(27) NOT NULL default ' ',
  "AAAAVOIE" varchar(4) NOT NULL default '0000',
  "AAAAVOIF" varchar(1) NOT NULL default ' ',
  "AAABJESQ" varchar(1) NOT NULL default ' ',
  "AAABJESR" varchar(10) NOT NULL default '0000000000',
  "AAABJESS" varchar(10) NOT NULL default '0000000000',
  "AAABJESP" varchar(2) NOT NULL default '00',
  "AAABLXXV" varchar(30) NOT NULL default ' ',
  "AAAAVOJJ" varchar(1) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEHO","AAABJEHP","AAAAVOJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXAQ" (
  "AAAARZQI" varchar(20) NOT NULL default ' ',
  "AAABIJQN" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAAARZQI","AAABIJQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXAW" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABCLXK" varchar(2) NOT NULL default '00',
  "AAAAFXBE" varchar(1) NOT NULL default '0',
  "AAAAFXBA" varchar(30) NOT NULL default ' ',
  "AAAAFXBB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABCLXK","AAAAFXBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXBF" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABCLXK" varchar(2) NOT NULL default '00',
  "AAAAFXBI" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABCLXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXBL" (
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFXDE" int(11) NOT NULL default '0',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAFXBQ" smallint(6) NOT NULL default '0',
  "AAAAFXBR" text,
  PRIMARY KEY ("AAABLFYD","AAABMREL","AAAAFXDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXBV" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXBX" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABLNRX" varchar(1) NOT NULL default '0',
  "AAAAFXCA" varchar(22) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABFPTL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXCH" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABCLXK" varchar(2) NOT NULL default '00',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFXCL" varchar(6) NOT NULL default ' ',
  "AAABGXAJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLTVR","AAABCLXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXCP" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXDE" varchar(12) NOT NULL default '000000000000',
  "AAABCLXK" varchar(2) NOT NULL default '00',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAAAFXDE","AAABCLXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXCX" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXDE" varchar(12) NOT NULL default '000000000000',
  "AAAAFXDA" varchar(60) NOT NULL default ' ',
  "AAAAFXDB" varchar(60) NOT NULL default ' ',
  "AAAAFXDC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAAAFXDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXDF" (
  "AAABLTVR" varchar(5) NOT NULL default '00000',
  "AAAAFXDH" varchar(32) NOT NULL default ' ',
  "AAAAFXDI" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAAAFXDK" varchar(6) NOT NULL default '000000',
  "AAAARGUD" varchar(6) NOT NULL default '000000',
  "AAAAFXDM" varchar(1) NOT NULL default ' ',
  "AAAAFXDN" varchar(1) NOT NULL default ' ',
  "AAAAFXDO" varchar(1) NOT NULL default ' ',
  "AAAAFXDP" varchar(1) NOT NULL default ' ',
  "AAAAFXDQ" varchar(1) NOT NULL default ' ',
  "AAABIVEF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXDT" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAAAGIAY" varchar(20) NOT NULL default ' ',
  "AAAAFXEG" varchar(20) NOT NULL default ' ',
  "AAAAFXDZ" varchar(20) NOT NULL default ' ',
  "AAAAFXEA" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(3) NOT NULL default ' ',
  "AAAAFXEC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXEE" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAFXEG" varchar(22) NOT NULL default ' ',
  "AAAAFXEH" varchar(30) NOT NULL default ' ',
  "AAAAFXEI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXEK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAAVSPT" varchar(4) NOT NULL default '0000',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAAAFXFR" varchar(22) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  "AAAAFXEQ" varchar(4) NOT NULL default ' ',
  "AAABLQOK" varchar(4) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXEZ" (
  "AAAALARC" varchar(22) NOT NULL default ' ',
  "AAAAVSPT" varchar(4) NOT NULL default '0000',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  "AAABLQOK" varchar(4) NOT NULL default ' ',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAALARC","AAAAVSPT","AAABLTLR","AAABMECK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXFK" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLXR" varchar(20) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAAAFXFR" varchar(22) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABLEJW" varchar(128) NOT NULL default ' ',
  "AAAAFXFU" varchar(22) NOT NULL default ' ',
  "AAAAFXFV" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXFX" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  "AAABLLNH" varchar(6) NOT NULL default '000000',
  "AAAAFXQK" varchar(28) NOT NULL default ' ',
  "AAAAFXQL" varchar(1) NOT NULL default ' ',
  "AAAATISY" varchar(1) NOT NULL default ' ',
  "AAAAFXGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXGM" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAFXRB" varchar(12) NOT NULL default ' ',
  "AAAAFXQV" varchar(1) NOT NULL default ' ',
  "AAAAFXQW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAAAFXRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXGZ" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABDIPJ" varchar(2) NOT NULL default '00',
  "AAAAFXSL" varchar(30) NOT NULL default ' ',
  "AAAAFXSM" varchar(30) NOT NULL default ' ',
  "AAAAFXSN" varchar(1) NOT NULL default ' ',
  "AAAAFXSO" varchar(132) NOT NULL default ' ',
  "AAAAFXSP" varchar(12) NOT NULL default ' ',
  "AAAAFXSQ" varchar(2) NOT NULL default ' ',
  "AAAAFXSR" varchar(12) NOT NULL default ' ',
  "AAAAFXSS" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFXSU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAABDIPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXHU" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAFXTA" varchar(3) NOT NULL default '000',
  "AAAAFXSD" varchar(30) NOT NULL default ' ',
  "AAAAFXSE" varchar(30) NOT NULL default ' ',
  "AAAAFXSF" varchar(1) NOT NULL default ' ',
  "AAAAFXSG" varchar(132) NOT NULL default ' ',
  "AAAAFXSH" varchar(12) NOT NULL default ' ',
  "AAAAFXSI" varchar(2) NOT NULL default ' ',
  "AAAAFXSJ" varchar(12) NOT NULL default ' ',
  "AAAAFXSK" varchar(1) NOT NULL default ' ',
  "AAAAFXSL" varchar(30) NOT NULL default ' ',
  "AAAAFXSM" varchar(30) NOT NULL default ' ',
  "AAAAFXSN" varchar(1) NOT NULL default ' ',
  "AAAAFXSO" varchar(132) NOT NULL default ' ',
  "AAAAFXSP" varchar(12) NOT NULL default ' ',
  "AAAAFXSQ" varchar(2) NOT NULL default ' ',
  "AAAAFXSR" varchar(12) NOT NULL default ' ',
  "AAAAFXSS" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFXSU" varchar(3) NOT NULL default ' ',
  "AAAAFXSV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAAAFXTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXIY" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAREEY" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABMRHK","AAAAREEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXJI" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXTP" varchar(30) NOT NULL default ' ',
  "AAAAFXTQ" varchar(1) NOT NULL default ' ',
  "AAAAFXTR" varchar(3) NOT NULL default ' ',
  "AAAAFXTS" varchar(3) NOT NULL default ' ',
  "AAAAFXTT" varchar(3) NOT NULL default ' ',
  "AAAAFXTU" varchar(1) NOT NULL default ' ',
  "AAAAFXTV" varchar(1) NOT NULL default ' ',
  "AAABAFHV" varchar(30) NOT NULL default ' ',
  "AAAAFXTX" varchar(12) NOT NULL default ' ',
  "AAAAFXTY" varchar(1) NOT NULL default ' ',
  "AAAAFXTZ" varchar(1) NOT NULL default ' ',
  "AAAAFXWG" varchar(1) NOT NULL default ' ',
  "AAAAFXWH" varchar(1) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAAAFXUD" varchar(1) NOT NULL default ' ',
  "AAAAFXUE" varchar(12) NOT NULL default ' ',
  "AAAAFXUF" varchar(8) NOT NULL default '00000000',
  "AAAAFXUG" varchar(6) NOT NULL default '000000',
  "AAABDSZA" varchar(86) NOT NULL default ' ',
  "AAAAFXUI" varchar(1) NOT NULL default ' ',
  "AAABGQTH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXKK" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXUR" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABIGOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXKU" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABERAT" varchar(20) NOT NULL default ' ',
  "AAAARKIE" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAABHZOI" varchar(12) NOT NULL default ' ',
  "AAAAJUEK" varchar(12) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABJSDF" varchar(4) NOT NULL default '0000',
  "AAABIZHF" varchar(1) NOT NULL default ' ',
  "AAAAFXVJ" varchar(1) NOT NULL default ' ',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXVL" varchar(1) NOT NULL default ' ',
  "AAAAFXVM" varchar(1) NOT NULL default ' ',
  "AAAAFXVN" varchar(2) NOT NULL default ' ',
  "AAAAFXVO" varchar(3) NOT NULL default '000',
  "AAAAFXVP" varchar(30) NOT NULL default ' ',
  "AAAAFXVQ" varchar(30) NOT NULL default ' ',
  "AAAAFXVR" varchar(3) NOT NULL default '000',
  "AAAAFXVS" varchar(1) NOT NULL default ' ',
  "AAAAFXVT" varchar(12) NOT NULL default ' ',
  "AAAAFXVU" varchar(2) NOT NULL default ' ',
  "AAAAFXVV" varchar(12) NOT NULL default ' ',
  "AAAAFXVW" varchar(132) NOT NULL default ' ',
  "AAAAJZTB" varchar(10) NOT NULL default ' ',
  "AAAAFXVY" varchar(30) NOT NULL default ' ',
  "AAAAFXVZ" varchar(1) NOT NULL default ' ',
  "AAAAFXWA" varchar(30) NOT NULL default ' ',
  "AAAAFXWB" varchar(30) NOT NULL default ' ',
  "AAAAFXWC" varchar(1) NOT NULL default ' ',
  "AAAAFXWD" varchar(20) NOT NULL default ' ',
  "AAAAFXWE" varchar(1) NOT NULL default ' ',
  "AAAAFXWF" varchar(1) NOT NULL default ' ',
  "AAAAFXWG" varchar(1) NOT NULL default ' ',
  "AAAAFXWH" varchar(1) NOT NULL default ' ',
  "AAAAFXWI" varchar(1) NOT NULL default ' ',
  "AAAAFXWJ" varchar(30) NOT NULL default ' ',
  "AAAAFXWK" varchar(1) NOT NULL default ' ',
  "AAAAFXWL" varchar(1) NOT NULL default ' ',
  "AAAAFXWM" varchar(20) NOT NULL default ' ',
  "AAAAFXWN" varchar(3) NOT NULL default ' ',
  "AAAAFXWO" varchar(20) NOT NULL default ' ',
  "AAAAFXWP" varchar(3) NOT NULL default ' ',
  "AAABIXJD" varchar(1) NOT NULL default ' ',
  "AAAAFXWR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXMU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAAAFXXD" varchar(180) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK"),
  KEY "AAAAFXNH" ("AAABHGII","AAABLGEY","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXNL" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXYI" varchar(12) NOT NULL default ' ',
  "AAAAFXXZ" varchar(3) NOT NULL default '000',
  "AAAAFXXT" varchar(12) NOT NULL default ' ',
  "AAAAFXXU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAAAFXYI","AAAAFXXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXNY" (
  "AAAAFXOD" varchar(30) NOT NULL default ' ',
  "AAAAFXOE" varchar(30) NOT NULL default ' ',
  "AAABFZUW" varchar(30) NOT NULL default ' ',
  "AAAAFXOC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAFXOD","AAAAFXOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXOF" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXYM" varchar(3) NOT NULL default '000',
  "AAABJEMN" varchar(30) NOT NULL default ' ',
  "AAABJEML" varchar(4) NOT NULL default '0000',
  "AAAAFXYH" varchar(12) NOT NULL default ' ',
  "AAAAFXYI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAAAFXYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXOS" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAFXYX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMKXW","AAAAFXYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXPD" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABLOSB" varchar(3) NOT NULL default '000',
  "AAAAZGSF" varchar(1) NOT NULL default ' ',
  "AAAAFXZE" varchar(30) NOT NULL default ' ',
  "AAAAFXZF" varchar(30) NOT NULL default ' ',
  "AAAAFXZG" varchar(1) NOT NULL default ' ',
  "AAAAFXZH" varchar(20) NOT NULL default ' ',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXZJ" varchar(30) NOT NULL default ' ',
  "AAAAFXZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABLOSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXPU" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFYAD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABMREL","AAAAFYAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXQB" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  "AAABLLNH" varchar(6) NOT NULL default '000000',
  "AAAAFXQK" varchar(28) NOT NULL default ' ',
  "AAAAFXQL" varchar(1) NOT NULL default ' ',
  "AAAATISY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXQP" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAFXRB" varchar(12) NOT NULL default ' ',
  "AAAAFXQV" varchar(1) NOT NULL default ' ',
  "AAAAFXQW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAAAFXRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXRC" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABDIPJ" varchar(2) NOT NULL default '00',
  "AAAAFXSL" varchar(30) NOT NULL default ' ',
  "AAAAFXSM" varchar(30) NOT NULL default ' ',
  "AAAAFXSN" varchar(1) NOT NULL default ' ',
  "AAAAFXSO" varchar(132) NOT NULL default ' ',
  "AAAAFXSP" varchar(12) NOT NULL default ' ',
  "AAAAFXSQ" varchar(2) NOT NULL default ' ',
  "AAAAFXSR" varchar(12) NOT NULL default ' ',
  "AAAAFXSS" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFXSU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAABDIPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXRX" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAFXTA" varchar(3) NOT NULL default '000',
  "AAAAFXSD" varchar(30) NOT NULL default ' ',
  "AAAAFXSE" varchar(30) NOT NULL default ' ',
  "AAAAFXSF" varchar(1) NOT NULL default ' ',
  "AAAAFXSG" varchar(132) NOT NULL default ' ',
  "AAAAFXSH" varchar(12) NOT NULL default ' ',
  "AAAAFXSI" varchar(2) NOT NULL default ' ',
  "AAAAFXSJ" varchar(12) NOT NULL default ' ',
  "AAAAFXSK" varchar(1) NOT NULL default ' ',
  "AAAAFXSL" varchar(30) NOT NULL default ' ',
  "AAAAFXSM" varchar(30) NOT NULL default ' ',
  "AAAAFXSN" varchar(1) NOT NULL default ' ',
  "AAAAFXSO" varchar(132) NOT NULL default ' ',
  "AAAAFXSP" varchar(12) NOT NULL default ' ',
  "AAAAFXSQ" varchar(2) NOT NULL default ' ',
  "AAAAFXSR" varchar(12) NOT NULL default ' ',
  "AAAAFXSS" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAAFXSU" varchar(3) NOT NULL default ' ',
  "AAAAFXSV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK","AAAAFXTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXTB" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAREEY" varchar(12) NOT NULL default ' ',
  "AAAARZWY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABMRHK","AAAAREEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXTL" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXTP" varchar(30) NOT NULL default ' ',
  "AAAAFXTQ" varchar(1) NOT NULL default ' ',
  "AAAAFXTR" varchar(3) NOT NULL default ' ',
  "AAAAFXTS" varchar(3) NOT NULL default ' ',
  "AAAAFXTT" varchar(3) NOT NULL default ' ',
  "AAAAFXTU" varchar(1) NOT NULL default ' ',
  "AAAAFXTV" varchar(1) NOT NULL default ' ',
  "AAABAFHV" varchar(30) NOT NULL default ' ',
  "AAAAFXTX" varchar(12) NOT NULL default ' ',
  "AAAAFXTY" varchar(1) NOT NULL default ' ',
  "AAAAFXTZ" varchar(1) NOT NULL default ' ',
  "AAAAFXWG" varchar(1) NOT NULL default ' ',
  "AAAAFXWH" varchar(1) NOT NULL default ' ',
  "AAABMBTQ" varchar(16) NOT NULL default ' ',
  "AAAAFXUD" varchar(1) NOT NULL default ' ',
  "AAAAFXUE" varchar(12) NOT NULL default ' ',
  "AAAAFXUF" varchar(8) NOT NULL default '00000000',
  "AAAAFXUG" varchar(6) NOT NULL default '000000',
  "AAABDSZA" varchar(86) NOT NULL default ' ',
  "AAAAFXUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXUM" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXUR" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABIGOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXUW" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABERAT" varchar(20) NOT NULL default ' ',
  "AAAARKIE" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAABHZOI" varchar(12) NOT NULL default ' ',
  "AAAAJUEK" varchar(12) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABJSDF" varchar(4) NOT NULL default '0000',
  "AAABIZHF" varchar(1) NOT NULL default ' ',
  "AAAAFXVJ" varchar(1) NOT NULL default ' ',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXVL" varchar(1) NOT NULL default ' ',
  "AAAAFXVM" varchar(1) NOT NULL default ' ',
  "AAAAFXVN" varchar(2) NOT NULL default ' ',
  "AAAAFXVO" varchar(3) NOT NULL default '000',
  "AAAAFXVP" varchar(30) NOT NULL default ' ',
  "AAAAFXVQ" varchar(30) NOT NULL default ' ',
  "AAAAFXVR" varchar(3) NOT NULL default '000',
  "AAAAFXVS" varchar(1) NOT NULL default ' ',
  "AAAAFXVT" varchar(12) NOT NULL default ' ',
  "AAAAFXVU" varchar(2) NOT NULL default ' ',
  "AAAAFXVV" varchar(12) NOT NULL default ' ',
  "AAAAFXVW" varchar(132) NOT NULL default ' ',
  "AAAAJZTB" varchar(10) NOT NULL default ' ',
  "AAAAFXVY" varchar(30) NOT NULL default ' ',
  "AAAAFXVZ" varchar(1) NOT NULL default ' ',
  "AAAAFXWA" varchar(30) NOT NULL default ' ',
  "AAAAFXWB" varchar(30) NOT NULL default ' ',
  "AAAAFXWC" varchar(1) NOT NULL default ' ',
  "AAAAFXWD" varchar(20) NOT NULL default ' ',
  "AAAAFXWE" varchar(1) NOT NULL default ' ',
  "AAAAFXWF" varchar(1) NOT NULL default ' ',
  "AAAAFXWG" varchar(1) NOT NULL default ' ',
  "AAAAFXWH" varchar(1) NOT NULL default ' ',
  "AAAAFXWI" varchar(1) NOT NULL default ' ',
  "AAAAFXWJ" varchar(30) NOT NULL default ' ',
  "AAAAFXWK" varchar(1) NOT NULL default ' ',
  "AAAAFXWL" varchar(1) NOT NULL default ' ',
  "AAAAFXWM" varchar(20) NOT NULL default ' ',
  "AAAAFXWN" varchar(3) NOT NULL default ' ',
  "AAAAFXWO" varchar(20) NOT NULL default ' ',
  "AAAAFXWP" varchar(3) NOT NULL default ' ',
  "AAABIXJD" varchar(1) NOT NULL default ' ',
  "AAAAFXWR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXWW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAAAFXXD" varchar(180) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII","AAABLGEY","AAABLTXI","AAABMRHK"),
  KEY "AAAAFXXJ" ("AAABHGII","AAABLGEY","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXXN" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXYI" varchar(12) NOT NULL default ' ',
  "AAAAFXXZ" varchar(3) NOT NULL default '000',
  "AAAAFXXT" varchar(12) NOT NULL default ' ',
  "AAAAFXXU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAAAFXYI","AAAAFXXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXYA" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAAAFXYM" varchar(3) NOT NULL default '000',
  "AAABJEMN" varchar(30) NOT NULL default ' ',
  "AAABJEML" varchar(4) NOT NULL default '0000',
  "AAAAFXYH" varchar(12) NOT NULL default ' ',
  "AAAAFXYI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAAAFXYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXYN" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAFXYX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABMKXW","AAAAFXYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXYY" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABLOSB" varchar(3) NOT NULL default '000',
  "AAAAZGSF" varchar(1) NOT NULL default ' ',
  "AAAAFXZE" varchar(30) NOT NULL default ' ',
  "AAAAFXZF" varchar(30) NOT NULL default ' ',
  "AAAAFXZG" varchar(1) NOT NULL default ' ',
  "AAAAFXZH" varchar(20) NOT NULL default ' ',
  "AAABIGOT" varchar(3) NOT NULL default '000',
  "AAAAFXZJ" varchar(30) NOT NULL default ' ',
  "AAAAFXZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABLGEY","AAABLTXI","AAABLOSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXZP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFXZX" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAFYAD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII","AAABMREL","AAAAFYAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYAE" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAAAFYBD" varchar(30) NOT NULL default ' ',
  "AAAAFYBE" varchar(30) NOT NULL default ' ',
  "AAAAFYBF" varchar(30) NOT NULL default ' ',
  "AAAAFYBG" varchar(3) NOT NULL default '000',
  "AAAAFYBH" varchar(30) NOT NULL default ' ',
  "AAAAFYBI" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAAAFYBK" varchar(40) NOT NULL default ' ',
  "AAAAFYBL" varchar(3) NOT NULL default ' ',
  "AAAAFYBM" varchar(8) NOT NULL default ' ',
  "AAAAFYBN" varchar(3) NOT NULL default ' ',
  "AAAAFYBO" varchar(60) NOT NULL default ' ',
  "AAAAKLVZ" varchar(4) NOT NULL default ' ',
  "AAAAFYBQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYAV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYBB" (
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAAAFYBD" varchar(30) NOT NULL default ' ',
  "AAAAFYBE" varchar(30) NOT NULL default ' ',
  "AAAAFYBF" varchar(30) NOT NULL default ' ',
  "AAAAFYBG" varchar(3) NOT NULL default '000',
  "AAAAFYBH" varchar(30) NOT NULL default ' ',
  "AAAAFYBI" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAAAFYBK" varchar(40) NOT NULL default ' ',
  "AAAAFYBL" varchar(3) NOT NULL default ' ',
  "AAAAFYBM" varchar(8) NOT NULL default ' ',
  "AAAAFYBN" varchar(3) NOT NULL default ' ',
  "AAAAFYBO" varchar(60) NOT NULL default ' ',
  "AAAAKLVZ" varchar(4) NOT NULL default ' ',
  "AAAAFYBQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYBS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYBY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHGII" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHGII","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYCG" (
  "AAAAVJFM" varchar(50) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAAGGQG" varchar(32) NOT NULL default ' ',
  "AAABGHFI" varchar(32) NOT NULL default ' ',
  "AAABIJNF" varchar(32) NOT NULL default ' ',
  "AAAAFYCV" varchar(200) NOT NULL default ' ',
  "AAABFVER" decimal(15,0) NOT NULL default '0',
  "AAAARVIK" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAVJFM","AAABIMRZ","AAAAGGQG","AAABGHFI"),
  KEY "AAAAFYCT" ("AAABIMRZ","AAAAFYCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYCW" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAVJFM" varchar(50) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYDH" (
  "AAABKNGX" varchar(32) NOT NULL default ' ',
  "AAABEEVC" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAGGOH" varchar(30) NOT NULL default ' ',
  "AAABLRFA" varchar(5) NOT NULL default '00000',
  "AAABIIDV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABIJNF" varchar(32) NOT NULL default ' ',
  "AAAAGGNU" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(255) NOT NULL default ' ',
  "AAAAGGNW" varchar(255) NOT NULL default ' ',
  "AAAAXLZS" VARBINARY(255),
  "AAABMBXY" VARBINARY(255),
  "AAAAGGNZ" VARBINARY(255),
  "AAAAGGOA" varchar(5) NOT NULL default '00000',
  "AAAAGGOB" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABKNGX","AAABEEVC","AAABMRHS","AAABFNHW","AAAAGGOH","AAABLRFA","AAABIIDV","AAABLFZK","AAABLEHK","AAABMLPO"),
  KEY "AAAAFYEM" ("AAABIJNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYEO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAAAGGQG" VARBINARY(255) NOT NULL,
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEKS","AAAAGGQG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYEW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABILJL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPJD","AAABILJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYFD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABILJL" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPJD","AAABILJL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYFN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAXFFU" varchar(10) NOT NULL default ' ',
  "AAAAMKRU" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGU" VARBINARY(255) NOT NULL,
  "AAAAFYHJ" VARBINARY(255) NOT NULL,
  "AAAAFYHK" VARBINARY(255) NOT NULL,
  "AAAAFYGZ" VARBINARY(255),
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAAAUXJU" VARBINARY(255),
  "AAAAXFFU" varchar(10) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABEEUL" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGU"(255),"AAAAFYHJ"(255),"AAAAFYHK"(255)),
  KEY "AAAAFYGM" ("AAABLUXQ","AAAAFYGZ"(255)),
  KEY "AAAAFYGP" ("AAABLUXQ","AAAAXFFU","AAAAFYHK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYGT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDOGU" VARBINARY(255) NOT NULL,
  "AAAAFYHJ" VARBINARY(255) NOT NULL,
  "AAAAFYHK" VARBINARY(255) NOT NULL,
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAAAFYGZ" VARBINARY(255),
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAAAUXJU" VARBINARY(255),
  "AAAAXFFU" varchar(10) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGQZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDOGU"(255),"AAAAFYHJ"(255),"AAAAFYHK"(255),"AAABJDPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYHM" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAAAFYHO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYHQ" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYHW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVJFM" varchar(50) NOT NULL default ' ',
  "AAABKNGX" VARBINARY(255),
  "AAAAFYIA" varchar(1) NOT NULL default ' ',
  "AAAARXTK" varchar(1) NOT NULL default ' ',
  "AAAAFYIC" varchar(60) NOT NULL default ' ',
  "AAAAFYID" varchar(60) NOT NULL default ' ',
  "AAAAFYIE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVJFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYIH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVJFM" varchar(50) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVJFM","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYIP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEDQH" varchar(6) NOT NULL default ' ',
  "AAABKNGX" VARBINARY(255),
  "AAAAFYIT" varchar(1) NOT NULL default ' ',
  "AAABFNHE" varchar(1) NOT NULL default ' ',
  "AAAAFYIV" varchar(2) NOT NULL default ' ',
  "AAAAFYIW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYIZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAAAFYJC" varchar(10) NOT NULL default ' ',
  "AAAAFYJD" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAGGPT" varchar(8) NOT NULL default ' ',
  "AAABJRGL" varchar(1) NOT NULL default ' ',
  "AAAAFYJH" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAAGGNZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYJM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAATVFS" varchar(16) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABKNGX" VARBINARY(255),
  "AAABEFNM" smallint(6) NOT NULL default '0',
  "AAAAGGNZ" varchar(1) NOT NULL default ' ',
  "AAAAFYJU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIMRZ","AAAATVFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYJY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAGGOH" varchar(30) NOT NULL default ' ',
  "AAABLRFA" varchar(5) NOT NULL default '00000',
  "AAABIIDV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABEEUL" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAGGNU" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(255) NOT NULL default ' ',
  "AAAAGGNW" varchar(255) NOT NULL default ' ',
  "AAAAXLZS" VARBINARY(255),
  "AAABMBXY" VARBINARY(255),
  "AAAAGGNZ" VARBINARY(255),
  "AAAAGGOA" varchar(5) NOT NULL default '00000',
  "AAAAGGOB" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABFNHW","AAAAGGOH","AAABLRFA","AAABIIDV","AAABLFZK","AAABLEHK","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYLF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAGGOH" varchar(30) NOT NULL default ' ',
  "AAABLRFA" varchar(5) NOT NULL default '00000',
  "AAABIIDV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGQZW" varchar(1) NOT NULL default ' ',
  "AAAAGGNU" varchar(5) NOT NULL default '00000',
  "AAABCRGY" varchar(255) NOT NULL default ' ',
  "AAAAGGNW" varchar(255) NOT NULL default ' ',
  "AAAAXLZS" VARBINARY(255),
  "AAABMBXY" VARBINARY(255),
  "AAAAGGNZ" VARBINARY(255),
  "AAAAGGOA" varchar(5) NOT NULL default '00000',
  "AAAAGGOB" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABJDPL","AAABFNHW","AAAAGGOH","AAABLRFA","AAABIIDV","AAABLFZK","AAABLEHK","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYMN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAARZMC" varchar(10) NOT NULL default ' ',
  "AAABLFWP" varchar(1) NOT NULL default ' ',
  "AAABCPET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAARZMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYMU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAFYNE" varchar(30) NOT NULL default ' ',
  "AAAARZMC" varchar(10) NOT NULL default ' ',
  "AAABKNCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPJD","AAABFNHW","AAAAFYNE","AAAARZMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYNG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAARZMC" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAARZMC","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAAAFYOE" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABEDAG" varchar(1) NOT NULL default ' ',
  "AAAAGBDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFYQY" varchar(10) NOT NULL default ' ',
  "AAAAFYQU" varchar(10) NOT NULL default ' ',
  "AAAAFYQV" varchar(1) NOT NULL default ' ',
  "AAAAFYQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFYQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGBQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAGBQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAFYSB" varchar(2) NOT NULL default ' ',
  "AAAAFYSC" int(11) NOT NULL default '0',
  "AAAAGBRA" varchar(32) NOT NULL default ' ',
  "AAAAFYRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAFYSB","AAAAFYSC"),
  KEY "AAAAFYSD" ("AAAAGBRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAFYST" int(11) NOT NULL default '0',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABLEJW" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAFYST"),
  KEY "AAAAFYSU" ("AAABLEJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(32) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAAAFYTG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRU"),
  KEY "AAAAFYTE" ("AAABLEKS","AAAAFYTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRU" varchar(32) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABFSPQ" varchar(50) NOT NULL default ' ',
  "AAABGMMY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABIMRU","AAABIMRZ","AAABFSPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYTY" (
  "AAABIMRU" varchar(32) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMRU","AAABLEKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYOH" varchar(8) NOT NULL default ' ',
  "AAABGYPG" varchar(25) NOT NULL default ' ',
  "AAAAFYUU" varchar(25) NOT NULL default ' ',
  "AAAAFYUV" smallint(6) NOT NULL default '0',
  "AAAAFYUW" smallint(6) NOT NULL default '0',
  "AAAAFYUX" smallint(6) NOT NULL default '0',
  "AAAAFYUY" smallint(6) NOT NULL default '0',
  "AAAAFYUZ" varchar(1) NOT NULL default ' ',
  "AAAAFYVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYOH" varchar(8) NOT NULL default ' ',
  "AAABGYPG" varchar(25) NOT NULL default ' ',
  "AAAAFYUU" varchar(25) NOT NULL default ' ',
  "AAAAFYUV" smallint(6) NOT NULL default '0',
  "AAAAFYUW" smallint(6) NOT NULL default '0',
  "AAAAFYUX" smallint(6) NOT NULL default '0',
  "AAAAFYUY" smallint(6) NOT NULL default '0',
  "AAAAFYUZ" varchar(1) NOT NULL default ' ',
  "AAAAFYVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBRA" varchar(32) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAABLEJW" varchar(255) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAAPTTA" varchar(10) NOT NULL default ' ',
  "AAAAFYVL" varchar(1) NOT NULL default ' ',
  "AAAAFYVM" varchar(12) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAAAGBRB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBRA","AAABHVMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKERC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLLR"),
  KEY "AAAAFYZS" ("AAABLLLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFYZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFZBB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAFZBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDBR" varchar(1) NOT NULL default ' ',
  "AAAAFZBB" varchar(15) NOT NULL default ' ',
  "AAAAFZAY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDBR","AAAAFZBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABEDCG" varchar(1) NOT NULL default ' ',
  "AAABEWYY" varchar(1) NOT NULL default ' ',
  "AAAAFZBI" varchar(20) NOT NULL default ' ',
  "AAAAFZBJ" varchar(1) NOT NULL default ' ',
  "AAAAFZBK" varchar(1) NOT NULL default ' ',
  "AAAAFZBL" varchar(1) NOT NULL default ' ',
  "AAAAFZBM" varchar(1) NOT NULL default ' ',
  "AAAAFZBN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABHVMV","AAABEDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZEY" (
  "AAABICFV" varchar(32) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAABLEJW" varchar(255) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABICFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH"),
  KEY "AAAAFZIO" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATGRY" varchar(10) NOT NULL default ' ',
  "AAABGMMY" smallint(6) NOT NULL default '0',
  "AAABLFOX" varchar(5) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATGRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATGRY" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAATGRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCANE" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCANE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCANE" varchar(10) NOT NULL default ' ',
  "AAAATOVN" varchar(30) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCANE","AAAATOVN","AAABLEKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCANE" varchar(10) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABEDCB" varchar(255) NOT NULL default ' ',
  "AAAAFZNS" varchar(1) NOT NULL default ' ',
  "AAAAFZNT" varchar(1) NOT NULL default ' ',
  "AAAAPTTA" varchar(10) NOT NULL default ' ',
  "AAAAFZNV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCANE","AAABHVMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCANE" varchar(10) NOT NULL default ' ',
  "AAAATGRY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCANE","AAAATGRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNL"),
  KEY "AAAAFZPS" ("AAABLLNN"),
  KEY "AAAAFZPU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMADG" varchar(12) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLXIY","AAABMADG","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMADG" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLXIY","AAABMADG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAAAFZRN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAAAFZRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABECST" varchar(20) NOT NULL default ' ',
  "AAAAPTTH" varchar(1) NOT NULL default ' ',
  "AAAAPTTI" varchar(2) NOT NULL default '00',
  "AAAAPTTA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAAAPTTO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAAAPTTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABGPQK" varchar(2) NOT NULL default ' ',
  "AAABHGFF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABGPQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAAAZGSF" varchar(50) NOT NULL default ' ',
  "AAAAFZSY" smallint(6) NOT NULL default '0',
  "AAABDNQK" varchar(50) NOT NULL default ' ',
  "AAAAVLJZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEKS","AAABMOHM","AAAAZGSF","AAAAFZSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAQINM" varchar(4) NOT NULL default '0000',
  "AAAAYSNK" varchar(4) NOT NULL default '0000',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAAAFZUB" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAAAQINM","AAAAYSNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAFZUB" VARBINARY(255) NOT NULL,
  "AAAAFZTN" double NOT NULL default '0',
  "AAAAFZTO" double NOT NULL default '0',
  "AAAAFZTP" double NOT NULL default '0',
  "AAAAFZTQ" double NOT NULL default '0',
  "AAAAFZTR" double NOT NULL default '0',
  "AAAAFZTS" double NOT NULL default '0',
  "AAAAFZTT" double NOT NULL default '0',
  "AAAAFZTU" double NOT NULL default '0',
  "AAAAFZTV" double NOT NULL default '0',
  "AAAAFZTW" double NOT NULL default '0',
  "AAAAFZTX" double NOT NULL default '0',
  "AAAAFZTY" double NOT NULL default '0',
  "AAABEIJR" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAFZUB"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZUC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABLRXL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZUJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABLRXL" varchar(12) NOT NULL default ' ',
  "AAAAFZUO" varchar(20) NOT NULL default ' ',
  "AAAAFZUP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZUU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABLQGL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZVB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABLQGL" varchar(3) NOT NULL default ' ',
  "AAAAFZVG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZVL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAGYSC" varchar(2) NOT NULL default ' ',
  "AAAAFZVP" varchar(1) NOT NULL default ' ',
  "AAAAFZVQ" varchar(1) NOT NULL default ' ',
  "AAAAFZVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAAAGYSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZVV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAGYSC" varchar(2) NOT NULL default ' ',
  "AAABLLCX" varchar(20) NOT NULL default ' ',
  "AAABKYOM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAAAGYSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZWG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAAFZWK" varchar(4) NOT NULL default '0000',
  "AAAAFZWL" varchar(3) NOT NULL default ' ',
  "AAAAFZWM" varchar(4) NOT NULL default '0000',
  "AAAAFZWN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZWR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(20) NOT NULL default ' ',
  "AAABKYOM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZXC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAFZYJ" varchar(2) NOT NULL default ' ',
  "AAAAFZYK" varchar(40) NOT NULL default ' ',
  "AAAAFZYL" varchar(4) NOT NULL default ' ',
  "AAAAFZYM" varchar(15) NOT NULL default ' ',
  "AAAAFZYN" varchar(4) NOT NULL default '0000',
  "AAAAFZXK" varchar(40) NOT NULL default ' ',
  "AAAAFZXL" varchar(4) NOT NULL default ' ',
  "AAABDHJA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAAAFZYJ","AAAAFZYK","AAAAFZYL","AAAAFZYM","AAAAFZYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZXU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAFZYJ" varchar(2) NOT NULL default ' ',
  "AAAAFZYK" varchar(40) NOT NULL default ' ',
  "AAAAFZYL" varchar(4) NOT NULL default ' ',
  "AAAAFZYM" varchar(15) NOT NULL default ' ',
  "AAAAFZYN" varchar(4) NOT NULL default '0000',
  "AAAAFZYD" varchar(40) NOT NULL default ' ',
  "AAAAFZYE" varchar(20) NOT NULL default ' ',
  "AAAAFZYF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAAAFZYJ","AAAAFZYK","AAAAFZYL","AAAAFZYM","AAAAFZYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZYO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZYT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGAEZ" varchar(4) NOT NULL default ' ',
  "AAABLLCX" varchar(20) NOT NULL default ' ',
  "AAABKYOM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGAEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZZC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAFZZF" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAAAFZZH" varchar(2) NOT NULL default ' ',
  "AAAAFZZI" varchar(2) NOT NULL default ' ',
  "AAAAFZZJ" varchar(30) NOT NULL default ' ',
  "AAAAFZZK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZZN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAFZZR" varchar(20) NOT NULL default ' ',
  "AAAAFZZS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAFZZW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAFN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAAB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGAFN" varchar(2) NOT NULL default ' ',
  "AAABLLCX" varchar(20) NOT NULL default ' ',
  "AAABKYOM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGAFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAAK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABASZN" varchar(2) NOT NULL default ' ',
  "AAAARXTK" varchar(2) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAAGAAR" varchar(1) NOT NULL default ' ',
  "AAAAGAAS" varchar(1) NOT NULL default ' ',
  "AAAAGAAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAABASZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABASZN" varchar(2) NOT NULL default ' ',
  "AAABLLCX" varchar(20) NOT NULL default ' ',
  "AAABKYOM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAGADQ","AAABASZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGABI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAAAGYSC" varchar(2) NOT NULL default ' ',
  "AAABASZN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGADQ","AAAAGYSC","AAABASZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGABR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKXW" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABMKXW","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGACB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKXW" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABMDEN","AAABMKXW","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGACN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAKC" VARBINARY(255) NOT NULL,
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABEDIF" varchar(12) NOT NULL default ' ',
  "AAAAGACU" varchar(2) NOT NULL default ' ',
  "AAAAGACV" varchar(12) NOT NULL default ' ',
  "AAAAGACW" varchar(2) NOT NULL default ' ',
  "AAAAGADR" decimal(15,0) NOT NULL default '0',
  "AAAAGADS" decimal(15,0) NOT NULL default '0',
  "AAAAGACZ" decimal(15,0) NOT NULL default '0',
  "AAAAGADA" decimal(15,0) NOT NULL default '0',
  "AAAAGADB" decimal(15,0) NOT NULL default '0',
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAQOAP" varchar(20) NOT NULL default ' ',
  "AAAAGADQ" varchar(6) NOT NULL default ' ',
  "AAABLRXL" varchar(12) NOT NULL default ' ',
  "AAABENKB" varchar(60) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAKC"(255)),
  KEY "AAAAGADL" ("AAABLUXQ","AAABLENL"),
  KEY "AAAAGADO" ("AAABMRFO","AAAAGADQ","AAAAGADR","AAAAGADS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGADT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAEK" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAGAET" VARBINARY(255),
  "AAAAGADZ" VARBINARY(255),
  "AAAAGAEA" VARBINARY(255),
  "AAAAGAEB" VARBINARY(255),
  "AAAAGAEC" VARBINARY(255),
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAGAEQ" varchar(12) NOT NULL default ' ',
  "AAAAGAEF" varchar(12) NOT NULL default ' ',
  "AAAAGAEG" varchar(12) NOT NULL default ' ',
  "AAAAGAEH" varchar(12) NOT NULL default ' ',
  "AAAAGAEI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAEK"(255)),
  KEY "AAAAGAEL" ("AAABLUXQ","AAAAGAKC"(255)),
  KEY "AAAAGAEO" ("AAABLUXQ","AAAAGAEQ"),
  KEY "AAAAGAER" ("AAABLUXQ","AAAAGAET"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAEU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAFD" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAGAEZ" varchar(4) NOT NULL default ' ',
  "AAABMLXR" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAFD"(255)),
  KEY "AAAAGAFE" ("AAABLUXQ","AAAAGAKC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAFH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAFR" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABMAKT" varchar(12) NOT NULL default ' ',
  "AAAAGAFN" varchar(2) NOT NULL default ' ',
  "AAAAGYSC" varchar(2) NOT NULL default ' ',
  "AAAAGAFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAFR"(255)),
  KEY "AAAAGAFS" ("AAABLUXQ","AAAAGAKC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAFV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAHU" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABLDEO" varchar(10) NOT NULL default ' ',
  "AAAAGAJY" varchar(24) NOT NULL default '000000000000000000000000',
  "AAAAZASJ" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(20) NOT NULL default ' ',
  "AAABLDCS" varchar(30) NOT NULL default ' ',
  "AAABLOSZ" varchar(30) NOT NULL default ' ',
  "AAABIIZV" varchar(30) NOT NULL default ' ',
  "AAAAGAGH" varchar(30) NOT NULL default ' ',
  "AAAAGAGI" varchar(10) NOT NULL default ' ',
  "AAAAGAGJ" varchar(10) NOT NULL default ' ',
  "AAAAGAGK" varchar(1) NOT NULL default ' ',
  "AAAATIZN" varchar(1) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGAJZ" decimal(15,0) NOT NULL default '0',
  "AAAAGAGP" decimal(15,0) NOT NULL default '0',
  "AAAAGAGQ" decimal(15,0) NOT NULL default '0',
  "AAAAGAGR" varchar(15) NOT NULL default '000000000000000',
  "AAAAGAGS" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGT" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGU" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGV" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGW" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGX" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGY" varchar(10) NOT NULL default '0000000000',
  "AAAAGAGZ" varchar(10) NOT NULL default '0000000000',
  "AAAAGAHA" varchar(10) NOT NULL default '0000000000',
  "AAAAGAHB" varchar(10) NOT NULL default '0000000000',
  "AAAAGAHC" varchar(10) NOT NULL default ' ',
  "AAAAGAHD" varchar(10) NOT NULL default '0000000000',
  "AAAAGAHE" varchar(13) NOT NULL default ' ',
  "AAAAGAHF" varchar(2) NOT NULL default ' ',
  "AAAAGAHG" varchar(13) NOT NULL default ' ',
  "AAAAGAHH" varchar(2) NOT NULL default ' ',
  "AAAAGAHI" decimal(15,0) NOT NULL default '0',
  "AAAAGAHJ" decimal(15,0) NOT NULL default '0',
  "AAAAGAHK" decimal(15,0) NOT NULL default '0',
  "AAAAGAHL" decimal(15,0) NOT NULL default '0',
  "AAAAGAHM" int(11) NOT NULL default '0',
  "AAAAGAHN" int(11) NOT NULL default '0',
  "AAABDQKL" varchar(10) NOT NULL default '0000000000',
  "AAABDQKM" varchar(10) NOT NULL default '0000000000',
  "AAABDQKN" varchar(10) NOT NULL default '0000000000',
  "AAABDQKO" varchar(10) NOT NULL default '0000000000',
  "AAABDQKP" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAAAGAHU"(255)),
  KEY "AAAAGAHV" ("AAABLUXQ","AAAAGAKC"(255)),
  KEY "AAAAGAHY" ("AAAAGAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAIA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAIL" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABASZN" varchar(2) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAGAIH" varchar(12) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABINIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAIL"(255)),
  KEY "AAAAGAIM" ("AAABLUXQ","AAAAGAKC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAIP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAJL" VARBINARY(255) NOT NULL,
  "AAAAGAKC" VARBINARY(255),
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAAAGYSO" varchar(8) NOT NULL default '00000000',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAGYSU" smallint(6) NOT NULL default '0',
  "AAABMKFG" varchar(128) NOT NULL default ' ',
  "AAABLKIV" varchar(128) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAGAJA" VARBINARY(255),
  "AAAAMMPO" varchar(1) NOT NULL default ' ',
  "AAABLTQF" VARBINARY(255),
  "AAABEDIF" varchar(12) NOT NULL default ' ',
  "AAAAGYST" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(3) NOT NULL default ' ',
  "AAAAGAJH" varchar(12) NOT NULL default ' ',
  "AAABLLJG" smallint(6) NOT NULL default '0',
  "AAAAGAJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAJL"(255)),
  KEY "AAAAGAJM" ("AAABLUXQ","AAAAGAKC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAJP" (
  "AAAAGAKC" VARBINARY(255) NOT NULL,
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAAAGAKC"(255)),
  KEY "AAAAGAJT" ("AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAJV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGAKC" VARBINARY(255) NOT NULL,
  "AAAAGAJY" varchar(24) NOT NULL default '000000000000000000000000',
  "AAAAGAJZ" decimal(15,0) NOT NULL default '0',
  "AAABLDEO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGAKC"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGANN" (
  "AAAARRJI" varchar(12) NOT NULL default ' ',
  "AAAAGANN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLVFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAVZED" varchar(4) NOT NULL default ' ',
  "AAABJEYN" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAGAKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAVZED","AAABJEYN","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGALF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABBVLM" varchar(8) NOT NULL default ' ',
  "AAAAGALK" varchar(1) NOT NULL default ' ',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  "AAAAGALM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGALQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAVZED" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABJEYN" varchar(1) NOT NULL default ' ',
  "AAAAWLJC" varchar(50) NOT NULL default ' ',
  "AAAAWLJD" varchar(50) NOT NULL default ' ',
  "AAAAGANC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAVZED","AAABMPWP","AAABJEYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWCQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAAAWCQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAWCQG" varchar(3) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAAAWCQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAVZED" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAWLJC" varchar(50) NOT NULL default ' ',
  "AAAAWLJD" varchar(50) NOT NULL default ' ',
  "AAAAGANC" varchar(50) NOT NULL default ' ',
  "AAAAGAND" varchar(50) NOT NULL default ' ',
  "AAAAGANE" varchar(50) NOT NULL default ' ',
  "AAAAGANF" varchar(50) NOT NULL default ' ',
  "AAAAGANG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAVZED","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGANL" (
  "AAAARRJI" varchar(12) NOT NULL default ' ',
  "AAAAGANN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAARRJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGANP" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU","AAAATVUS","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAOF" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU","AAAATVUS","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAOV" (
  "AAAAGGKS" varchar(4) NOT NULL default ' ',
  "AAAAVIRJ" varchar(60) NOT NULL default ' ',
  "AAAAGGKO" varchar(1) NOT NULL default ' ',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAGGKK" varchar(1) NOT NULL default ' ',
  "AAABLHOZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGGKS","AAAAVIRJ","AAAAGGKO","AAABMJXI","AAAAGGKR","AAAAHWKU","AAAAGGKJ","AAAAGGKK"),
  KEY "AAAAGAPN" ("AAAAGGKR","AAAAHWKU","AAAAGGKO","AAABMJXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAPS" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAAAGAPW" varchar(2) NOT NULL default ' ',
  "AAAAGAPX" varchar(1) NOT NULL default ' ',
  "AAAAGAPY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAQA" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAAATVUE" varchar(1) NOT NULL default ' ',
  "AAAAGEWE" varchar(16) NOT NULL default ' ',
  "AAAAGEWF" varchar(8) NOT NULL default ' ',
  "AAAATVUG" varchar(12) NOT NULL default ' ',
  "AAAATVUH" varchar(4) NOT NULL default ' ',
  "AAAATVUI" varchar(8) NOT NULL default '00000000',
  "AAAATVUJ" varchar(6) NOT NULL default '000000',
  "AAAAVEFF" varchar(12) NOT NULL default ' ',
  "AAAATVUL" varchar(4) NOT NULL default ' ',
  "AAAAVEFG" varchar(8) NOT NULL default '00000000',
  "AAAATVUN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU","AAAATVUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMCNP" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAATJPI" varchar(40) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAAATJPI","AAABECXR"),
  KEY "AAAAGARF" ("AAAATJPI","AAABECXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGARI" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAAGGLC" varchar(5) NOT NULL default '00000',
  "AAAATVUE" varchar(1) NOT NULL default ' ',
  "AAAAWJTD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABMKFH","AAABMLQA","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGARW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGATY" varchar(8) NOT NULL default '00000000',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMBXO" varchar(2) NOT NULL default ' ',
  "AAAAGATE" smallint(6) NOT NULL default '0',
  "AAAAGASD" smallint(6) NOT NULL default '0',
  "AAAAGASE" smallint(6) NOT NULL default '0',
  "AAAAGASF" smallint(6) NOT NULL default '0',
  "AAAAGASG" smallint(6) NOT NULL default '0',
  "AAAAGASH" smallint(6) NOT NULL default '0',
  "AAAAGASI" smallint(6) NOT NULL default '0',
  "AAAAGASJ" smallint(6) NOT NULL default '0',
  "AAAAGASK" smallint(6) NOT NULL default '0',
  "AAAAGASL" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMLQA","AAAAGATY","AAABIRUN","AAABIHQV","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGASR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGATY" varchar(8) NOT NULL default '00000000',
  "AAABMBXO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAGATY","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGASY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGATY" varchar(8) NOT NULL default '00000000',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMBXO" varchar(2) NOT NULL default ' ',
  "AAAAGATE" smallint(6) NOT NULL default '0',
  "AAAAGATF" smallint(6) NOT NULL default '0',
  "AAAAGATG" smallint(6) NOT NULL default '0',
  "AAAAGATH" smallint(6) NOT NULL default '0',
  "AAAAGATI" smallint(6) NOT NULL default '0',
  "AAAAGATJ" smallint(6) NOT NULL default '0',
  "AAAAGATK" smallint(6) NOT NULL default '0',
  "AAAAGATL" smallint(6) NOT NULL default '0',
  "AAAAGATM" smallint(6) NOT NULL default '0',
  "AAAAGATN" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMLQA","AAAAGATY","AAABIRUN","AAABIHQV","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGATT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGATY" varchar(8) NOT NULL default '00000000',
  "AAABMBXO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAGATY","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAUA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAGAUK" varchar(4) NOT NULL default ' ',
  "AAABMBXO" varchar(2) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAAAJLSW" smallint(6) NOT NULL default '0',
  "AAAAJLSZ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMLQA","AAABIRUN","AAAAGAUK","AAABMBXO","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABLXQQ","AAABKRTQ"),
  KEY "AAAAGAVG" ("AAABMRIL","AAABLXQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMOG" varchar(8) NOT NULL default ' ',
  "AAAAGAVT" varchar(2) NOT NULL default '00',
  "AAAAGAVN" varchar(1) NOT NULL default ' ',
  "AAAAGAVO" varchar(4) NOT NULL default ' ',
  "AAAAGAVP" varchar(4) NOT NULL default ' ',
  "AAAAGAVQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMOG","AAAAGAVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMOG" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAVZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAWE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGJZC" varchar(15) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABGJZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDUTW" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDUTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAWR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABDUTW" varchar(15) NOT NULL default ' ',
  "AAABASZN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABDUTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAWZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVR" varchar(15) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABGKLD" varchar(8) NOT NULL default '00000000',
  "AAAAGAXH" decimal(11,2) NOT NULL default '0.00',
  "AAAAGAXI" decimal(11,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMKAH" decimal(9,2) NOT NULL default '0.00',
  "AAABAEWT" varchar(3) NOT NULL default ' ',
  "AAAAGAXM" varchar(10) NOT NULL default ' ',
  "AAAAUUZZ" varchar(6) NOT NULL default ' ',
  "AAAAGAXO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVR","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABMKKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAXV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAYA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWVR" varchar(15) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAGAYE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWVR","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAAATUZP" varchar(4) NOT NULL default '0000',
  "AAABEDNI" varchar(50) NOT NULL default ' ',
  "AAAAGAYR" varchar(1) NOT NULL default ' ',
  "AAAAGAYS" varchar(2) NOT NULL default ' ',
  "AAABECXS" varchar(30) NOT NULL default ' ',
  "AAAAHGJK" varchar(50) NOT NULL default ' ',
  "AAAAGAYV" varchar(1) NOT NULL default ' ',
  "AAAAGAYW" varchar(1) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  "AAAAGAYY" varchar(32) NOT NULL default ' ',
  "AAAAGAYZ" varchar(1) NOT NULL default ' ',
  "AAAAGBHI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABECYT","AAAATUZP","AAABEDNI"),
  KEY "AAAAGAZJ" ("AAABMRIL","AAABECYT","AAABEDNI","AAAATUZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGAZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAABEDNI" varchar(50) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABLEBM" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABECYT","AAABEDNI"),
  KEY "AAAAGBAH" ("AAABLEBM"),
  KEY "AAAAGBAJ" ("AAABMRIL","AAABECYT","AAABEDNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABEDNI" varchar(50) NOT NULL default ' ',
  "AAAATUZP" varchar(4) NOT NULL default '0000',
  "AAAAGBAV" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABLXUK" varchar(8) NOT NULL default '00000000',
  "AAABLXUL" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default ' ',
  "AAABMLFX" varchar(10) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAGBBJ" varchar(1) NOT NULL default ' ',
  "AAABHCYD" varchar(1) NOT NULL default ' ',
  "AAAAGBBL" varchar(8) NOT NULL default '00000000',
  "AAAAGBBM" varchar(12) NOT NULL default ' ',
  "AAAAGBBN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABEDNI"),
  KEY "AAAAGBBU" ("AAABMRIL","AAABEDNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGBQZ" varchar(1) NOT NULL default ' ',
  "AAABMLEL" varchar(4) NOT NULL default '0000',
  "AAAAGBCY" varchar(12) NOT NULL default '000000000000',
  "AAAAGBCZ" smallint(6) NOT NULL default '0',
  "AAAAGBDA" blob,
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAGBQZ","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGBQZ" varchar(1) NOT NULL default ' ',
  "AAAAGBJT" varchar(2) NOT NULL default '00',
  "AAABMLEL" varchar(4) NOT NULL default '0000',
  "AAAAGBCY" varchar(12) NOT NULL default '000000000000',
  "AAAAGBCZ" smallint(6) NOT NULL default '0',
  "AAAAGBDA" blob,
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAGBQZ","AAAAGBJT","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAABCVIU" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKQPP" varchar(8) NOT NULL default ' ',
  "AAABCCXV" varchar(12) NOT NULL default ' ',
  "AAAAGBDU" varchar(20) NOT NULL default ' ',
  "AAABEDAG" varchar(1) NOT NULL default ' ',
  "AAAAGBDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABEDBS","AAABCVIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGBEL" varchar(40) NOT NULL default ' ',
  "AAAAGBEM" varchar(1) NOT NULL default ' ',
  "AAAAGBEW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAABMLQA"),
  KEY "AAAAGBEU" ("AAABMLQA","AAAAGBEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBEX" (
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
CREATE TABLE "AAAAGBFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABKUHH" varchar(12) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAAAWUMF" varchar(3) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABKYIM" varchar(4) NOT NULL default ' ',
  "AAABLNZG" varchar(1) NOT NULL default ' ',
  "AAAAWANU" varchar(1) NOT NULL default ' ',
  "AAAAGBHQ" varchar(25) NOT NULL default ' ',
  "AAAAGBHR" varchar(2) NOT NULL default ' ',
  "AAAAGBHT" varchar(3) NOT NULL default ' ',
  "AAAAGBHS" varchar(3) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLEJR" varchar(255) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAAAGBGO" varchar(255) NOT NULL default ' ',
  "AAAAGBGP" varchar(10) NOT NULL default ' ',
  "AAAAGBGQ" varchar(3) NOT NULL default ' ',
  "AAAAGBGR" varchar(8) NOT NULL default '00000000',
  "AAAAGBGS" varchar(1) NOT NULL default '0',
  "AAABIVNA" varchar(14) NOT NULL default ' ',
  "AAABIVNB" varchar(14) NOT NULL default ' ',
  "AAABIVNC" varchar(14) NOT NULL default ' ',
  "AAABIVND" varchar(14) NOT NULL default ' ',
  "AAAAGBGX" varchar(255) NOT NULL default ' ',
  "AAAAGBJM" varchar(10) NOT NULL default ' ',
  "AAAAGBGZ" varchar(255) NOT NULL default ' ',
  "AAAAGBHA" varchar(10) NOT NULL default ' ',
  "AAABEEEK" varchar(8) NOT NULL default '00000000',
  "AAAAGBHC" varchar(25) NOT NULL default ' ',
  "AAAAGBHD" varchar(3) NOT NULL default ' ',
  "AAAAGBHE" varchar(2) NOT NULL default ' ',
  "AAABECTH" varchar(12) NOT NULL default '000000000000',
  "AAAAGBHG" varchar(12) NOT NULL default '000000000000',
  "AAABECTR" varchar(1) NOT NULL default ' ',
  "AAAAGBHI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF"),
  UNIQUE KEY "AAAAGBHO" ("AAABMRIL","AAAAGBHQ","AAAAGBHR","AAAAGBHS","AAAAGBHT","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF"),
  KEY "AAAAGBHY" ("AAABMRIL","AAABLUKI"),
  KEY "AAAAGBIB" ("AAABMRIL","AAABKUHH"),
  KEY "AAAAGBIE" ("AAABMRIL","AAABKRQD"),
  KEY "AAAAGBIH" ("AAABMRIL","AAAAWUMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABLEBM" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF"),
  KEY "AAAAGBIZ" ("AAABLEBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGBQZ" varchar(1) NOT NULL default ' ',
  "AAAAGBJT" varchar(2) NOT NULL default '00',
  "AAABLEJR" varchar(255) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAAAZLIN" varchar(16) NOT NULL default ' ',
  "AAAAGBJM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAGBQZ","AAAAGBJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAOVUM" varchar(10) NOT NULL default ' ',
  "AAAAGBLN" varchar(3) NOT NULL default '000',
  "AAAALGVJ" varchar(30) NOT NULL default ' ',
  "AAAAGBKL" varchar(3) NOT NULL default '000',
  "AAAALGVK" varchar(1) NOT NULL default ' ',
  "AAAAGBKB" varchar(8) NOT NULL default '00000000',
  "AAAAGBKC" double NOT NULL default '0',
  "AAAAGBKD" varchar(30) NOT NULL default ' ',
  "AAABLTXD" varchar(3) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLLIO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAOVUM","AAAAGBLN","AAAALGVJ","AAAAGBKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAOVUM" varchar(10) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABECKQ" varchar(3) NOT NULL default ' ',
  "AAAAGBLC" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAOVUM"),
  KEY "AAAAGBLA" ("AAABMRIL","AAAAGBLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAOVUM" varchar(10) NOT NULL default ' ',
  "AAAAGBLN" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAOVUM","AAAAGBLN"),
  KEY "AAAAGBLO" ("AAABMRIL","AAABMLCZ","AAABMCEY","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAOVUM" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAOVUM","AAABMCRM","AAABMCVP"),
  KEY "AAAAGBMC" ("AAABMRIL","AAABMCRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAALGVJ" varchar(30) NOT NULL default ' ',
  "AAAALGVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAALGVJ","AAAALGVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBOK" varchar(20) NOT NULL default ' ',
  "AAAARZHL" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAAAUNEG" varchar(2) NOT NULL default ' ',
  "AAABMCWW" varchar(70) NOT NULL default ' ',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABJZVA" varchar(4) NOT NULL default ' ',
  "AAAAGBQF" varchar(1) NOT NULL default ' ',
  "AAAAGBPP" varchar(4) NOT NULL default '0000',
  "AAABEAMZ" varchar(3) NOT NULL default ' ',
  "AAABEAPN" varchar(10) NOT NULL default ' ',
  "AAAAGBNE" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBOK","AAAARZHL"),
  KEY "AAAAGBNJ" ("AAABJTNV"),
  KEY "AAAAGBNL" ("AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBOK" varchar(20) NOT NULL default ' ',
  "AAAAGBSG" decimal(15,0) NOT NULL default '0',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAAAGBPZ" decimal(15,0) NOT NULL default '0',
  "AAABGBSN" varchar(12) NOT NULL default ' ',
  "AAAAGBNU" varchar(20) NOT NULL default ' ',
  "AAAAGBNV" varchar(1) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAGBNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGBOK" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAGBOK"),
  KEY "AAAAGBOI" ("AAABMRIL","AAAAGBOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBQT" varchar(20) NOT NULL default '00000000000000000000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAVKAP" varchar(20) NOT NULL default ' ',
  "AAABEALW" varchar(1) NOT NULL default ' ',
  "AAABEANG" varchar(1) NOT NULL default ' ',
  "AAABEANH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(7) NOT NULL default ' ',
  "AAABEANN" varchar(2) NOT NULL default ' ',
  "AAABEANO" varchar(12) NOT NULL default ' ',
  "AAAAGBSG" decimal(15,0) NOT NULL default '0',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAAAGBRT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAGBQT"),
  KEY "AAAAGBPC" ("AAABEAOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBRD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAAAUNEG" varchar(2) NOT NULL default ' ',
  "AAABMCWW" varchar(70) NOT NULL default ' ',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABJZVA" varchar(4) NOT NULL default ' ',
  "AAAAGBPP" varchar(4) NOT NULL default '0000',
  "AAABEAMZ" varchar(3) NOT NULL default ' ',
  "AAABEAPN" varchar(10) NOT NULL default ' ',
  "AAABGEXP" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAGBRZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGBQT" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGBSM" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGBSG" decimal(15,0) NOT NULL default '0',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAAAGBPZ" decimal(15,0) NOT NULL default '0',
  "AAABGBSN" varchar(12) NOT NULL default ' ',
  "AAAAGBQB" decimal(15,0) NOT NULL default '0',
  "AAAAGBQC" varchar(12) NOT NULL default ' ',
  "AAAAGBQD" decimal(15,0) NOT NULL default '0',
  "AAAAGBQE" varchar(12) NOT NULL default ' ',
  "AAAAGBQF" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(3) NOT NULL default ' ',
  "AAAAGDPC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAGBRD"),
  KEY "AAAAGBQK" ("AAABJTNV"),
  KEY "AAAAGBQM" ("AAABMCWW"),
  KEY "AAAAGBQO" ("AAAAGBSM"),
  KEY "AAAAGBQQ" ("AAABMRFO"),
  KEY "AAAAGBQS" ("AAAAGBQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBRD" varchar(20) NOT NULL default '00000000000000000000',
  "AAAARZHL" varchar(3) NOT NULL default '000',
  "AAABLEJR" varchar(255) NOT NULL default ' ',
  "AAAAGBQZ" varchar(5) NOT NULL default ' ',
  "AAAAGBRA" varchar(32) NOT NULL default ' ',
  "AAAAGBRB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBRD","AAAARZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBRZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAAAVKAP" varchar(20) NOT NULL default ' ',
  "AAABEALZ" varchar(1) NOT NULL default ' ',
  "AAABEALW" varchar(1) NOT NULL default ' ',
  "AAABEANG" varchar(1) NOT NULL default ' ',
  "AAABEANH" varchar(1) NOT NULL default ' ',
  "AAAAGBSG" decimal(15,0) NOT NULL default '0',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAABEANN" varchar(2) NOT NULL default ' ',
  "AAABEANO" varchar(12) NOT NULL default ' ',
  "AAAAGBRT" varchar(8) NOT NULL default '00000000',
  "AAAATYJC" varchar(70) NOT NULL default ' ',
  "AAAATYJD" varchar(70) NOT NULL default ' ',
  "AAAATYJE" varchar(70) NOT NULL default ' ',
  "AAAATYJF" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGBSM" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGBSD" decimal(15,0) NOT NULL default '0',
  "AAABEANN" varchar(2) NOT NULL default ' ',
  "AAABEANO" varchar(12) NOT NULL default ' ',
  "AAAAGBSG" decimal(15,0) NOT NULL default '0',
  "AAABEAND" varchar(3) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAUNEG" varchar(2) NOT NULL default ' ',
  "AAABMCWW" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGBSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABGJDS" varchar(8) NOT NULL default '00000000',
  "AAABBHTG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBST" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABGJDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLBXJ" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAAAVZXS" varchar(1) NOT NULL default ' ',
  "AAABDJRZ" varchar(6) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAGBXG" varchar(1) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAANJPU" varchar(1) NOT NULL default ' ',
  "AAABMCNB" varchar(10) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMCRY" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCPL" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAVZEC" varchar(3) NOT NULL default '000',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUH" decimal(13,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(13,2) NOT NULL default '0.00',
  "AAABEMIB" varchar(4) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUL" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUM" decimal(13,2) NOT NULL default '0.00',
  "AAAAHHAU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTF" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQH" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMCPN" varchar(8) NOT NULL default '00000000',
  "AAAAGBUV" varchar(8) NOT NULL default '00000000',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABMCTD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGBUZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQZ" varchar(2) NOT NULL default ' ',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMCQJ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCUF" varchar(6) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABLVGA" varchar(2) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCRL" varchar(16) NOT NULL default ' ',
  "AAABKNXB" varchar(16) NOT NULL default ' ',
  "AAAAGBVS" varchar(1) NOT NULL default ' ',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABJKLH" varchar(3) NOT NULL default ' ',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABMDBB","AAABMCPV","AAABLBXJ","AAABMCPS","AAAAVZXS","AAABDJRZ","AAABMQBT","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGBWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLBXJ" varchar(1) NOT NULL default ' ',
  "AAAAVZCL" varchar(4) NOT NULL default ' ',
  "AAAAGBWV" varchar(50) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABMCST" varchar(1) NOT NULL default ' ',
  "AAAAVZEH" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAVZEC" varchar(3) NOT NULL default '000',
  "AAAAGBXC" varchar(30) NOT NULL default ' ',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAAAGBXE" varchar(1) NOT NULL default ' ',
  "AAAAWUME" varchar(1) NOT NULL default ' ',
  "AAAAGBXG" varchar(1) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGBXN" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQS" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQT" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQU" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQW" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQX" decimal(15,2) NOT NULL default '0.00',
  "AAABIBQZ" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRA" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRC" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRD" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRF" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRG" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRI" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRJ" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRL" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRM" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRO" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRP" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRR" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRS" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRU" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRV" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRX" decimal(15,2) NOT NULL default '0.00',
  "AAABIBRY" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSA" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSB" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSD" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSE" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSG" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSH" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSJ" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSK" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSM" decimal(15,2) NOT NULL default '0.00',
  "AAABIBSN" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBYV" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBYW" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBYX" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBYY" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBYZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZA" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZB" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZC" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZD" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZE" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZF" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZG" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZH" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZI" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZK" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZL" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZM" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZN" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZO" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZP" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZR" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZS" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZT" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZU" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZV" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZW" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZX" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZY" decimal(15,2) NOT NULL default '0.00',
  "AAAAGBZZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAGCAA" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPWP","AAABMREX","AAABLZBN","AAABMDBB","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGCCC" varchar(10) NOT NULL default '0000000000',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAGCBA" varchar(20) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABJZVA" varchar(20) NOT NULL default ' ',
  "AAABECND" varchar(10) NOT NULL default ' ',
  "AAAAGCAQ" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABECNZ" varchar(4) NOT NULL default ' ',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAGCCD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAGCCC","AAABLSCQ","AAABMQDU","AAAAGCBA","AAABKNGX"),
  KEY "AAAAGCBC" ("AAABJZVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGCCC" varchar(10) NOT NULL default '0000000000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABJTNV" varchar(77) NOT NULL default ' ',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABIJDG" varchar(4) NOT NULL default '0000',
  "AAABIJDI" varchar(6) NOT NULL default '000000',
  "AAABIJDR" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABFWAW" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGCCC","AAABLMHB","AAABJTNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGCCC" varchar(10) NOT NULL default '0000000000',
  "AAAAGCCD" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABECNZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGCCC","AAAAGCCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCCE" (
  "AAAARFYV" varchar(1) NOT NULL default ' ',
  "AAABKSUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCCI" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMOHM","AAABLHMS","AAABLLBG","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCCU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOHM","AAABLHMS","AAABLLBG","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCDG" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(10) NOT NULL default ' ',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABLUFA" varchar(3) NOT NULL default '000',
  "AAAAVNMX" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLKHW","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCDS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCEC" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUSR" varchar(70) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLUSR","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCEL" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  "AAAAGCES" varchar(1) NOT NULL default ' ',
  "AAAAQXDP" varchar(5) NOT NULL default '00000',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAAAGCEV" varchar(20) NOT NULL default ' ',
  "AAAAGCEW" varchar(5) NOT NULL default '00000',
  "AAAAGCEX" varchar(10) NOT NULL default ' ',
  "AAAAGCEY" varchar(10) NOT NULL default ' ',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  "AAAAGCFA" varchar(20) NOT NULL default ' ',
  "AAAAGCFB" varchar(12) NOT NULL default ' ',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABGIFA" varchar(1) NOT NULL default ' ',
  "AAAAGCFG" varchar(1) NOT NULL default ' ',
  "AAAAGCFH" varchar(1) NOT NULL default ' ',
  "AAAAGCFI" varchar(1) NOT NULL default ' ',
  "AAAAGCFJ" varchar(4) NOT NULL default '0000',
  "AAAAGCFK" varchar(1) NOT NULL default ' ',
  "AAAAGCFL" varchar(1) NOT NULL default ' ',
  "AAAAGCFM" varchar(1) NOT NULL default ' ',
  "AAAAGCFN" varchar(20) NOT NULL default ' ',
  "AAAAGCFO" varchar(1) NOT NULL default ' ',
  "AAAAGDSI" varchar(1) NOT NULL default ' ',
  "AAAAGDSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCFU" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAAAGCFZ" varchar(20) NOT NULL default ' ',
  "AAAAGCGA" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCGF" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABHEWT" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAAGCGL" varchar(1) NOT NULL default ' ',
  "AAAAGCYU" varchar(12) NOT NULL default ' ',
  "AAAAJORJ" varchar(8) NOT NULL default '00000000',
  "AAAAJORK" varchar(6) NOT NULL default '000000',
  "AAAAGCGP" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLHMS","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCGS" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABGKVZ" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAAAGDWW" varchar(2) NOT NULL default '00',
  "AAAAGDWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAAATWIS","AAAAGEFJ","AAABHKDA"),
  KEY "AAAAGCHH" ("AAABHKDA","AAAAGEFJ","AAAATWIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCHL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAAATWIS","AAAAGEFJ","AAABHKDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCHZ" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABIFRG" varchar(60) NOT NULL default ' ',
  "AAAAGCIM" varchar(60) NOT NULL default ' ',
  "AAABLEPD" varchar(120) NOT NULL default ' ',
  "AAABLEPE" varchar(120) NOT NULL default ' ',
  "AAABLEPF" varchar(120) NOT NULL default ' ',
  "AAABLEPG" varchar(120) NOT NULL default ' ',
  "AAABLEPH" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABIFRG","AAAAGCIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCIN" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABGMFA" varchar(3) NOT NULL default '000',
  "AAAAGCIR" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABGMFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCIW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABGMFA" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABGMFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCJG" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABHQRQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCJN" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABHQRQ" varchar(20) NOT NULL default ' ',
  "AAAAGCLV" varchar(1) NOT NULL default '0',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAAAGCJT" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABHQRQ","AAAAGCLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCJZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABHQRQ" varchar(20) NOT NULL default ' ',
  "AAAAGCLV" varchar(1) NOT NULL default '0',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABHQRQ","AAAAGCLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCKL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABHQRQ" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABHQRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCKV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCLD" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAAAGCLJ" varchar(10) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAAAGCLL" varchar(2) NOT NULL default '00',
  "AAABJXTS" varchar(2) NOT NULL default '00',
  "AAABCEBQ" varchar(1) NOT NULL default ' ',
  "AAABIYUV" varchar(1) NOT NULL default ' ',
  "AAABDPTW" varchar(1) NOT NULL default ' ',
  "AAAAGCLQ" varchar(1) NOT NULL default ' ',
  "AAAAGCLR" varchar(1) NOT NULL default ' ',
  "AAAAGCLS" varchar(1) NOT NULL default ' ',
  "AAAAGCLT" varchar(1) NOT NULL default ' ',
  "AAAAGCLU" varchar(1) NOT NULL default ' ',
  "AAAAGCLV" varchar(1) NOT NULL default '0',
  "AAAAGCLW" varchar(128) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCMC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAAAGCMJ" varchar(60) NOT NULL default ' ',
  "AAAAGCMK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCMQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABMECK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCNB" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(10) NOT NULL default ' ',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABIXKR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLKHW","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCNL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCNV" (
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCNY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCOE" (
  "AAAAGCOQ" varchar(5) NOT NULL default '00000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGCOH" varchar(132) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGCOQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCOM" (
  "AAAAGEGU" varchar(8) NOT NULL default ' ',
  "AAAAGDUJ" varchar(5) NOT NULL default '00000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGCOQ" varchar(5) NOT NULL default '00000',
  "AAAAGCPV" varchar(40) NOT NULL default ' ',
  "AAAAGCOS" varchar(255) NOT NULL default ' ',
  "AAAAGCOT" varchar(255) NOT NULL default ' ',
  "AAABAIWR" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAAAGCOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEGU","AAAAGDUJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCPB" (
  "AAABAIWR" varchar(5) NOT NULL default '00000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGCPE" varchar(132) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABAIWR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCPJ" (
  "AAABHEGL" varchar(5) NOT NULL default '00000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGYIP" varchar(132) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCPR" (
  "AAAAGCPV" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAGCPU" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCPW" (
  "AAABHTTC" varchar(10) NOT NULL default ' ',
  "AAAAGCSZ" varchar(10) NOT NULL default ' ',
  "AAABIMAG" varchar(1) NOT NULL default ' ',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATOVN" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABDEYE" int(11) NOT NULL default '0',
  "AAAAGCQE" int(11) NOT NULL default '0',
  "AAAAYGNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTTC","AAAAGCSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCQI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGDEW" varchar(13) NOT NULL default ' ',
  "AAABIYQF" varchar(1) NOT NULL default ' ',
  "AAABMBXO" varchar(255) NOT NULL default ' ',
  "AAAAGDCV" int(11) NOT NULL default '0',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLPSU" varchar(128) NOT NULL default ' ',
  "AAABHRXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNI","AAAAGDEW"),
  KEY "AAAAGCRA" ("AAABLUXP","AAAAGEIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCRD" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGDEW" varchar(13) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGDEW","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCRM" (
  "AAABLRIF" varchar(10) NOT NULL default '0000000000',
  "AAAAGCRU" varchar(3) NOT NULL default '000',
  "AAABLNDO" varchar(2) NOT NULL default ' ',
  "AAAAGCRW" varchar(1) NOT NULL default ' ',
  "AAAAGCRR" varchar(60) NOT NULL default ' ',
  "AAAAGCSC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLRIF","AAAAGCRU"),
  KEY "AAAAGCRV" ("AAAAGCRW","AAABLNDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCRY" (
  "AAABLNDO" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAAAGCSB" varchar(30) NOT NULL default ' ',
  "AAAAGCSC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLNDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCSE" (
  "AAAAGEHA" varchar(10) NOT NULL default ' ',
  "AAAAGCSR" varchar(10) NOT NULL default ' ',
  "AAAAUKME" varchar(10) NOT NULL default '0000000000',
  "AAAAGEHC" varchar(3) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABHTTC" varchar(10) NOT NULL default ' ',
  "AAAAGCSX" varchar(60) NOT NULL default ' ',
  "AAAAGCSY" varchar(60) NOT NULL default ' ',
  "AAAAGCSZ" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEHA","AAAAGCSR","AAAAUKME","AAAAGEHC","AAABLFEA","AAAAGEIF","AAABHTTC","AAAAGCSX","AAAAGCSY","AAAAGCSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCTA" (
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAAAUYDF" varchar(20) NOT NULL default ' ',
  "AAAAGCTD" varchar(1) NOT NULL default ' ',
  "AAAAGCTE" varchar(1) NOT NULL default ' ',
  "AAAAGCTF" int(11) NOT NULL default '0',
  "AAAAGCTG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMIRY","AAAAUYDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCTJ" (
  "AAAAGCTU" varchar(3) NOT NULL default ' ',
  "AAAAVFDV" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVFDW" tinyint(3) unsigned NOT NULL default '0',
  "AAAAVFDX" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAAAGCTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCTP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGCTU" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAGCTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCTV" (
  "AAABIFRG" varchar(3) NOT NULL default ' ',
  "AAAAGCTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIFRG","AAAAGCTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCUA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGCUG" varchar(1) NOT NULL default ' ',
  "AAAAGCUH" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAGCUG","AAAAGCUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCUI" (
  "AAAAGCUR" varchar(120) NOT NULL default ' ',
  "AAAAGCUP" varchar(250) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGCUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCUN" (
  "AAAAGCUR" varchar(120) NOT NULL default ' ',
  "AAAAGCUP" varchar(250) NOT NULL default ' ',
  "AAABLPSU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGCUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCUS" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAAGCVX" varchar(3) NOT NULL default '000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAAAKEYV" varchar(40) NOT NULL default ' ',
  "AAAAKEYW" varchar(60) NOT NULL default ' ',
  "AAAAKEYR" varchar(10) NOT NULL default ' ',
  "AAAAKEYS" varchar(32) NOT NULL default ' ',
  "AAAAKEYT" varchar(10) NOT NULL default ' ',
  "AAAAKEYU" varchar(32) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAAAGCVX","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCVL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAAGCVX" varchar(3) NOT NULL default '000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAAAGCVX","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCVZ" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(20) NOT NULL default ' ',
  "AAABIJSB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCWH" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(20) NOT NULL default ' ',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLKHW","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCWU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLKHW" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCXE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABGYIL" varchar(30) NOT NULL default ' ',
  "AAABFODZ" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGEIF","AAABLUXP","AAABGYIL","AAABFODZ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCXU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABGYIL" varchar(30) NOT NULL default ' ',
  "AAABFODZ" varchar(10) NOT NULL default '0000000000',
  "AAABIZHF" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGEIF","AAABLUXP","AAABGYIL","AAABFODZ","AAABIZHF","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCYM" (
  "AAAAGDFL" varchar(20) NOT NULL default ' ',
  "AAAAGDFF" varchar(5) NOT NULL default '00000',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAAAGCYR" varchar(1) NOT NULL default ' ',
  "AAAAGCYS" varchar(60) NOT NULL default ' ',
  "AAAAGCYT" varchar(1) NOT NULL default ' ',
  "AAAAGCYU" varchar(12) NOT NULL default ' ',
  "AAAAJORJ" varchar(8) NOT NULL default '00000000',
  "AAAAJORK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAGDFL","AAAAGDFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCYZ" (
  "AAAAGDFL" varchar(20) NOT NULL default ' ',
  "AAAAGDFF" varchar(5) NOT NULL default '00000',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAAAGCZD" varchar(10) NOT NULL default '0000000000',
  "AAAAGCZE" varchar(10) NOT NULL default '0000000000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAAAGCZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDFL","AAAAGDFF","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCZO" (
  "AAAAGDFL" varchar(20) NOT NULL default ' ',
  "AAAAGDFF" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDFL","AAAAGDFF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGCZW" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAABJSHI" varchar(1) NOT NULL default ' ',
  "AAAAGDAC" varchar(12) NOT NULL default ' ',
  "AAAAGDAD" varchar(8) NOT NULL default '00000000',
  "AAAAGDAE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDAJ" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABLUVE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDAV" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAAAGDBH" varchar(10) NOT NULL default '0000000000',
  "AAAAGDEW" varchar(5) NOT NULL default '00000',
  "AAAAVRCD" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABLUVE","AAAAGDBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDBI" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAAAGDBN" varchar(1) NOT NULL default ' ',
  "AAAAGDBO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDBQ" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDBW" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDEW" varchar(5) NOT NULL default '00000',
  "AAAAGDEX" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGDCV" int(11) NOT NULL default '0',
  "AAAAGDDM" varchar(20) NOT NULL default ' ',
  "AAAAGDDN" varchar(20) NOT NULL default ' ',
  "AAAAGDDO" varchar(20) NOT NULL default ' ',
  "AAAAGDDP" varchar(20) NOT NULL default ' ',
  "AAAAGDDQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAAAGDEW","AAAAGDEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDCO" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDEW" varchar(5) NOT NULL default '00000',
  "AAAAGDEX" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGDCV" int(11) NOT NULL default '0',
  "AAAAGDDM" varchar(20) NOT NULL default ' ',
  "AAAAGDDN" varchar(20) NOT NULL default ' ',
  "AAAAGDDO" varchar(20) NOT NULL default ' ',
  "AAAAGDDP" varchar(20) NOT NULL default ' ',
  "AAAAGDDQ" varchar(20) NOT NULL default ' ',
  "AAABJAAI" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAAAGDEW","AAAAGDEX"),
  KEY "AAAAGDDH" ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABMRHS","AAAAGDDM","AAAAGDDN","AAAAGDDO","AAAAGDDP","AAAAGDDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDDR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDEW" varchar(5) NOT NULL default '00000',
  "AAAAGDEX" varchar(5) NOT NULL default '00000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAAAGDEW","AAAAGDEX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDEI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDEW" varchar(5) NOT NULL default '00000',
  "AAAAGDEX" varchar(5) NOT NULL default '00000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAAAGDEW","AAAAGDEX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDEZ" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAAAGDFL" varchar(20) NOT NULL default ' ',
  "AAAAGDFF" varchar(5) NOT NULL default '00000',
  "AAAAGDFG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABLUVE","AAAAGDFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDFM" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABDTDP" varchar(4) NOT NULL default '0000',
  "AAABMNRG" tinyint(3) unsigned NOT NULL default '0',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAAAGDFY" varchar(60) NOT NULL default ' ',
  "AAAAGDFZ" varchar(1) NOT NULL default ' ',
  "AAAAGDGA" varchar(255) NOT NULL default ' ',
  "AAAAIIYV" varchar(128) NOT NULL default ' ',
  "AAABKIKF" varchar(60) NOT NULL default ' ',
  "AAAAGDGD" smallint(6) NOT NULL default '0',
  "AAABLPSU" text,
  PRIMARY KEY ("AAAAGEIF","AAABLUXP","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDGI" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAAAGDGP" varchar(60) NOT NULL default ' ',
  "AAABLJJN" varchar(60) NOT NULL default ' ',
  "AAABGBTL" varchar(60) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAABJSJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP","AAAATWIS","AAAATWIV","AAAAGEFJ","AAABHKDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDHA" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAAAGDHI" varchar(1) NOT NULL default ' ',
  "AAAAGDHJ" varchar(1) NOT NULL default ' ',
  "AAAAGDHK" varchar(1) NOT NULL default ' ',
  "AAABIGGP" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP","AAABLHMS","AAABLLBG","AAABLUSV","AAAATWIS","AAAATWIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDHU" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABDTDP" varchar(5) NOT NULL default '00000',
  "AAABGMFA" varchar(3) NOT NULL default '000',
  "AAABLQCG" varchar(60) NOT NULL default ' ',
  "AAABLQCH" varchar(60) NOT NULL default ' ',
  "AAABLQCI" varchar(60) NOT NULL default ' ',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGDIJ" varchar(20) NOT NULL default ' ',
  "AAAAGDIK" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDIO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAABLUSR" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAAGEIF","AAABLUXP","AAABLHMS","AAABLLBG","AAABLUSV","AAAATWIS","AAAATWIV","AAABLUSR","AAABLUMP"),
  KEY "AAAAGDJM" ("AAABLUXP","AAAAGEIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDJP" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAABLUSR" varchar(60) NOT NULL default ' ',
  "AAAAGDJZ" longblob,
  PRIMARY KEY ("AAABLUNI","AAAAGEIF","AAABLUXP","AAABLHMS","AAABLLBG","AAABLUSV","AAAATWIS","AAAATWIV","AAABLUSR"),
  KEY "AAAAGDKJ" ("AAABLUXP","AAAAGEIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDKM" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAATWIV" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAAAGDYL" varchar(3) NOT NULL default '000',
  "AAAAGDKX" varchar(10) NOT NULL default ' ',
  "AAAAGDKY" varchar(40) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABDBGB" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP","AAABLHMS","AAABLLBG","AAABLUSV","AAAATWIS","AAAATWIV","AAABMOHM","AAABDTDP","AAAAGDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDLN" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAAAGDYL" varchar(3) NOT NULL default '000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABDBGB" varchar(20) NOT NULL default ' ',
  "AAAAGDLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABMOHM","AAABDTDP","AAAAGDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDMF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAAAGDYL" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABMOHM","AAABDTDP","AAAAGDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDMV" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAAAGDMY" varchar(12) NOT NULL default ' ',
  "AAAAGDPC" varchar(8) NOT NULL default '00000000',
  "AAAAGDNA" varchar(12) NOT NULL default ' ',
  "AAAATQII" varchar(8) NOT NULL default '00000000',
  "AAAAGDNC" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDNF" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAAAGEGU" varchar(8) NOT NULL default ' ',
  "AAAAGDUJ" varchar(5) NOT NULL default '00000',
  "AAAAGEGW" varchar(12) NOT NULL default ' ',
  "AAAAGDPC" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAAAGEHA" varchar(10) NOT NULL default ' ',
  "AAAAUKME" varchar(10) NOT NULL default '0000000000',
  "AAAAGEHC" varchar(3) NOT NULL default ' ',
  "AAABLPSU" varchar(128) NOT NULL default ' ',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGEHK" varchar(20) NOT NULL default ' ',
  "AAABDYST" varchar(20) NOT NULL default ' ',
  "AAAAGDPL" varchar(1) NOT NULL default ' ',
  "AAAAGDOY" varchar(3) NOT NULL default ' ',
  "AAAAGDOX" varchar(10) NOT NULL default ' ',
  "AAAAKASX" varchar(12) NOT NULL default ' ',
  "AAAAGEHQ" varchar(8) NOT NULL default '00000000',
  "AAAAKASY" varchar(6) NOT NULL default '000000',
  "AAAAGDOZ" varchar(2) NOT NULL default ' ',
  "AAAAGEHT" varchar(13) NOT NULL default ' ',
  "AAAAGEHU" varchar(32) NOT NULL default ' ',
  "AAAAGDOJ" varchar(3) NOT NULL default ' ',
  "AAAAGDOK" varchar(10) NOT NULL default ' ',
  "AAAAGEHX" varchar(12) NOT NULL default ' ',
  "AAAATMPV" varchar(8) NOT NULL default '00000000',
  "AAAATMPW" varchar(6) NOT NULL default '000000',
  "AAAAGDSF" varchar(8) NOT NULL default ' ',
  "AAAAGEIB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP"),
  KEY "AAAAGDOS" ("AAAAGEGU","AAABMRFO","AAAAGDUJ"),
  KEY "AAAAGDOW" ("AAAAGDOX","AAAAGDOY","AAAAGDOZ","AAAAGEHT"),
  KEY "AAAAGDPB" ("AAAAGDPC","AAABMRFO"),
  KEY "AAAAGDPE" ("AAAAGDPL","AAAAGEHK","AAABDYST"),
  KEY "AAAAGDPI" ("AAABDYST","AAAAGEHK","AAAAGDPL"),
  KEY "AAAAGDPM" ("AAABMRFO","AAAAGEHA","AAAAUKME","AAAAGEHC"),
  KEY "AAAAGDPR" ("AAABLUXP"),
  KEY "AAAAGDPT" ("AAABMRFO","AAAAGEGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDPW" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABMDEN"),
  KEY "AAAAGDQH" ("AAABMDEN","AAABKWFM","AAABMKKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDQL" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAAAGDQQ" varchar(5) NOT NULL default '00000',
  "AAAAGDQR" varchar(20) NOT NULL default ' ',
  "AAAAGDQS" varchar(1) NOT NULL default ' ',
  "AAABJSHI" varchar(1) NOT NULL default ' ',
  "AAAAGDQU" varchar(8) NOT NULL default '00000000',
  "AAAAGDQV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDRA" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDRR" varchar(20) NOT NULL default ' ',
  "AAABBZTO" varchar(40) NOT NULL default ' ',
  "AAAAGDRG" varchar(1) NOT NULL default ' ',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAAAGDRJ" varchar(2) NOT NULL default '00',
  "AAAAGDRK" varchar(30) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAAAGDRN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ","AAAAGDRR","AAABBZTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDRT" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAAAGDRX" varchar(10) NOT NULL default ' ',
  "AAAAGDRY" varchar(10) NOT NULL default ' ',
  "AAAAGDRZ" varchar(30) NOT NULL default ' ',
  "AAAAGDSA" varchar(3) NOT NULL default '000',
  "AAAAGDSB" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAGDSD" varchar(20) NOT NULL default ' ',
  "AAAAGDSE" varchar(1) NOT NULL default ' ',
  "AAAAGDSF" varchar(8) NOT NULL default ' ',
  "AAAAGEIB" varchar(8) NOT NULL default ' ',
  "AAAAGDSH" varchar(1) NOT NULL default ' ',
  "AAAAGDSI" varchar(1) NOT NULL default ' ',
  "AAAAGDSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAAAGDTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDSN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLFZO" varchar(15) NOT NULL default ' ',
  "AAAAGDSU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAGDUR","AAAAGDUS","AAAAGDTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDSZ" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAAAKZCT" varchar(6) NOT NULL default '000000',
  "AAAAGDTE" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAAAGDTI" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP"),
  KEY "AAAAGDTO" ("AAABLUXP","AAABMRFO","AAAAGEIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDTS" (
  "AAAAGEGU" varchar(8) NOT NULL default ' ',
  "AAAAGDUJ" varchar(5) NOT NULL default '00000',
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAAAGDTZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAGEGU","AAAAGDUJ","AAAAGEIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDUD" (
  "AAAAGEGU" varchar(8) NOT NULL default ' ',
  "AAAAGDUJ" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGEGU","AAAAGDUJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDUL" (
  "AAAAGDUR" varchar(20) NOT NULL default ' ',
  "AAAAGDUS" varchar(20) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAGDUP" varchar(1) NOT NULL default ' ',
  "AAAAGDUQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAGDUR","AAAAGDUS","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDUU" (
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABILXJ" varchar(64) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN","AAABMDEN","AAABILXJ"),
  KEY "AAAAGDVC" ("AAABMDEN","AAABILXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDVF" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABHCED" varchar(2) NOT NULL default '00',
  "AAABLUFA" varchar(10) NOT NULL default ' ',
  "AAABGLXQ" varchar(2) NOT NULL default '00',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAAAUNGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDVT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABJPEE" varchar(2) NOT NULL default '00',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAABJPEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDWH" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABJPEE" varchar(2) NOT NULL default '00',
  "AAAAGDWN" varchar(1) NOT NULL default ' ',
  "AAAAGDWO" varchar(1) NOT NULL default ' ',
  "AAAAGDWP" varchar(1) NOT NULL default ' ',
  "AAAAGDWQ" varchar(1) NOT NULL default ' ',
  "AAAAGDWR" varchar(2) NOT NULL default '00',
  "AAAAGDWS" varchar(1) NOT NULL default ' ',
  "AAAAGDWT" varchar(1) NOT NULL default ' ',
  "AAAAGDWU" varchar(4) NOT NULL default '0000',
  "AAAAGDWV" varchar(1) NOT NULL default ' ',
  "AAAAGDWW" varchar(2) NOT NULL default '00',
  "AAAAGDWX" varchar(1) NOT NULL default ' ',
  "AAABLECP" varchar(1) NOT NULL default ' ',
  "AAAAGDWZ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAABJPEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDXF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAAAGDZM" varchar(3) NOT NULL default '000',
  "AAAAGDYL" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAAAGDZM","AAAAGDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDXV" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAAAGDZM" varchar(3) NOT NULL default '000',
  "AAAAGDYL" varchar(3) NOT NULL default '000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABDBGB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAAAGDZM","AAAAGDYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDYM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDYY" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAAAGDZM" varchar(3) NOT NULL default '000',
  "AAABJPEE" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  "AAAAGDZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAAAGDZM","AAABJPEE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGDZP" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABLUSR" varchar(70) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGECO" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABDUFB","AAABLUSR","AAABHHRZ","AAABMRHS","AAAAGECO","AAABMRFO","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEAI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABLUSR" varchar(70) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGECO" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABDUFB","AAABLUSR","AAABHHRZ","AAABMRHS","AAAAGECO","AAABMRFO","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEBB" (
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABLUSR" varchar(70) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGECO" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  "AAAAGECE" varchar(3) NOT NULL default ' ',
  "AAAAGECF" varchar(1) NOT NULL default ' ',
  "AAAATITJ" varchar(12) NOT NULL default ' ',
  "AAABAHPB" varchar(12) NOT NULL default ' ',
  "AAABLZKI" varchar(8) NOT NULL default '00000000',
  "AAABLRNV" varchar(6) NOT NULL default '000000',
  "AAAAGEBP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDUFB","AAABLUSR","AAABHHRZ","AAABMRHS","AAAAGECO","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEBW" (
  "AAABDUFB" varchar(1) NOT NULL default ' ',
  "AAABLUSR" varchar(70) NOT NULL default ' ',
  "AAABHHRZ" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGECO" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  "AAAAGECE" varchar(3) NOT NULL default ' ',
  "AAAAGECF" varchar(1) NOT NULL default ' ',
  "AAAATITJ" varchar(12) NOT NULL default ' ',
  "AAABAHPB" varchar(12) NOT NULL default ' ',
  "AAABLZKI" varchar(8) NOT NULL default '00000000',
  "AAABLRNV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABDUFB","AAABLUSR","AAABHHRZ","AAABMRHS","AAAAGECO","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGECQ" (
  "AAAAGECV" varchar(12) NOT NULL default ' ',
  "AAABGZSJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGECV","AAABGZSJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGECY" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEDF" varchar(5) NOT NULL default '00000',
  "AAAAGEDG" varchar(5) NOT NULL default '00000',
  "AAABLTXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEDN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAABHMRI" varchar(5) NOT NULL default '00000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEEB" (
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABMIRY" varchar(30) NOT NULL default ' ',
  "AAABHBMG" varchar(120) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAAAGEEG" varchar(20) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABKMYU","AAABMIRY","AAABHBMG","AAABILWI"),
  KEY "AAAAGEEN" ("AAABMIRY","AAABHBMG","AAABJLOQ","AAABIJJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEES" (
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDTDP" varchar(2) NOT NULL default '00',
  "AAAAGEEW" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREL","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEFA" (
  "AAABHWXD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHWXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEFD" (
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAAATWIS" varchar(10) NOT NULL default ' ',
  "AAAAGEFJ" varchar(20) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAAAGEFL" varchar(60) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(4) NOT NULL default '0000',
  "AAAAGEFO" varchar(60) NOT NULL default ' ',
  "AAAAGEFP" varchar(60) NOT NULL default ' ',
  "AAAAGEFQ" varchar(60) NOT NULL default ' ',
  "AAAAGEFR" varchar(1) NOT NULL default ' ',
  "AAAAGEFS" varchar(60) NOT NULL default ' ',
  "AAAAGEFT" varchar(60) NOT NULL default ' ',
  "AAAAGEFU" varchar(4) NOT NULL default '0000',
  "AAAAGEFV" varchar(1) NOT NULL default ' ',
  "AAAAGEFW" varchar(60) NOT NULL default ' ',
  "AAAAGEFX" varchar(60) NOT NULL default ' ',
  "AAAAGEFY" varchar(4) NOT NULL default '0000',
  "AAAAGEFZ" varchar(3) NOT NULL default '000',
  "AAAAGEGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEGF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(5) NOT NULL default '00000',
  "AAABLLBG" varchar(20) NOT NULL default ' ',
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABDTDP" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLHMS","AAABLLBG","AAABLUSV","AAABDTDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEGR" (
  "AAAAGEIF" varchar(2) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAAAGEGU" varchar(8) NOT NULL default ' ',
  "AAAAGEGV" varchar(5) NOT NULL default '00000',
  "AAAAGEGW" varchar(12) NOT NULL default ' ',
  "AAAAGEGX" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(1) NOT NULL default ' ',
  "AAAAGEHA" varchar(10) NOT NULL default ' ',
  "AAAAUKME" varchar(10) NOT NULL default '0000000000',
  "AAAAGEHC" varchar(3) NOT NULL default ' ',
  "AAABLUUV" varchar(128) NOT NULL default ' ',
  "AAAAGEHE" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAAAGEHH" varchar(12) NOT NULL default ' ',
  "AAAAGEHI" varchar(8) NOT NULL default '00000000',
  "AAAAGEHJ" varchar(6) NOT NULL default '000000',
  "AAAAGEHK" varchar(20) NOT NULL default ' ',
  "AAAAGEHL" varchar(20) NOT NULL default ' ',
  "AAAAGEHM" varchar(1) NOT NULL default ' ',
  "AAAAGEHN" varchar(3) NOT NULL default ' ',
  "AAAAGEHO" varchar(10) NOT NULL default ' ',
  "AAAAKASX" varchar(12) NOT NULL default ' ',
  "AAAAGEHQ" varchar(8) NOT NULL default '00000000',
  "AAAAKASY" varchar(6) NOT NULL default '000000',
  "AAAAGEHS" varchar(2) NOT NULL default ' ',
  "AAAAGEHT" varchar(13) NOT NULL default ' ',
  "AAAAGEHU" varchar(32) NOT NULL default ' ',
  "AAAAGEHV" varchar(3) NOT NULL default ' ',
  "AAAAGEHW" varchar(10) NOT NULL default ' ',
  "AAAAGEHX" varchar(12) NOT NULL default ' ',
  "AAAATMPV" varchar(8) NOT NULL default '00000000',
  "AAAATMPW" varchar(6) NOT NULL default '000000',
  "AAAAGEIA" varchar(8) NOT NULL default ' ',
  "AAAAGEIB" varchar(8) NOT NULL default ' ',
  "AAAAGEIC" varchar(2) NOT NULL default ' ',
  "AAAAGEID" varchar(30) NOT NULL default ' ',
  "AAAAGEIE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAGEIF","AAABLUXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEIH" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABMQYE" varchar(1) NOT NULL default ' ',
  "AAAAGEIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEIO" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABKLWC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABLCGT","AAABKLWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEIV" (
  "AAABLEEW" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEEW","AAABMRHS","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEJD" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAAAGGKS" varchar(4) NOT NULL default ' ',
  "AAAAVIRJ" varchar(60) NOT NULL default ' ',
  "AAAAGESY" varchar(2) NOT NULL default '00',
  "AAAAXZZH" varchar(4) NOT NULL default '0000',
  "AAAAGEKE" varchar(12) NOT NULL default ' ',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAABHEIG","AAAAGGKS","AAAAVIRJ","AAAAGESY","AAAAXZZH"),
  KEY "AAAAGEJT" ("AAAAHWKU"),
  KEY "AAAAGEJV" ("AAABHEIG","AAAAGGKR","AAABHEIH","AAAAHWKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEKA" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAAAGEKE" varchar(12) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABHEIH","AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEKK" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAAAGELQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABMRHS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGELC" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGELP" varchar(60) NOT NULL default ' ',
  "AAAAGELQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABMRHS"),
  KEY "AAAAGELL" ("AAABHEIH","AAAAXMSJ","AAABMRHS","AAAAGELP","AAAAGELQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGELR" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGEMA" varchar(1) NOT NULL default ' ',
  "AAAAGEMB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAAAGGKR","AAAAHWKU","AAAAGEMA","AAAAGEMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABHEIG","AAABHEIH","AAABHEHZ","AAABHEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEMP" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAAAXZZH" varchar(4) NOT NULL default '0000',
  "AAAAGEMU" varchar(2) NOT NULL default '00',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGEMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABLTXI","AAAAXZZH"),
  KEY "AAAAGENC" ("AAAAGGKR","AAAAHWKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGENF" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAAAXZZH" varchar(4) NOT NULL default '0000',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGESG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABMRHS","AAABLTXI","AAAAXZZH","AAAAGGKR","AAAAHWKU"),
  KEY "AAAAGENV" ("AAABHEIH","AAABMRHS","AAAAHWKU","AAAAGGKR"),
  KEY "AAAAGEOA" ("AAAAHWKU","AAAAGGKR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEOE" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAAAGEOS" varchar(12) NOT NULL default ' ',
  "AAAAGEPR" varchar(4) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHEIH","AAABHEIG"),
  KEY "AAAAGEOR" ("AAAAGEOS"),
  KEY "AAAAGEOT" ("AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEOV" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAABMRHS","AAABHEIG"),
  KEY "AAAAGEPD" ("AAABHEIG","AAABHEIH","AAABMRHS","AAABLFZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEPI" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABHEIG" varchar(12) NOT NULL default ' ',
  "AAAAGEPR" varchar(4) NOT NULL default ' ',
  "AAAAXZZH" varchar(5) NOT NULL default '00000',
  "AAAAGEPN" varchar(20) NOT NULL default ' ',
  "AAAAGEPO" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIH","AAABHEIG","AAAAGEPR","AAAAXZZH"),
  KEY "AAAAGEPT" ("AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEPV" (
  "AAABGEHW" varchar(2) NOT NULL default ' ',
  "AAABBKCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEPZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGEHW" varchar(2) NOT NULL default ' ',
  "AAABASZN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGEHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEQF" (
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABLECD","AAABHEIH","AAAAXMSJ","AAABMRHS","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEQT" (
  "AAAAGGKS" varchar(4) NOT NULL default ' ',
  "AAAAVIRJ" varchar(60) NOT NULL default ' ',
  "AAAAGERM" varchar(1) NOT NULL default ' ',
  "AAAAGESB" varchar(1) NOT NULL default ' ',
  "AAAAGGKJ" varchar(1) NOT NULL default ' ',
  "AAAAGERP" varchar(4) NOT NULL default '0000',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGESA" varchar(1) NOT NULL default ' ',
  "AAAAGERU" varchar(1) NOT NULL default ' ',
  "AAAAGERV" varchar(1) NOT NULL default ' ',
  "AAAAGERW" varchar(4) NOT NULL default '0000',
  "AAABLHOZ" varchar(1) NOT NULL default ' ',
  "AAAAGGKK" varchar(1) NOT NULL default ' ',
  "AAAAGERJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGGKS","AAAAVIRJ","AAAAGERM","AAAAGESB","AAAAGGKJ","AAAAGERP","AAABMJXI","AAAAGGKR","AAAAHWKU","AAAAGESA","AAAAGERU","AAAAGERV","AAAAGERW"),
  KEY "AAAAGERX" ("AAAAGGKR","AAAAHWKU","AAAAGESA","AAAAGESB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGESC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAGEST" varchar(4) NOT NULL default '0000',
  "AAAAGESG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHEIH","AAAAGEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGESK" (
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAGGKS" varchar(4) NOT NULL default ' ',
  "AAAAVIRJ" varchar(60) NOT NULL default ' ',
  "AAAAGESY" varchar(1) NOT NULL default '0',
  "AAAAGESZ" varchar(2) NOT NULL default '00',
  "AAAAGGKR" varchar(4) NOT NULL default ' ',
  "AAAAHWKU" varchar(60) NOT NULL default ' ',
  "AAAAGEST" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLECD","AAABHEIH","AAAAGGKS","AAAAVIRJ","AAAAGESY","AAAAGESZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGETA" (
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGETD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJXI" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMJXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGETJ" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABLTXI","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGETV" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABHEIH","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEUD" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEUN" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAAAGEVN" varchar(4) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAAAGEVR" varchar(4) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABLTXI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEVG" (
  "AAABHEIH" varchar(12) NOT NULL default ' ',
  "AAAAXMSJ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAAAGEVN" varchar(4) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAAAGEVR" varchar(4) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHEIH","AAAAXMSJ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEVX" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAAATVUE" varchar(1) NOT NULL default ' ',
  "AAAAGEWE" varchar(16) NOT NULL default ' ',
  "AAAAGEWF" varchar(8) NOT NULL default ' ',
  "AAAATVUG" varchar(12) NOT NULL default ' ',
  "AAAATVUH" varchar(4) NOT NULL default ' ',
  "AAAATVUI" varchar(8) NOT NULL default '00000000',
  "AAAATVUJ" varchar(6) NOT NULL default '000000',
  "AAAAVEFF" varchar(12) NOT NULL default ' ',
  "AAAATVUL" varchar(4) NOT NULL default ' ',
  "AAAAVEFG" varchar(8) NOT NULL default '00000000',
  "AAAATVUN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW","AAAATVUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEWT" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEXE" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAAAGEXS" varchar(6) NOT NULL default '000000',
  "AAABGEJG" varchar(2) NOT NULL default ' ',
  "AAABGEJH" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW","AAAATVUS","AAAAGEXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEXT" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAAATVUF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW","AAAATVUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEYF" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABGGIZ" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAABLPJD","AAABGGIZ","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEYQ" (
  "AAABLHPM" varchar(5) NOT NULL default ' ',
  "AAAAGGBR" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLRQQ" varchar(20) NOT NULL default ' ',
  "AAABHIQM" varchar(30) NOT NULL default ' ',
  "AAABDKEL" varchar(30) NOT NULL default ' ',
  "AAAALJNG" varchar(1) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGEZX" varchar(32) NOT NULL default ' ',
  "AAAAGEZE" varchar(10) NOT NULL default ' ',
  "AAAAGEZF" varchar(5) NOT NULL default ' ',
  "AAAAGEZG" varchar(2) NOT NULL default ' ',
  "AAAAGEZH" varchar(20) NOT NULL default ' ',
  "AAABGDGK" varchar(10) NOT NULL default ' ',
  "AAAAGEZJ" varchar(1) NOT NULL default ' ',
  "AAAAGEZK" varchar(70) NOT NULL default ' ',
  "AAAAGEZL" varchar(12) NOT NULL default ' ',
  "AAAAGEZM" varchar(14) NOT NULL default '00000000000000',
  "AAAAGEZN" varchar(12) NOT NULL default ' ',
  "AAAAGEZO" varchar(14) NOT NULL default '00000000000000',
  "AAAAKNLU" varchar(32) NOT NULL default ' ',
  "AAAAGEZQ" varchar(10) NOT NULL default ' ',
  "AAAAGEZR" varchar(1) NOT NULL default ' ',
  "AAAAGEZS" varchar(12) NOT NULL default ' ',
  "AAAAGEZT" varchar(14) NOT NULL default '00000000000000',
  "AAAAGEZU" varchar(12) NOT NULL default ' ',
  "AAAAGEZV" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAAAGEZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGEZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR"),
  KEY "AAAAGFAH" ("AAABMRIL","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR"),
  KEY "AAAAGFAT" ("AAABMRIL","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFAW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR"),
  KEY "AAAAGFBD" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLLR"),
  KEY "AAAAGFCA" ("AAABLLLI","AAABLLLH","AAABLLLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLLR"),
  KEY "AAAAGFCZ" ("AAABLLLI","AAABLLLH","AAABLLLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFDD" (
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
  KEY "AAAAGFDW" ("AAABLLLI","AAABLLLH","AAABLLLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFEA" (
  "AAABLHPM" varchar(5) NOT NULL default ' ',
  "AAAAGFEC" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(1) NOT NULL default ' ',
  "AAAAGFEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFEG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLHPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR","AAABHZJH"),
  UNIQUE KEY "AAAAGFFD" ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR","AAABHZJH"),
  UNIQUE KEY "AAAAGFFX" ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFGA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH"),
  UNIQUE KEY "AAAAGFGP" ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABMLQA","AAABLLLR","AAABHZJH","AAABLUMP"),
  KEY "AAAAGFHH" ("AAABMRIL","AAABLUNI","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABMLQA","AAABLLLR","AAABHZJH","AAABLUMP"),
  KEY "AAAAGFIN" ("AAABMRIL","AAABLUNI","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFIR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLQA","AAABLLLR","AAABHZJH","AAABLUMP"),
  KEY "AAAAGFJF" ("AAABLUNI","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFJU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR","AAABHZJH","AAABHZGV"),
  KEY "AAAAGFKW" ("AAABMRIL","AAABLLLR"),
  KEY "AAAAGFKZ" ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGFLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR","AAABHZJH","AAABHZGV"),
  KEY "AAAAGFMI" ("AAABMRIL","AAABLLLR"),
  KEY "AAAAGFML" ("AAABMRIL","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
