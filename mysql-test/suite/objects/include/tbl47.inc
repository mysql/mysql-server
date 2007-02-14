# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl47.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABLFSC" (
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLGDA" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(75) NOT NULL default ' ',
  "AAABLFZO" varchar(20) NOT NULL default ' ',
  "AAABLFSH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCZ","AAABLGDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFSK" (
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLFSM" varchar(64) NOT NULL default ' ',
  "AAABLFSN" varchar(30) NOT NULL default ' ',
  "AAABLFSO" varchar(10) NOT NULL default ' ',
  "AAABLFSP" varchar(10) NOT NULL default ' ',
  "AAABLFSQ" varchar(90) NOT NULL default ' ',
  "AAABLFSR" varchar(128) NOT NULL default ' ',
  "AAABLFSS" varchar(128) NOT NULL default ' ',
  "AAABLFST" varchar(128) NOT NULL default ' ',
  "AAABLFSU" varchar(128) NOT NULL default ' ',
  "AAABLFSV" varchar(128) NOT NULL default ' ',
  "AAABLFSW" varchar(128) NOT NULL default ' ',
  "AAABLFSX" varchar(128) NOT NULL default ' ',
  "AAABLFSY" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLFTT" varchar(1) NOT NULL default ' ',
  "AAABLFTU" varchar(1) NOT NULL default ' ',
  "AAABLFTV" varchar(6) NOT NULL default '000000',
  "AAABLFTW" varchar(10) NOT NULL default ' ',
  "AAABLFTX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFTK" (
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAABLFTQ" varchar(10) NOT NULL default ' ',
  "AAABLGBU" varchar(2) NOT NULL default ' ',
  "AAABLFTS" varchar(30) NOT NULL default ' ',
  "AAABLFTT" varchar(1) NOT NULL default ' ',
  "AAABLFTU" varchar(1) NOT NULL default ' ',
  "AAABLFTV" varchar(6) NOT NULL default '000000',
  "AAABLFTW" varchar(10) NOT NULL default ' ',
  "AAABLFTX" varchar(10) NOT NULL default ' ',
  "AAABLFTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFUA" (
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFUD" (
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMOZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFUJ" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABLMOZ" varchar(10) NOT NULL default ' ',
  "AAABLFWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFUO" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFUU" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  "AAABLFUY" varchar(4) NOT NULL default '0000',
  "AAABLFWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABLGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFVC" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABMRHS","AAABLGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFVK" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABLFWF" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABMRHS","AAABLGEA","AAABLFWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFVU" (
  "AAABMKGJ" varchar(80) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABLFWF" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(80) NOT NULL default ' ',
  "AAABMNMZ" varchar(1) NOT NULL default ' ',
  "AAABMNNA" varchar(1) NOT NULL default ' ',
  "AAABLFWB" varchar(1) NOT NULL default ' ',
  "AAABLFWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABLGEA","AAABLFWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFWG" (
  "AAABLFWQ" varchar(80) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFWQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFWM" (
  "AAABLFWQ" varchar(80) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLFWP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFWR" (
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABLFWT" varchar(6) NOT NULL default ' ',
  "AAABLFWU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFWW" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(4) NOT NULL default '0000',
  "AAABLFWZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFXC" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLFXL" varchar(4) NOT NULL default ' ',
  "AAABLZKF" varchar(40) NOT NULL default ' ',
  "AAABLFXN" varchar(10) NOT NULL default '0000000000',
  "AAABLFXO" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMOHM","AAABMLPV","AAABLFXL","AAABLZKF","AAABLFXN","AAABLFXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFXP" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABLFZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFXX" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABLFZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABLFYJ" varchar(5) NOT NULL default '00000',
  "AAABLMUZ" varchar(1) NOT NULL default ' ',
  "AAABLFYL" varchar(2) NOT NULL default '00',
  "AAABLOSB" varchar(5) NOT NULL default '00000',
  "AAABLHAH" varchar(100) NOT NULL default ' ',
  "AAABLFYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMBXY" varchar(5) NOT NULL default '00000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFZC" (
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFZF" (
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZT","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFZL" (
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  "AAABLGDA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZT","AAABLGDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFZR" (
  "AAABLFZT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFZU" (
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABLFZX" varchar(8) NOT NULL default '00000000',
  "AAABLFZY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLPJD","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGAB" (
  "AAABLXFQ" VARBINARY(255) NOT NULL,
  "AAABLGAD" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABLXFQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGAF" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABLGBU" varchar(2) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABLGAP" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLGBU","AAABMFSV","AAABLGAP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGAR" (
  "AAABLGAU" varchar(15) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLGAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGAV" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABLGBU" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLGBU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGBD" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABLGBU" varchar(2) NOT NULL default ' ',
  "AAABLGBL" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLGBU","AAABLGBL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGBN" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABLGBU" varchar(2) NOT NULL default ' ',
  "AAABLGBV" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLGBU","AAABLGBV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGBX" (
  "AAABMOHM" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGCA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(16) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGCG" (
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABLGCJ" varchar(1) NOT NULL default ' ',
  "AAABLGCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGCN" (
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLGDB" varchar(64) NOT NULL default ' ',
  "AAABMBIU" varchar(80) NOT NULL default ' ',
  "AAABLGCR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCZ","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGCU" (
  "AAABLGCZ" varchar(30) NOT NULL default ' ',
  "AAABLGDA" varchar(1) NOT NULL default ' ',
  "AAABLGDB" varchar(64) NOT NULL default ' ',
  "AAABLPWW" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCZ","AAABLGDA","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGDC" (
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(40) NOT NULL default ' ',
  "AAABLGDL" varchar(64) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLGDH" varchar(8) NOT NULL default '00000000',
  "AAABLGDI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMIRY","AAABMIRX","AAABLGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGDM" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABLGDP" varchar(80) NOT NULL default ' ',
  "AAABLGDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGDT" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEA" varchar(64) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABMRHS","AAABLGEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGEB" (
  "AAABLGEF" varchar(14) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGEH" (
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABLGEL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLGEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGEM" (
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLGVR" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLGSP" varchar(1) NOT NULL default ' ',
  "AAABLLPO" varchar(1) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABLGSX" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLGTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLGTK" varchar(5) NOT NULL default ' ',
  "AAABLPDT" decimal(5,0) NOT NULL default '0',
  "AAABLGUD" varchar(3) NOT NULL default ' ',
  "AAABLGPI" varchar(1) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLGSW" varchar(10) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLRMN","AAABLGVR","AAABMCSR","AAABLURS","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLGVR" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLSGZ" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  "AAABLSJO" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUS" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUU" decimal(13,3) NOT NULL default '0.000',
  "AAABLRYH" varchar(1) NOT NULL default ' ',
  "AAABLGTD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLRMN","AAABLGVR","AAABMPLT","AAABLSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLGHH" varchar(8) NOT NULL default '00000000',
  "AAABLGHI" varchar(8) NOT NULL default '00000000',
  "AAABLGVA" varchar(8) NOT NULL default '00000000',
  "AAABLGVB" varchar(8) NOT NULL default '00000000',
  "AAABLGHL" varchar(8) NOT NULL default '00000000',
  "AAABLGPH" varchar(18) NOT NULL default ' ',
  "AAABLGNW" varchar(4) NOT NULL default ' ',
  "AAABLGHO" varchar(8) NOT NULL default '00000000',
  "AAABLGHP" varchar(1) NOT NULL default ' ',
  "AAABLGHQ" varchar(8) NOT NULL default '00000000',
  "AAABLGHR" smallint(6) NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLGME" varchar(8) NOT NULL default '00000000',
  "AAABLGMF" varchar(8) NOT NULL default '00000000',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABMDGI" varchar(8) NOT NULL default '00000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLGIB" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLGMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLXUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABLNWR" varchar(8) NOT NULL default '00000000',
  "AAABLGIO" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ","AAABLUII","AAABLNWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABLNWR" varchar(8) NOT NULL default '00000000',
  "AAABLGJA" decimal(13,3) NOT NULL default '0.000',
  "AAABLGJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ","AAABLUII","AAABLNWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLGJN" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLGJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMDBL" varchar(4) NOT NULL default ' ',
  "AAABLGJS" varchar(4) NOT NULL default ' ',
  "AAABLGTM" varchar(5) NOT NULL default ' ',
  "AAABLGJU" varchar(5) NOT NULL default ' ',
  "AAABLGJV" varchar(10) NOT NULL default ' ',
  "AAABLGJW" varchar(2) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLGNW" varchar(4) NOT NULL default ' ',
  "AAABLGJZ" varchar(6) NOT NULL default ' ',
  "AAABLGKA" varchar(1) NOT NULL default ' ',
  "AAABLGZH" varchar(1) NOT NULL default ' ',
  "AAABLGKC" varchar(1) NOT NULL default ' ',
  "AAABLGKD" varchar(1) NOT NULL default ' ',
  "AAABLGKE" varchar(1) NOT NULL default ' ',
  "AAABLGKF" varchar(1) NOT NULL default ' ',
  "AAABLGKG" smallint(6) NOT NULL default '0',
  "AAABLGKH" smallint(6) NOT NULL default '0',
  "AAABLGKI" smallint(6) NOT NULL default '0',
  "AAABLGKJ" smallint(6) NOT NULL default '0',
  "AAABLGKK" smallint(6) NOT NULL default '0',
  "AAABLGKL" smallint(6) NOT NULL default '0',
  "AAABLGKM" smallint(6) NOT NULL default '0',
  "AAABLGKN" smallint(6) NOT NULL default '0',
  "AAABLGKO" smallint(6) NOT NULL default '0',
  "AAABLGKP" smallint(6) NOT NULL default '0',
  "AAABLGKQ" smallint(6) NOT NULL default '0',
  "AAABLGKR" varchar(1) NOT NULL default ' ',
  "AAABLGTN" varchar(8) NOT NULL default '00000000',
  "AAABLGTO" varchar(8) NOT NULL default '00000000',
  "AAABLOWF" varchar(8) NOT NULL default '00000000',
  "AAABLGTQ" varchar(8) NOT NULL default '00000000',
  "AAABLGVA" varchar(8) NOT NULL default '00000000',
  "AAABLGVB" varchar(8) NOT NULL default '00000000',
  "AAABLQLC" varchar(8) NOT NULL default '00000000',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  "AAABLGPK" varchar(1) NOT NULL default ' ',
  "AAABLGPL" varchar(1) NOT NULL default ' ',
  "AAABLGLC" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLGLH" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLI" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLJ" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLK" decimal(13,3) NOT NULL default '0.000',
  "AAABLGLL" varchar(6) NOT NULL default '000000',
  "AAABLGLM" varchar(3) NOT NULL default ' ',
  "AAABLGLN" varchar(3) NOT NULL default ' ',
  "AAABLGLO" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLP" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLQ" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLR" decimal(13,3) NOT NULL default '0.000',
  "AAABLGLS" varchar(6) NOT NULL default '000000',
  "AAABLGLT" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLU" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLV" decimal(13,2) NOT NULL default '0.00',
  "AAABLGLW" decimal(13,3) NOT NULL default '0.000',
  "AAABLGLX" varchar(20) NOT NULL default ' ',
  "AAABLGLY" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLGMD" varchar(4) NOT NULL default ' ',
  "AAABLGME" varchar(8) NOT NULL default '00000000',
  "AAABLGMF" varchar(8) NOT NULL default '00000000',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABMDGI" varchar(8) NOT NULL default '00000000',
  "AAABLSTP" varchar(10) NOT NULL default ' ',
  "AAABLGPS" varchar(1) NOT NULL default ' ',
  "AAABLGMK" varchar(1) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABLGQD" varchar(1) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLGMP" varchar(1) NOT NULL default ' ',
  "AAABLGMQ" varchar(4) NOT NULL default ' ',
  "AAABLGMR" varchar(4) NOT NULL default ' ',
  "AAABLGMS" decimal(13,2) NOT NULL default '0.00',
  "AAABLGMT" decimal(13,2) NOT NULL default '0.00',
  "AAABLGMU" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLWZD"),
  KEY "AAABLGMX" ("AAABMRIL","AAABLGTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLWZD","AAABLYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLGVR" varchar(3) NOT NULL default ' ',
  "AAABLGNO" varchar(6) NOT NULL default '000000',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  "AAABLGSX" varchar(1) NOT NULL default ' ',
  "AAABLGNT" varchar(1) NOT NULL default ' ',
  "AAABLGNU" smallint(6) NOT NULL default '0',
  "AAABLGNV" varchar(1) NOT NULL default ' ',
  "AAABLGNW" varchar(4) NOT NULL default ' ',
  "AAABLGNX" varchar(8) NOT NULL default '00000000',
  "AAABLGNY" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLGOD" decimal(3,0) NOT NULL default '0',
  "AAABLGOE" decimal(13,2) NOT NULL default '0.00',
  "AAABLGOF" decimal(13,2) NOT NULL default '0.00',
  "AAABLGOG" decimal(13,2) NOT NULL default '0.00',
  "AAABLGUU" decimal(13,3) NOT NULL default '0.000',
  "AAABLGOI" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" decimal(13,3) NOT NULL default '0.000',
  "AAABLGTL" decimal(11,2) NOT NULL default '0.00',
  "AAABLGUC" decimal(5,0) NOT NULL default '0',
  "AAABLGTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLGON" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDT" decimal(5,0) NOT NULL default '0',
  "AAABLGTK" varchar(5) NOT NULL default ' ',
  "AAABLGTM" varchar(5) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLGOS" varchar(3) NOT NULL default ' ',
  "AAABLGUD" varchar(3) NOT NULL default ' ',
  "AAABLGOU" varchar(3) NOT NULL default ' ',
  "AAABLGOV" varchar(1) NOT NULL default ' ',
  "AAABLGUV" decimal(13,3) NOT NULL default '0.000',
  "AAABLGOX" varchar(2) NOT NULL default ' ',
  "AAABLMEJ" varchar(8) NOT NULL default '00000000',
  "AAABLGOZ" varchar(8) NOT NULL default '00000000',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABMDGI" varchar(8) NOT NULL default '00000000',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLGPE" varchar(10) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLGPH" varchar(18) NOT NULL default ' ',
  "AAABLGPI" varchar(1) NOT NULL default ' ',
  "AAABLPDM" varchar(1) NOT NULL default ' ',
  "AAABLGPK" varchar(1) NOT NULL default ' ',
  "AAABLGPL" varchar(1) NOT NULL default ' ',
  "AAABMQBS" tinyint(3) unsigned NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLGUY" varchar(1) NOT NULL default ' ',
  "AAABLGPS" varchar(1) NOT NULL default ' ',
  "AAABLGPT" varchar(1) NOT NULL default ' ',
  "AAABLOPF" varchar(6) NOT NULL default ' ',
  "AAABLGVD" decimal(11,2) NOT NULL default '0.00',
  "AAABLGPW" varchar(8) NOT NULL default '00000000',
  "AAABLGPX" varchar(8) NOT NULL default '00000000',
  "AAABLGPY" varchar(18) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABLGQU" decimal(11,2) NOT NULL default '0.00',
  "AAABLGQW" decimal(5,2) NOT NULL default '0.00',
  "AAABLGQX" varchar(1) NOT NULL default ' ',
  "AAABLGQD" varchar(1) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLRMN","AAABLGVR"),
  KEY "AAABLGQJ" ("AAABMRIL","AAABLRMN","AAABLGVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLGQR" varchar(2) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLGQT" varchar(18) NOT NULL default ' ',
  "AAABLGQU" decimal(11,2) NOT NULL default '0.00',
  "AAABLGQV" varchar(5) NOT NULL default ' ',
  "AAABLGQW" decimal(5,2) NOT NULL default '0.00',
  "AAABLGQX" varchar(1) NOT NULL default ' ',
  "AAABLGRJ" varchar(8) NOT NULL default '00000000',
  "AAABLGRK" varchar(8) NOT NULL default '00000000',
  "AAABLGRA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLGRJ" varchar(8) NOT NULL default '00000000',
  "AAABLGRK" varchar(8) NOT NULL default '00000000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABMLTD","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGRU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLGVR" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLGVN" varchar(10) NOT NULL default '0000000000',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  "AAABLGSH" varchar(1) NOT NULL default ' ',
  "AAABLGSI" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLGSP" varchar(1) NOT NULL default ' ',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABLOOH" varchar(1) NOT NULL default ' ',
  "AAABLGSS" varchar(4) NOT NULL default ' ',
  "AAABLGST" varchar(4) NOT NULL default ' ',
  "AAABLGSU" varchar(2) NOT NULL default ' ',
  "AAABLGSV" varchar(2) NOT NULL default ' ',
  "AAABLGSW" varchar(10) NOT NULL default ' ',
  "AAABLGSX" varchar(1) NOT NULL default ' ',
  "AAABLGSY" varchar(4) NOT NULL default ' ',
  "AAABLGSZ" varchar(4) NOT NULL default ' ',
  "AAABLGTA" varchar(8) NOT NULL default '00000000',
  "AAABLGTB" varchar(8) NOT NULL default '00000000',
  "AAABLGTC" varchar(8) NOT NULL default '00000000',
  "AAABLGTD" varchar(1) NOT NULL default ' ',
  "AAABLGTE" varchar(1) NOT NULL default ' ',
  "AAABLGTF" varchar(1) NOT NULL default ' ',
  "AAABLGTG" varchar(1) NOT NULL default ' ',
  "AAABLGTH" varchar(1) NOT NULL default ' ',
  "AAABLGTI" varchar(1) NOT NULL default ' ',
  "AAABLGTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLGTK" varchar(5) NOT NULL default ' ',
  "AAABLGTL" decimal(11,2) NOT NULL default '0.00',
  "AAABLGTM" varchar(5) NOT NULL default ' ',
  "AAABLGTN" varchar(8) NOT NULL default '00000000',
  "AAABLGTO" varchar(8) NOT NULL default '00000000',
  "AAABLOWF" varchar(8) NOT NULL default '00000000',
  "AAABLGTQ" varchar(8) NOT NULL default '00000000',
  "AAABLPEA" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEB" decimal(11,2) NOT NULL default '0.00',
  "AAABLPED" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEE" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEF" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEG" decimal(11,2) NOT NULL default '0.00',
  "AAABLGTX" varchar(3) NOT NULL default ' ',
  "AAABLPEH" decimal(11,2) NOT NULL default '0.00',
  "AAABLGTZ" varchar(1) NOT NULL default ' ',
  "AAABLPDT" decimal(5,0) NOT NULL default '0',
  "AAABLGUB" decimal(5,0) NOT NULL default '0',
  "AAABLGUC" decimal(5,0) NOT NULL default '0',
  "AAABLGUD" varchar(3) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLGUI" varchar(1) NOT NULL default ' ',
  "AAABLGUJ" varchar(1) NOT NULL default ' ',
  "AAABLGUK" varchar(1) NOT NULL default ' ',
  "AAABLGUL" varchar(1) NOT NULL default ' ',
  "AAABLGUM" varchar(1) NOT NULL default ' ',
  "AAABLGUN" varchar(1) NOT NULL default ' ',
  "AAABLGUO" varchar(1) NOT NULL default ' ',
  "AAABLGUP" varchar(1) NOT NULL default ' ',
  "AAABLLPO" varchar(1) NOT NULL default ' ',
  "AAABLGUR" varchar(1) NOT NULL default ' ',
  "AAABLGUS" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJO" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUU" decimal(13,3) NOT NULL default '0.000',
  "AAABLGUV" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLGUX" decimal(11,2) NOT NULL default '0.00',
  "AAABLGUY" varchar(1) NOT NULL default ' ',
  "AAABLGUZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLGVA" varchar(8) NOT NULL default '00000000',
  "AAABLGVB" varchar(8) NOT NULL default '00000000',
  "AAABLOPF" varchar(6) NOT NULL default ' ',
  "AAABLGVD" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLRMN","AAABLGVR","AAABMPLT","AAABLXUH"),
  UNIQUE KEY "AAABLGVK" ("AAABMRIL","AAABLWZD","AAABLGVN"),
  KEY "AAABLGVO" ("AAABMRIL","AAABLRMN","AAABLGVR","AAABMPLT"),
  KEY "AAABLGVT" ("AAABMRIL","AAABMPLT","AAABLOWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABLGXW" varchar(4) NOT NULL default ' ',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLMFQ" varchar(6) NOT NULL default '000000',
  "AAABLMFR" varchar(6) NOT NULL default '000000',
  "AAABLWJT" varchar(30) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLUGX" varchar(10) NOT NULL default ' ',
  "AAABLGWM" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABLGWO" varchar(6) NOT NULL default '000000',
  "AAABLGWP" varchar(6) NOT NULL default '000000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABLGWR" varchar(8) NOT NULL default '00000000',
  "AAABLGWS" varchar(6) NOT NULL default '000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABLGWU" varchar(6) NOT NULL default '000000',
  "AAABLGWV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZUS","AAABMDBA","AAABLGXW"),
  KEY "AAABLGXA" ("AAABMRIL","AAABMLFK","AAABLGXS","AAABMCJZ","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLGXS" varchar(3) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABLGXW" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLMFQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABLGXS","AAABMNZR","AAABLODQ","AAABMDBA","AAABLGXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLGYC" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABLGYE" varchar(30) NOT NULL default ' ',
  "AAABLGYF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABMJWN","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLGZA" varchar(3) NOT NULL default '000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  "AAABLRVX" varchar(1) NOT NULL default ' ',
  "AAABLGYR" varchar(1) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLGZG" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLGZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLGZO" varchar(4) NOT NULL default ' ',
  "AAABLGZF" smallint(6) NOT NULL default '0',
  "AAABLGZG" varchar(1) NOT NULL default ' ',
  "AAABLGZH" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLGZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLGZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLULY" varchar(10) NOT NULL default ' ',
  "AAABLGZT" smallint(6) NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLGZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLWZD","AAABLULY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHAJ" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHAJ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLLJR" varchar(18) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLIMG" varchar(6) NOT NULL default '000000',
  "AAABLHAS" decimal(15,2) NOT NULL default '0.00',
  "AAABLHAT" decimal(15,2) NOT NULL default '0.00',
  "AAABLHAU" varchar(5) NOT NULL default ' ',
  "AAABLHAV" varchar(8) NOT NULL default '00000000',
  "AAABLHAW" varchar(4) NOT NULL default ' ',
  "AAABLHAX" varchar(13) NOT NULL default ' ',
  "AAABLHAY" varchar(4) NOT NULL default ' ',
  "AAABLHAZ" varchar(13) NOT NULL default ' ',
  "AAABLHBA" varchar(4) NOT NULL default ' ',
  "AAABLHBB" varchar(2) NOT NULL default ' ',
  "AAABLHBY" varchar(1) NOT NULL default ' ',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABMKAH" decimal(15,3) NOT NULL default '0.000',
  "AAABLHBH" varchar(3) NOT NULL default ' ',
  "AAABLHBI" varchar(4) NOT NULL default ' ',
  "AAABLHBJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255)),
  KEY "AAABLHBM" ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV","AAABLHBY"),
  KEY "AAABLHBS" ("AAABMRIL","AAABLPNX","AAABLLJR","AAABLIMF","AAABLIMG","AAABLHBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHCK" (
  "AAABLHCP" varchar(30) NOT NULL default ' ',
  "AAABLHCQ" varchar(30) NOT NULL default ' ',
  "AAABLHCR" varchar(30) NOT NULL default ' ',
  "AAABLHCS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHCP","AAABLHCQ","AAABLHCR","AAABLHCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHCT" (
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLTIX" varchar(70) NOT NULL default ' ',
  "AAABLHCW" varchar(20) NOT NULL default ' ',
  "AAABLHCX" varchar(20) NOT NULL default ' ',
  "AAABLJIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHDA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLHFA" varchar(4) NOT NULL default ' ',
  "AAABLHFB" varchar(18) NOT NULL default ' ',
  "AAABLHFC" varchar(10) NOT NULL default ' ',
  "AAABLHFD" varchar(6) NOT NULL default '000000',
  "AAABLHFY" varchar(4) NOT NULL default ' ',
  "AAABLHFZ" varchar(18) NOT NULL default ' ',
  "AAABLHGA" varchar(10) NOT NULL default ' ',
  "AAABLHGB" varchar(6) NOT NULL default '000000',
  "AAABLHFI" varchar(2) NOT NULL default ' ',
  "AAABLHFL" varchar(2) NOT NULL default ' ',
  "AAABLHFM" varchar(10) NOT NULL default ' ',
  "AAABLHFN" varchar(6) NOT NULL default '000000',
  "AAABLHFO" varchar(6) NOT NULL default '000000',
  "AAABLHDX" varchar(10) NOT NULL default ' ',
  "AAABLHDY" varchar(18) NOT NULL default ' ',
  "AAABLHFR" varchar(2) NOT NULL default ' ',
  "AAABLHFS" varchar(10) NOT NULL default ' ',
  "AAABLHFT" varchar(6) NOT NULL default '000000',
  "AAABLHFU" varchar(6) NOT NULL default '000000',
  "AAABLHED" varchar(10) NOT NULL default ' ',
  "AAABLHEE" varchar(18) NOT NULL default ' ',
  "AAABLIMN" varchar(3) NOT NULL default ' ',
  "AAABLHEG" decimal(15,3) NOT NULL default '0.000',
  "AAABLHEH" decimal(15,3) NOT NULL default '0.000',
  "AAABLHQG" decimal(15,3) NOT NULL default '0.000',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  "AAABLHEK" varchar(4) NOT NULL default ' ',
  "AAABLKHG" varchar(1) NOT NULL default ' ',
  "AAABLHEM" varchar(2) NOT NULL default ' ',
  "AAABLHEN" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLHEP" varchar(10) NOT NULL default ' ',
  "AAABLHEQ" varchar(10) NOT NULL default ' ',
  "AAABLHER" varchar(4) NOT NULL default ' ',
  "AAABLHES" varchar(4) NOT NULL default ' ',
  "AAABLHET" decimal(15,3) NOT NULL default '0.000',
  "AAABLHEU" decimal(15,3) NOT NULL default '0.000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLHEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLHFA","AAABLHFB","AAABLHFC","AAABLHFD","AAABLHFY","AAABLHFZ","AAABLHGA","AAABLHGB","AAABLHFI"),
  KEY "AAABLHFJ" ("AAABMRIL","AAABLHFL","AAABLHFM","AAABLHFN","AAABLHFO"),
  KEY "AAABLHFP" ("AAABMRIL","AAABLHFR","AAABLHFS","AAABLHFT","AAABLHFU"),
  KEY "AAABLHFV" ("AAABMRIL","AAABLJBH","AAABLHFY","AAABLHFZ","AAABLHGA","AAABLHGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHGP" varchar(3) NOT NULL default ' ',
  "AAABLHGF" int(11) NOT NULL default '0',
  "AAABLHGG" int(11) NOT NULL default '0',
  "AAABLHGH" int(11) NOT NULL default '0',
  "AAABLHGI" int(11) NOT NULL default '0',
  "AAABLHGJ" int(11) NOT NULL default '0',
  "AAABLHGK" int(11) NOT NULL default '0',
  "AAABLHGL" int(11) NOT NULL default '0',
  "AAABLHGM" int(11) NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABLHGV" varchar(8) NOT NULL default '00000000',
  "AAABLHGW" varchar(8) NOT NULL default '00000000',
  "AAABLHGX" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLHHA" varchar(4) NOT NULL default '0000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLHHD" varchar(8) NOT NULL default '00000000',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLTA" varchar(20) NOT NULL default ' ',
  "AAABLMJR" varchar(14) NOT NULL default '00000000000000',
  "AAABLMJH" varchar(1) NOT NULL default ' ',
  "AAABLHHP" varchar(30) NOT NULL default ' ',
  "AAABLHHQ" varchar(1) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABLMJK" varchar(6) NOT NULL default '000000',
  "AAABLHIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLLSZ","AAABLHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABLHJE" varchar(10) NOT NULL default '0000000000',
  "AAABLHIE" varchar(1) NOT NULL default ' ',
  "AAABLHJF" varchar(5) NOT NULL default '00000',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLHIJ" varchar(10) NOT NULL default '0000000000',
  "AAABLHIK" varchar(20) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABLHIM" varchar(20) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLLSZ","AAABLHMS","AAABLHJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABLHJQ" int(11) NOT NULL default '0',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABLHJD" varchar(2) NOT NULL default ' ',
  "AAABLHJE" varchar(10) NOT NULL default '0000000000',
  "AAABLHJF" varchar(5) NOT NULL default '00000',
  "AAABLHJG" varchar(1) NOT NULL default ' ',
  "AAABLHJH" varchar(1) NOT NULL default ' ',
  "AAABLHJI" varchar(132) NOT NULL default ' ',
  "AAABMDCZ" varchar(10) NOT NULL default ' ',
  "AAABMDDF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLLSZ","AAABMLCZ","AAABLHMS","AAABLHJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHJR" (
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLHJZ" varchar(10) NOT NULL default ' ',
  "AAABLHJU" varchar(14) NOT NULL default '00000000000000',
  "AAABLHJV" varchar(14) NOT NULL default '00000000000000',
  "AAABLHJW" varchar(14) NOT NULL default '00000000000000',
  "AAABLHJX" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLRYG","AAABLHJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLQDA" varchar(8) NOT NULL default '00000000',
  "AAABLMJK" varchar(6) NOT NULL default '000000',
  "AAABLHKG" varchar(14) NOT NULL default '00000000000000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLLSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHOH" int(11) NOT NULL default '0',
  "AAABLHNZ" int(11) NOT NULL default '0',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLHLE" varchar(14) NOT NULL default ' ',
  "AAABLHLF" varchar(10) NOT NULL default ' ',
  "AAABLHLG" varchar(1) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHOH","AAABLHNZ","AAABLRMN","AAABLHLE","AAABLHLF","AAABLHLG","AAABLRYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHOH" int(11) NOT NULL default '0',
  "AAABLHNZ" int(11) NOT NULL default '0',
  "AAABLRWE" varchar(10) NOT NULL default ' ',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLRYH" varchar(1) NOT NULL default ' ',
  "AAABLRBQ" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLHLU" varchar(2) NOT NULL default ' ',
  "AAABLHLV" varchar(1) NOT NULL default ' ',
  "AAABLHLW" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLQNI" varchar(5) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  "AAABLHMH" varchar(4) NOT NULL default ' ',
  "AAABLXLD" varchar(3) NOT NULL default '000',
  "AAABLHMJ" varchar(1) NOT NULL default ' ',
  "AAABLHMK" varchar(1) NOT NULL default ' ',
  "AAABLQMQ" varchar(4) NOT NULL default ' ',
  "AAABLHMM" varchar(2) NOT NULL default ' ',
  "AAABLQMZ" varchar(4) NOT NULL default ' ',
  "AAABLHMO" tinyint(3) unsigned NOT NULL default '0',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLHMQ" varchar(6) NOT NULL default '000000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLHOH","AAABLHNZ","AAABLRWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHOH" int(11) NOT NULL default '0',
  "AAABLHNZ" int(11) NOT NULL default '0',
  "AAABLHNP" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLMJR" varchar(14) NOT NULL default '00000000000000',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABLHNF" varchar(14) NOT NULL default ' ',
  "AAABLHNG" varchar(14) NOT NULL default ' ',
  "AAABLHNH" varchar(14) NOT NULL default ' ',
  "AAABLMHQ" varchar(8) NOT NULL default '00000000',
  "AAABLMHS" varchar(6) NOT NULL default '000000',
  "AAABLMHR" varchar(8) NOT NULL default '00000000',
  "AAABLMHT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLHOH","AAABLHNZ","AAABLHNP","AAABLLSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHOH" int(11) NOT NULL default '0',
  "AAABLHNZ" int(11) NOT NULL default '0',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  "AAABLHNW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHOH","AAABLHNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHOH" int(11) NOT NULL default '0',
  "AAABLHOD" varchar(8) NOT NULL default '00000000',
  "AAABLHOE" varchar(6) NOT NULL default '000000',
  "AAABLHOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHOI" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(120) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABLHOZ" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABMLPV","AAABLJIM"),
  KEY "AAABLHOS" ("AAABLJIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHOU" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(120) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABLHOZ" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABMLPV","AAABLJIM"),
  KEY "AAABLHPE" ("AAABLJIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHPG" (
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(120) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHPM","AAABMLPV","AAABLJIM","AAABLHPU","AAABLVCU"),
  KEY "AAABLHPR" ("AAABLJIM"),
  KEY "AAABLHPT" ("AAABLHPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLLJR" varchar(10) NOT NULL default ' ',
  "AAABLIMF" varchar(6) NOT NULL default '000000',
  "AAABLIMG" varchar(6) NOT NULL default '000000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLIMN" varchar(3) NOT NULL default ' ',
  "AAABLHQG" decimal(15,3) NOT NULL default '0.000',
  "AAABLHQH" varchar(3) NOT NULL default ' ',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNX","AAABLLJR","AAABLIMF","AAABLIMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABLHQX" decimal(13,3) NOT NULL default '0.000',
  "AAABLHQY" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVT" varchar(1) NOT NULL default ' ',
  "AAABLHRA" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRB" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABLHRE" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRF" decimal(13,3) NOT NULL default '0.000',
  "AAABLHRG" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRH" varchar(1) NOT NULL default ' ',
  "AAABLHRI" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRK" decimal(5,0) NOT NULL default '0',
  "AAABLHRL" varchar(4) NOT NULL default ' ',
  "AAABLHRM" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLHRO" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRP" varchar(1) NOT NULL default ' ',
  "AAABLHRQ" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRR" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRS" decimal(5,0) NOT NULL default '0',
  "AAABLHRT" varchar(4) NOT NULL default ' ',
  "AAABLHRU" decimal(13,2) NOT NULL default '0.00',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLHRY" decimal(11,2) NOT NULL default '0.00',
  "AAABLHRZ" varchar(8) NOT NULL default '00000000',
  "AAABLHSA" decimal(11,2) NOT NULL default '0.00',
  "AAABLHSB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVV","AAABMCVW","AAABMRFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHSH" (
  "AAABLHSK" varchar(15) NOT NULL default ' ',
  "AAABLUWW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLHUX" varchar(6) NOT NULL default '000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLHSY" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLHVF" varchar(4) NOT NULL default ' ',
  "AAABLILV" varchar(30) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABLHVK" varchar(1) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABLHTF" varchar(4) NOT NULL default ' ',
  "AAABLHTG" decimal(13,3) NOT NULL default '0.000',
  "AAABLHTH" varchar(3) NOT NULL default ' ',
  "AAABLHTI" decimal(13,3) NOT NULL default '0.000',
  "AAABLHTJ" varchar(3) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  "AAABLHTN" varchar(1) NOT NULL default ' ',
  "AAABLILT" varchar(1) NOT NULL default ' ',
  "AAABLILL" varchar(2) NOT NULL default ' ',
  "AAABLILM" varchar(18) NOT NULL default ' ',
  "AAABLILN" varchar(10) NOT NULL default ' ',
  "AAABLILO" varchar(6) NOT NULL default '000000',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLILF" decimal(15,3) NOT NULL default '0.000',
  "AAABLILG" varchar(3) NOT NULL default ' ',
  "AAABLILZ" decimal(5,0) NOT NULL default '0',
  "AAABLIMA" decimal(5,0) NOT NULL default '0',
  "AAABMMJT" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUE" varchar(2) NOT NULL default ' ',
  "AAABLHUF" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUG" decimal(9,5) NOT NULL default '0.00000',
  "AAABLHUH" varchar(5) NOT NULL default ' ',
  "AAABLHUI" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUJ" varchar(8) NOT NULL default '00000000',
  "AAABLHUK" varchar(1) NOT NULL default ' ',
  "AAABLHUL" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUM" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUN" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUO" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUP" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLHUR" decimal(9,5) NOT NULL default '0.00000',
  "AAABLHUS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNX","AAABMQIL","AAABMLTD","AAABLHUX","AAABMDBB"),
  KEY "AAABLHUZ" ("AAABMRIL","AAABLIMT"),
  KEY "AAABLHVC" ("AAABMRIL","AAABLJBH","AAABLHVF","AAABMQIL"),
  KEY "AAABLHVH" ("AAABMRIL","AAABLJBH","AAABLHVK"),
  KEY "AAABLHVL" ("AAABMRIL","AAABLILV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABLHVT" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLXIX" varchar(12) NOT NULL default ' ',
  "AAABLHVX" varchar(8) NOT NULL default '00000000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABLHVZ" varchar(1) NOT NULL default ' ',
  "AAABLICU" varchar(5) NOT NULL default ' ',
  "AAABLHWB" decimal(9,5) NOT NULL default '0.00000',
  "AAABLHWC" varchar(4) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLWYU" varchar(8) NOT NULL default '00000000',
  "AAABLHWF" varchar(80) NOT NULL default ' ',
  "AAABLHWG" varchar(8) NOT NULL default '00000000',
  "AAABLIGM" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLHWL" varchar(4) NOT NULL default ' ',
  "AAABLHWM" varchar(3) NOT NULL default ' ',
  "AAABLHWN" varchar(28) NOT NULL default ' ',
  "AAABLHWO" varchar(8) NOT NULL default '00000000',
  "AAABLHWP" varchar(8) NOT NULL default '00000000',
  "AAABLHWQ" varchar(1) NOT NULL default ' ',
  "AAABLHWR" varchar(3) NOT NULL default ' ',
  "AAABLHWS" varchar(28) NOT NULL default ' ',
  "AAABLHWT" varchar(4) NOT NULL default ' ',
  "AAABLHWU" varchar(80) NOT NULL default ' ',
  "AAABLJAL" varchar(8) NOT NULL default '00000000',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLHWX" varchar(8) NOT NULL default '00000000',
  "AAABLHWY" varchar(8) NOT NULL default '00000000',
  "AAABLHWZ" varchar(10) NOT NULL default ' ',
  "AAABLHXA" varchar(10) NOT NULL default ' ',
  "AAABLHXB" varchar(30) NOT NULL default ' ',
  "AAABLHXC" varchar(16) NOT NULL default ' ',
  "AAABLHXD" varchar(10) NOT NULL default ' ',
  "AAABLHXE" varchar(12) NOT NULL default ' ',
  "AAABLHXF" varchar(2) NOT NULL default ' ',
  "AAABLHXG" varchar(4) NOT NULL default ' ',
  "AAABLHXH" varchar(1) NOT NULL default ' ',
  "AAABLHXI" varchar(20) NOT NULL default ' ',
  "AAABLHXJ" varchar(1) NOT NULL default ' ',
  "AAABLHXK" varchar(1) NOT NULL default ' ',
  "AAABLIGQ" varchar(10) NOT NULL default ' ',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  "AAABLIGR" decimal(5,2) NOT NULL default '0.00',
  "AAABLHXO" varchar(10) NOT NULL default ' ',
  "AAABLIYS" varchar(1) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABLHXR" varchar(35) NOT NULL default ' ',
  "AAABLHXS" varchar(6) NOT NULL default ' ',
  "AAABLHXT" varchar(35) NOT NULL default ' ',
  "AAABLXIV" varchar(8) NOT NULL default '00000000',
  "AAABLHXV" varchar(8) NOT NULL default '00000000',
  "AAABLHXW" varchar(6) NOT NULL default ' ',
  "AAABLHXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABLHYX" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLYYI" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLHYP" varchar(8) NOT NULL default '00000000',
  "AAABLHYQ" varchar(8) NOT NULL default '00000000',
  "AAABLHYR" varchar(1) NOT NULL default ' ',
  "AAABLHYS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV","AAABLHYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLHYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIDS" varchar(10) NOT NULL default ' ',
  "AAABLIDT" varchar(6) NOT NULL default '000000',
  "AAABLIDU" varchar(6) NOT NULL default '000000',
  "AAABLHZY" varchar(10) NOT NULL default ' ',
  "AAABLHZZ" varchar(6) NOT NULL default '000000',
  "AAABLIAA" varchar(2) NOT NULL default ' ',
  "AAABLHZG" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  "AAABLHZK" decimal(15,3) NOT NULL default '0.000',
  "AAABLHZL" decimal(15,3) NOT NULL default '0.000',
  "AAABLHZM" varchar(3) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLIDS","AAABLIDT","AAABLIDU","AAABLHZY","AAABLHZZ","AAABLIAA"),
  KEY "AAABLHZW" ("AAABMRIL","AAABLHZY","AAABLHZZ","AAABLIAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLYYI" decimal(15,3) NOT NULL default '0.000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLXPV" decimal(5,0) NOT NULL default '0',
  "AAABLXPW" decimal(5,0) NOT NULL default '0',
  "AAABLIAN" varchar(3) NOT NULL default ' ',
  "AAABLXOC" varchar(40) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLIAR" varchar(32) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLIAU" varchar(3) NOT NULL default ' ',
  "AAABLIAV" decimal(5,0) NOT NULL default '0',
  "AAABLIAW" decimal(5,0) NOT NULL default '0',
  "AAABLIAX" decimal(5,0) NOT NULL default '0',
  "AAABLIAY" decimal(5,0) NOT NULL default '0',
  "AAABLIAZ" decimal(5,0) NOT NULL default '0',
  "AAABLIBA" decimal(11,2) NOT NULL default '0.00',
  "AAABLIBB" varchar(5) NOT NULL default ' ',
  "AAABLIBC" decimal(11,2) NOT NULL default '0.00',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLIBH" varchar(1) NOT NULL default ' ',
  "AAABLIBI" decimal(3,1) NOT NULL default '0.0',
  "AAABLIBJ" decimal(3,1) NOT NULL default '0.0',
  "AAABLIBK" varchar(4) NOT NULL default ' ',
  "AAABLIBL" varchar(1) NOT NULL default ' ',
  "AAABLYWK" varchar(2) NOT NULL default ' ',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABLXOW" varchar(35) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABLXPY" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABLXQW" varchar(1) NOT NULL default ' ',
  "AAABLXQX" varchar(1) NOT NULL default ' ',
  "AAABLXQY" varchar(1) NOT NULL default ' ',
  "AAABLXQZ" varchar(1) NOT NULL default ' ',
  "AAABLXRA" varchar(1) NOT NULL default ' ',
  "AAABLXRB" varchar(1) NOT NULL default ' ',
  "AAABLXRC" varchar(1) NOT NULL default ' ',
  "AAABLXRD" varchar(1) NOT NULL default ' ',
  "AAABLXRE" varchar(1) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  "AAABLICN" varchar(2) NOT NULL default ' ',
  "AAABLICO" decimal(1,0) NOT NULL default '0',
  "AAABLICP" varchar(1) NOT NULL default ' ',
  "AAABLICQ" varchar(4) NOT NULL default ' ',
  "AAABLICR" varchar(6) NOT NULL default ' ',
  "AAABLIGI" varchar(5) NOT NULL default ' ',
  "AAABLICT" decimal(15,2) NOT NULL default '0.00',
  "AAABLICU" varchar(5) NOT NULL default ' ',
  "AAABLICV" decimal(13,2) NOT NULL default '0.00',
  "AAABLICW" varchar(2) NOT NULL default ' ',
  "AAABLICX" varchar(1) NOT NULL default ' ',
  "AAABLICY" decimal(3,1) NOT NULL default '0.0',
  "AAABLICZ" varchar(1) NOT NULL default ' ',
  "AAABLIDA" varchar(1) NOT NULL default ' ',
  "AAABLIDB" varchar(1) NOT NULL default ' ',
  "AAABLIDC" varchar(8) NOT NULL default '00000000',
  "AAABLIDD" decimal(3,1) NOT NULL default '0.0',
  "AAABLIDE" decimal(5,0) NOT NULL default '0',
  "AAABLIDF" varchar(3) NOT NULL default ' ',
  "AAABLIDG" decimal(5,0) NOT NULL default '0',
  "AAABLIDH" decimal(5,0) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLUJB" varchar(10) NOT NULL default ' ',
  "AAABLUJC" varchar(5) NOT NULL default '00000',
  "AAABLIDL" decimal(13,3) NOT NULL default '0.000',
  "AAABLIDM" decimal(13,3) NOT NULL default '0.000',
  "AAABLIDN" decimal(13,3) NOT NULL default '0.000',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLIDP" decimal(15,2) NOT NULL default '0.00',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABLIDR" varchar(8) NOT NULL default '00000000',
  "AAABLIDS" varchar(10) NOT NULL default ' ',
  "AAABLIDT" varchar(6) NOT NULL default '000000',
  "AAABLIDU" varchar(6) NOT NULL default '000000',
  "AAABLIDV" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLIDX" varchar(4) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABLIEA" decimal(15,2) NOT NULL default '0.00',
  "AAABLIEB" varchar(10) NOT NULL default ' ',
  "AAABLIEC" varchar(6) NOT NULL default '000000',
  "AAABLIED" varchar(6) NOT NULL default '000000',
  "AAABLIEE" varchar(4) NOT NULL default ' ',
  "AAABLXNY" varchar(18) NOT NULL default ' ',
  "AAABLIEG" VARBINARY(255),
  "AAABLXSC" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSE" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSG" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSH" decimal(13,2) NOT NULL default '0.00',
  "AAABLJEY" decimal(13,2) NOT NULL default '0.00',
  "AAABLJEZ" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFA" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFB" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFC" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXOH" varchar(6) NOT NULL default '000000',
  "AAABLJGL" varchar(1) NOT NULL default ' ',
  "AAABLXSS" decimal(13,3) NOT NULL default '0.000',
  "AAABLXPC" varchar(1) NOT NULL default ' ',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABLXQO" varchar(1) NOT NULL default ' ',
  "AAABLIFB" decimal(13,3) NOT NULL default '0.000',
  "AAABLXNZ" varchar(18) NOT NULL default ' ',
  "AAABLXPA" varchar(3) NOT NULL default ' ',
  "AAABLXOV" varchar(6) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABLIFG" decimal(13,2) NOT NULL default '0.00',
  "AAABLIFH" decimal(13,2) NOT NULL default '0.00',
  "AAABLIFI" varchar(18) NOT NULL default '000000000000000000',
  "AAABLIFJ" varchar(18) NOT NULL default '000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIFR" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLIFT" varchar(1) NOT NULL default ' ',
  "AAABLIFU" varchar(5) NOT NULL default ' ',
  "AAABLIFV" decimal(9,5) NOT NULL default '0.00000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLWVZ" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLZDH" varchar(6) NOT NULL default '000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLIGF" varchar(12) NOT NULL default ' ',
  "AAABLXIT" varchar(20) NOT NULL default ' ',
  "AAABLXIV" varchar(8) NOT NULL default '00000000',
  "AAABLIGI" varchar(5) NOT NULL default ' ',
  "AAABLIGJ" decimal(9,5) NOT NULL default '0.00000',
  "AAABLIGK" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABLIGM" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMCFY" varchar(3) NOT NULL default ' ',
  "AAABLIGP" varchar(6) NOT NULL default ' ',
  "AAABLIGQ" varchar(10) NOT NULL default ' ',
  "AAABLIGR" decimal(5,2) NOT NULL default '0.00',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLXHO" varchar(8) NOT NULL default '00000000',
  "AAABLIGU" varchar(10) NOT NULL default ' ',
  "AAABLIGV" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABLIGX" varchar(8) NOT NULL default '00000000',
  "AAABLIGY" varchar(8) NOT NULL default '00000000',
  "AAABLIGZ" varchar(10) NOT NULL default ' ',
  "AAABLIHA" varchar(2) NOT NULL default ' ',
  "AAABLIHB" varchar(2) NOT NULL default ' ',
  "AAABLIHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLIHF" varchar(1) NOT NULL default ' ',
  "AAABLIHG" varchar(2) NOT NULL default ' ',
  "AAABLIHH" varchar(4) NOT NULL default ' ',
  "AAABLIHI" varchar(4) NOT NULL default ' ',
  "AAABLIHJ" varchar(12) NOT NULL default ' ',
  "AAABLIHK" varchar(35) NOT NULL default ' ',
  "AAABLIHL" varchar(16) NOT NULL default ' ',
  "AAABLIHM" decimal(3,0) NOT NULL default '0',
  "AAABLIHN" varchar(8) NOT NULL default '00000000',
  "AAABLIHO" varchar(1) NOT NULL default ' ',
  "AAABLIHP" varchar(1) NOT NULL default ' ',
  "AAABLIHQ" varchar(1) NOT NULL default ' ',
  "AAABLIHR" varchar(1) NOT NULL default ' ',
  "AAABLIHS" varchar(1) NOT NULL default ' ',
  "AAABLIHT" varchar(1) NOT NULL default ' ',
  "AAABLIHU" varchar(1) NOT NULL default ' ',
  "AAABLIHV" varchar(1) NOT NULL default ' ',
  "AAABLIHW" varchar(1) NOT NULL default ' ',
  "AAABLIHX" varchar(16) NOT NULL default ' ',
  "AAABLIHY" varchar(1) NOT NULL default ' ',
  "AAABLIHZ" varchar(3) NOT NULL default ' ',
  "AAABLIIA" varchar(3) NOT NULL default ' ',
  "AAABLIIB" varchar(3) NOT NULL default ' ',
  "AAABLIIC" varchar(3) NOT NULL default ' ',
  "AAABLIID" varchar(3) NOT NULL default ' ',
  "AAABLIIE" varchar(3) NOT NULL default ' ',
  "AAABLIIF" varchar(3) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLIIH" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLXJR" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(4) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAABLXJV" varchar(5) NOT NULL default ' ',
  "AAABLXJW" varchar(8) NOT NULL default '00000000',
  "AAABLXJX" varchar(8) NOT NULL default '00000000',
  "AAABLXJY" varchar(8) NOT NULL default '00000000',
  "AAABLXJZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLIIT" varchar(2) NOT NULL default ' ',
  "AAABLIYT" varchar(8) NOT NULL default '00000000',
  "AAABLIIV" varchar(1) NOT NULL default ' ',
  "AAABLIIW" decimal(9,5) NOT NULL default '0.00000',
  "AAABLIIX" varchar(1) NOT NULL default ' ',
  "AAABLILV" varchar(30) NOT NULL default ' ',
  "AAABLIIZ" varchar(1) NOT NULL default ' ',
  "AAABLIJA" varchar(10) NOT NULL default ' ',
  "AAABLXLE" varchar(3) NOT NULL default ' ',
  "AAABLXLF" varchar(3) NOT NULL default ' ',
  "AAABMCXT" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLIJF" varchar(1) NOT NULL default ' ',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  "AAABLIJH" varchar(10) NOT NULL default ' ',
  "AAABLIJI" varchar(1) NOT NULL default ' ',
  "AAABLIJJ" varchar(6) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLIMT"),
  KEY "AAABLIJN" ("AAABMRIL","AAABLILV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLUPZ" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLUPQ" varchar(1) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLXWN" varchar(1) NOT NULL default ' ',
  "AAABLXWS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV","AAABMALG","AAABLUPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLLJR" varchar(18) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLIMG" varchar(6) NOT NULL default '000000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLIMK" varchar(4) NOT NULL default ' ',
  "AAABLIMW" varchar(1) NOT NULL default ' ',
  "AAABLIMT" varchar(10) NOT NULL default ' ',
  "AAABLIMU" varchar(6) NOT NULL default '000000',
  "AAABLIMV" varchar(6) NOT NULL default '000000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABLIMN" varchar(3) NOT NULL default ' ',
  "AAABLIMO" varchar(1) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLILF" decimal(15,3) NOT NULL default '0.000',
  "AAABLILG" varchar(3) NOT NULL default ' ',
  "AAABLILH" decimal(15,3) NOT NULL default '0.000',
  "AAABLILI" decimal(15,3) NOT NULL default '0.000',
  "AAABLILJ" decimal(15,3) NOT NULL default '0.000',
  "AAABLILK" varchar(1) NOT NULL default ' ',
  "AAABLILL" varchar(2) NOT NULL default ' ',
  "AAABLILM" varchar(18) NOT NULL default ' ',
  "AAABLILN" varchar(10) NOT NULL default ' ',
  "AAABLILO" varchar(6) NOT NULL default '000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABLILT" varchar(1) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  "AAABLILV" varchar(30) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLILZ" decimal(5,0) NOT NULL default '0',
  "AAABLIMA" decimal(5,0) NOT NULL default '0',
  "AAABLIMB" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABLPNX","AAABLLJR","AAABLIMF","AAABLIMG"),
  KEY "AAABLIMH" ("AAABMRIL","AAABLJBH","AAABLIMK","AAABLIMW","AAABLIMM","AAABLIMN","AAABLIMO","AAABMLCZ","AAABLXOB"),
  KEY "AAABLIMR" ("AAABMRIL","AAABLIMT","AAABLIMU","AAABLIMV","AAABLIMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIPL" varchar(18) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLIQZ" varchar(4) NOT NULL default ' ',
  "AAABLIND" decimal(4,1) NOT NULL default '0.0',
  "AAABLINE" decimal(4,1) NOT NULL default '0.0',
  "AAABLINF" decimal(4,1) NOT NULL default '0.0',
  "AAABLIOR" varchar(8) NOT NULL default '00000000',
  "AAABLIOS" varchar(8) NOT NULL default '00000000',
  "AAABLINI" varchar(80) NOT NULL default ' ',
  "AAABLINJ" varchar(1) NOT NULL default ' ',
  "AAABLINK" varchar(1) NOT NULL default ' ',
  "AAABLINL" varchar(1) NOT NULL default ' ',
  "AAABLIRA" varchar(8) NOT NULL default '00000000',
  "AAABLIRB" varchar(6) NOT NULL default '000000',
  "AAABLINO" varchar(10) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLIPL"),
  KEY "AAABLINT" ("AAABMRIL","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLINW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIPL" varchar(18) NOT NULL default ' ',
  "AAABLIPM" decimal(4,0) NOT NULL default '0',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLIOF" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLLYO" varchar(10) NOT NULL default ' ',
  "AAABLIQP" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQI" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOL" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQH" decimal(4,1) NOT NULL default '0.0',
  "AAABLION" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOO" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOP" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOR" varchar(8) NOT NULL default '00000000',
  "AAABLIOS" varchar(8) NOT NULL default '00000000',
  "AAABLIOT" decimal(11,2) NOT NULL default '0.00',
  "AAABLIOU" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOV" decimal(13,3) NOT NULL default '0.000',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAABLIQU" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABLIPC" decimal(4,0) NOT NULL default '0',
  "AAABLIPD" decimal(13,3) NOT NULL default '0.000',
  "AAABLIPE" varchar(3) NOT NULL default ' ',
  "AAABLIPF" decimal(13,3) NOT NULL default '0.000',
  "AAABLJFZ" varchar(3) NOT NULL default ' ',
  "AAABLUFO" varchar(10) NOT NULL default ' ',
  "AAABLIPI" varchar(5) NOT NULL default '00000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLIPL","AAABLIPM"),
  KEY "AAABLIPN" ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIQU" varchar(1) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLIQA" decimal(11,2) NOT NULL default '0.00',
  "AAABLIQB" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLIQD" decimal(5,2) NOT NULL default '0.00',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLIQH" decimal(4,1) NOT NULL default '0.0',
  "AAABLIQI" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABLIQL" varchar(1) NOT NULL default ' ',
  "AAABLIQM" decimal(4,2) NOT NULL default '0.00',
  "AAABLIQN" decimal(11,2) NOT NULL default '0.00',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAABLIQP" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQQ" varchar(2) NOT NULL default '00',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLIQU","AAABMCVP","AAABMCVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIQX" (
  "AAABLXHU" varchar(10) NOT NULL default ' ',
  "AAABLIQZ" varchar(4) NOT NULL default ' ',
  "AAABLIRA" varchar(8) NOT NULL default '00000000',
  "AAABLIRB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLXHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABLIRP" decimal(4,1) NOT NULL default '0.0',
  "AAABLIRQ" decimal(4,1) NOT NULL default '0.0',
  "AAABLIRR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLURS","AAABLURT","AAABMCVI","AAABLKMM","AAABLKMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIRZ" (
  "AAABLISR" varchar(30) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABMKXF" varchar(1) NOT NULL default ' ',
  "AAABLISD" varchar(1) NOT NULL default ' ',
  "AAABLISE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLISR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLISG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLISR" varchar(30) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLISR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLISM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLISR" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLISR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLISS" (
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLISU" varchar(4) NOT NULL default ' ',
  "AAABLISV" varchar(4) NOT NULL default ' ',
  "AAABLISW" varchar(3) NOT NULL default ' ',
  "AAABLISX" varchar(2) NOT NULL default '00',
  "AAABLISY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLITA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  "AAABLITE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLITH" (
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABLITJ" varchar(1) NOT NULL default ' ',
  "AAABLITK" varchar(1) NOT NULL default ' ',
  "AAABLITL" varchar(1) NOT NULL default ' ',
  "AAABLITM" varchar(20) NOT NULL default ' ',
  "AAABLITN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLITP" (
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLITS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLITY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLUPZ" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLUPQ" varchar(1) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABLXWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHF","AAABMLTD","AAABMALG","AAABLUPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIWB" varchar(10) NOT NULL default ' ',
  "AAABLIVQ" varchar(10) NOT NULL default ' ',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLIVM" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLIVT" varchar(4) NOT NULL default ' ',
  "AAABLIVG" varchar(8) NOT NULL default '00000000',
  "AAABLIVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIWB"),
  KEY "AAABLIVK" ("AAABMRIL","AAABLIVM","AAABLJHF"),
  KEY "AAABLIVO" ("AAABMRIL","AAABLIVQ"),
  KEY "AAABLIVR" ("AAABMRIL","AAABLIVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIWB" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIWB","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIWE" (
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLJJO" varchar(10) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLJJQ" varchar(1) NOT NULL default ' ',
  "AAABLJJN" varchar(30) NOT NULL default ' ',
  "AAABLIWK" varchar(1) NOT NULL default ' ',
  "AAABLJJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABLJJO","AAABLZKF","AAABLJJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJFI" varchar(10) NOT NULL default ' ',
  "AAABLXQD" varchar(6) NOT NULL default '000000',
  "AAABLIXI" varchar(10) NOT NULL default ' ',
  "AAABLIXJ" varchar(6) NOT NULL default '000000',
  "AAABLIXK" varchar(2) NOT NULL default ' ',
  "AAABLJFK" varchar(2) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLIXS" varchar(1) NOT NULL default ' ',
  "AAABLIXC" varchar(1) NOT NULL default ' ',
  "AAABLIXD" varchar(1) NOT NULL default ' ',
  "AAABLIXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJFI","AAABLXQD","AAABLIXI","AAABLIXJ","AAABLIXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLIXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLJDA" varchar(2) NOT NULL default ' ',
  "AAABLIXQ" varchar(4) NOT NULL default ' ',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLIXS" varchar(1) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABLIXV" varchar(6) NOT NULL default ' ',
  "AAABLJBT" varchar(8) NOT NULL default '00000000',
  "AAABLJBU" varchar(1) NOT NULL default ' ',
  "AAABLJCW" varchar(10) NOT NULL default ' ',
  "AAABLJCX" varchar(10) NOT NULL default ' ',
  "AAABLWHH" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABLIYO" varchar(5) NOT NULL default ' ',
  "AAABLIYP" varchar(5) NOT NULL default ' ',
  "AAABLUFL" decimal(9,5) NOT NULL default '0.00000',
  "AAABLXSI" decimal(9,5) NOT NULL default '0.00000',
  "AAABLIYS" varchar(1) NOT NULL default ' ',
  "AAABLIYT" varchar(8) NOT NULL default '00000000',
  "AAABLLYC" decimal(15,2) NOT NULL default '0.00',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMCSY" decimal(3,0) NOT NULL default '0',
  "AAABMCTB" decimal(5,3) NOT NULL default '0.000',
  "AAABMCSZ" decimal(3,0) NOT NULL default '0',
  "AAABMCTC" decimal(5,3) NOT NULL default '0.000',
  "AAABMCTA" decimal(3,0) NOT NULL default '0',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABLJEW" decimal(15,2) NOT NULL default '0.00',
  "AAABLJEX" decimal(15,2) NOT NULL default '0.00',
  "AAABLIZF" varchar(4) NOT NULL default ' ',
  "AAABLIZG" decimal(3,0) NOT NULL default '0',
  "AAABLIZH" decimal(5,3) NOT NULL default '0.000',
  "AAABLIZI" decimal(3,0) NOT NULL default '0',
  "AAABLIZJ" decimal(5,3) NOT NULL default '0.000',
  "AAABLIZK" decimal(3,0) NOT NULL default '0',
  "AAABLIZL" varchar(1) NOT NULL default ' ',
  "AAABLIZM" decimal(13,2) NOT NULL default '0.00',
  "AAABLIZN" decimal(13,2) NOT NULL default '0.00',
  "AAABLIZO" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLXII" varchar(10) NOT NULL default ' ',
  "AAABLIZR" varchar(10) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLIZT" varchar(10) NOT NULL default ' ',
  "AAABLXJM" varchar(10) NOT NULL default ' ',
  "AAABLJHC" varchar(4) NOT NULL default ' ',
  "AAABLJHD" varchar(8) NOT NULL default '00000000',
  "AAABLJHP" varchar(10) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLWYT" varchar(2) NOT NULL default '00',
  "AAABLWYU" varchar(8) NOT NULL default '00000000',
  "AAABLJAB" decimal(13,2) NOT NULL default '0.00',
  "AAABLJAC" decimal(13,2) NOT NULL default '0.00',
  "AAABLJAD" decimal(13,2) NOT NULL default '0.00',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMCUD" varchar(1) NOT NULL default ' ',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABLJFN" varchar(3) NOT NULL default ' ',
  "AAABLJAK" varchar(2) NOT NULL default '00',
  "AAABLJAL" varchar(8) NOT NULL default '00000000',
  "AAABMDAG" varchar(30) NOT NULL default ' ',
  "AAABLJAN" varchar(1) NOT NULL default ' ',
  "AAABLJAO" decimal(9,5) NOT NULL default '0.00000',
  "AAABLJAP" decimal(9,5) NOT NULL default '0.00000',
  "AAABLJAQ" varchar(5) NOT NULL default ' ',
  "AAABLJAR" varchar(5) NOT NULL default ' ',
  "AAABLJAS" varchar(4) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABLJAW" varchar(10) NOT NULL default ' ',
  "AAABLJAX" varchar(1) NOT NULL default ' ',
  "AAABLJAY" varchar(1) NOT NULL default ' ',
  "AAABLJAZ" varchar(10) NOT NULL default ' ',
  "AAABLJBA" varchar(1) NOT NULL default ' ',
  "AAABLJBB" varchar(3) NOT NULL default ' ',
  "AAABLJBC" varchar(1) NOT NULL default ' ',
  "AAABLJBD" varchar(1) NOT NULL default ' ',
  "AAABLJBE" varchar(5) NOT NULL default ' ',
  "AAABLJBF" decimal(11,2) NOT NULL default '0.00',
  "AAABLJBG" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLJBJ" varchar(10) NOT NULL default ' ',
  "AAABLJBK" varchar(4) NOT NULL default ' ',
  "AAABLJBL" varchar(4) NOT NULL default ' ',
  "AAABLJBM" varchar(4) NOT NULL default ' ',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  "AAABLJHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHF"),
  KEY "AAABLJBR" ("AAABMRIL","AAABLJBT","AAABLJBU"),
  KEY "AAABLJBV" ("AAABMRIL","AAABLJCW"),
  KEY "AAABLJBY" ("AAABMRIL","AAABLURS","AAABLURA"),
  KEY "AAABLJCC" ("AAABMRIL","AAABLWHI"),
  KEY "AAABLJCF" ("AAABMRIL","AAABMQII"),
  KEY "AAABLJCI" ("AAABMRIL","AAABMREX","AAABMDGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLJCQ" varchar(10) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABLXVO" decimal(13,2) NOT NULL default '0.00',
  "AAABLJGC" decimal(13,2) NOT NULL default '0.00',
  "AAABLJCU" decimal(13,2) NOT NULL default '0.00',
  "AAABLJCV" decimal(13,2) NOT NULL default '0.00',
  "AAABLJCW" varchar(10) NOT NULL default ' ',
  "AAABLJCX" varchar(10) NOT NULL default ' ',
  "AAABLWHH" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLJDA" varchar(2) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHF","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCTP" varchar(2) NOT NULL default ' ',
  "AAABLJDO" varchar(8) NOT NULL default '00000000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLJDS" varchar(3) NOT NULL default ' ',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABLJDV" varchar(3) NOT NULL default ' ',
  "AAABLJDW" decimal(5,0) NOT NULL default '0',
  "AAABLJDX" decimal(5,0) NOT NULL default '0',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXPY" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(15,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABLLYC" decimal(15,2) NOT NULL default '0.00',
  "AAABLXSC" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSE" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSG" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSH" decimal(13,2) NOT NULL default '0.00',
  "AAABLXDF" decimal(13,2) NOT NULL default '0.00',
  "AAABLJEP" decimal(13,2) NOT NULL default '0.00',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABLXUC" varchar(1) NOT NULL default ' ',
  "AAABLXRM" varchar(1) NOT NULL default ' ',
  "AAABLJEV" decimal(13,2) NOT NULL default '0.00',
  "AAABLJEW" decimal(15,2) NOT NULL default '0.00',
  "AAABLJEX" decimal(15,2) NOT NULL default '0.00',
  "AAABLJEY" decimal(13,2) NOT NULL default '0.00',
  "AAABLJEZ" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFA" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFB" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFC" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFD" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFE" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFF" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFG" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFH" decimal(13,2) NOT NULL default '0.00',
  "AAABLJFI" varchar(10) NOT NULL default ' ',
  "AAABLJFJ" varchar(6) NOT NULL default '000000',
  "AAABLJFK" varchar(2) NOT NULL default ' ',
  "AAABLJFL" varchar(4) NOT NULL default '0000',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLJFN" varchar(3) NOT NULL default ' ',
  "AAABLXOC" varchar(40) NOT NULL default ' ',
  "AAABLLYO" varchar(10) NOT NULL default ' ',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABLXVO" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABLJFT" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABLJFV" varchar(2) NOT NULL default ' ',
  "AAABLJFW" varchar(2) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABLJFY" varchar(4) NOT NULL default ' ',
  "AAABLJFZ" varchar(3) NOT NULL default ' ',
  "AAABLJGA" decimal(13,3) NOT NULL default '0.000',
  "AAABLJGB" varchar(10) NOT NULL default ' ',
  "AAABLJGC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLJGL" varchar(1) NOT NULL default ' ',
  "AAABLJGM" varchar(10) NOT NULL default ' ',
  "AAABLJGN" varchar(6) NOT NULL default '000000',
  "AAABLJGO" varchar(2) NOT NULL default ' ',
  "AAABLJGP" varchar(1) NOT NULL default ' ',
  "AAABLJGQ" varchar(18) NOT NULL default ' ',
  "AAABLOYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJHF","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABLJHQ" varchar(1) NOT NULL default ' ',
  "AAABLJHR" varchar(4) NOT NULL default ' ',
  "AAABLJHP" varchar(10) NOT NULL default ' ',
  "AAABLJHK" varchar(10) NOT NULL default ' ',
  "AAABLJHC" varchar(4) NOT NULL default ' ',
  "AAABLJHD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLJHF","AAABLJHQ","AAABLJHR"),
  KEY "AAABLJHI" ("AAABMRIL","AAABLJHK","AAABLJHQ","AAABLJHR"),
  KEY "AAABLJHN" ("AAABMRIL","AAABLJHP","AAABLJHQ","AAABLJHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJHS" (
  "AAABLJIF" varchar(5) NOT NULL default ' ',
  "AAABLJHU" varchar(5) NOT NULL default ' ',
  "AAABLJHV" varchar(8) NOT NULL default '00000000',
  "AAABLJHW" varchar(6) NOT NULL default '000000',
  "AAABLJHX" varchar(1) NOT NULL default ' ',
  "AAABLJHY" varchar(1) NOT NULL default ' ',
  "AAABLJHZ" varchar(1) NOT NULL default ' ',
  "AAABLJIA" varchar(1) NOT NULL default ' ',
  "AAABLJIB" varchar(1) NOT NULL default ' ',
  "AAABLJIC" varchar(5) NOT NULL default ' ',
  "AAABLJID" varchar(8) NOT NULL default '00000000',
  "AAABLJIE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLJIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJIG" (
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(61) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLJIM","AAABLVCU","AAABLOZE","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJIQ" (
  "AAABLJJN" varchar(30) NOT NULL default ' ',
  "AAABLJJO" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(3) NOT NULL default ' ',
  "AAABLJJQ" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLJIW" varchar(1) NOT NULL default ' ',
  "AAABLJIX" varchar(1) NOT NULL default ' ',
  "AAABLJIY" varchar(1) NOT NULL default ' ',
  "AAABLJIZ" varchar(1) NOT NULL default ' ',
  "AAABLJJA" varchar(1) NOT NULL default ' ',
  "AAABLJJB" int(11) NOT NULL default '0',
  "AAABLJJC" int(11) NOT NULL default '0',
  "AAABLJJD" int(11) NOT NULL default '0',
  "AAABLJJE" int(11) NOT NULL default '0',
  "AAABLJJF" int(11) NOT NULL default '0',
  "AAABLJJG" varchar(1) NOT NULL default ' ',
  "AAABLJJH" varchar(10) NOT NULL default ' ',
  "AAABLJJI" int(11) NOT NULL default '0',
  "AAABLJJJ" varchar(1) NOT NULL default ' ',
  "AAABLJJK" varchar(30) NOT NULL default ' ',
  "AAABLJJL" varchar(40) NOT NULL default ' ',
  "AAABLJJM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLJJN","AAABLJJO","AAABLPJD","AAABLJJQ","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJJS" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(180) NOT NULL default ' ',
  "AAABLJKC" varchar(10) NOT NULL default '0000000000',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAABLJJY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABMOHM","AAABMLPV","AAABLJKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJKD" (
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJNA" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLJNB" varchar(1) NOT NULL default ' ',
  "AAABLLIB" varchar(3) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABLKLJ" varchar(1) NOT NULL default ' ',
  "AAABLKLK" varchar(1) NOT NULL default ' ',
  "AAABLKLL" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLO" varchar(2) NOT NULL default ' ',
  "AAABLKLP" varchar(8) NOT NULL default '00000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLKLR" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLT" varchar(6) NOT NULL default '000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  "AAABLKSB" varchar(2) NOT NULL default ' ',
  "AAABLKLW" decimal(5,0) NOT NULL default '0',
  "AAABLKLX" varchar(3) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLKMB" varchar(1) NOT NULL default ' ',
  "AAABLKSS" varchar(3) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLKMJ" varchar(8) NOT NULL default '00000000',
  "AAABLKMK" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABLKMQ" varchar(8) NOT NULL default '00000000',
  "AAABLKMR" varchar(6) NOT NULL default '000000',
  "AAABLKMS" varchar(8) NOT NULL default '00000000',
  "AAABLKMT" varchar(6) NOT NULL default '000000',
  "AAABLKMU" varchar(8) NOT NULL default '00000000',
  "AAABLKMV" varchar(6) NOT NULL default '000000',
  "AAABLKMW" varchar(8) NOT NULL default '00000000',
  "AAABLKMX" varchar(6) NOT NULL default '000000',
  "AAABLKMY" varchar(8) NOT NULL default '00000000',
  "AAABLKMZ" varchar(6) NOT NULL default '000000',
  "AAABLKNA" varchar(8) NOT NULL default '00000000',
  "AAABLKNB" varchar(6) NOT NULL default '000000',
  "AAABLKNC" varchar(8) NOT NULL default '00000000',
  "AAABLKND" varchar(6) NOT NULL default '000000',
  "AAABLKNE" varchar(8) NOT NULL default '00000000',
  "AAABLKNF" varchar(6) NOT NULL default '000000',
  "AAABLKNG" varchar(8) NOT NULL default '00000000',
  "AAABLKNH" varchar(6) NOT NULL default '000000',
  "AAABLKNI" varchar(8) NOT NULL default '00000000',
  "AAABLKNJ" varchar(6) NOT NULL default '000000',
  "AAABLKRK" varchar(20) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLJMT" varchar(8) NOT NULL default ' ',
  "AAABLKNN" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJNA"),
  KEY "AAABLJMX" ("AAABLXTG"),
  KEY "AAABLJMZ" ("AAABLJNA","AAABLJNB","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(12) NOT NULL default ' ',
  "AAABLJNI" varchar(1) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKUI" varchar(12) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKUI","AAABLKKD","AAABLKKE"),
  KEY "AAABLJOA" ("AAABMRIL","AAABLLIM","AAABLKKD","AAABLKKE","AAABLKUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKUI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKTG","AAABLKPA","AAABLKUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLKJW" varchar(20) NOT NULL default ' ',
  "AAABLJOW" varchar(1) NOT NULL default ' ',
  "AAABLJOX" varchar(8) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLJPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG","AAABLKPA","AAABLKTH","AAABLXTG"),
  KEY "AAABLJQA" ("AAABLXTG"),
  KEY "AAABLJQC" ("AAABLKKD","AAABLKKE","AAABLKTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJQ" varchar(6) NOT NULL default '000000',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKID" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKII" varchar(3) NOT NULL default '000',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLJVY" varchar(5) NOT NULL default ' ',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABLKIO" varchar(5) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKIR" varchar(12) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKIT" varchar(12) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKJA" varchar(3) NOT NULL default '000',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAABLJRT" ("AAABLXTG","AAABLKTG","AAABLKKD","AAABLKKE"),
  KEY "AAABLJRY" ("AAABLKTG","AAABLKKD","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG"),
  KEY "AAABLJSV" ("AAABLKKD","AAABLKKE","AAABLKTG","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLLEL" varchar(1) NOT NULL default ' ',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLJTN" varchar(4) NOT NULL default ' ',
  "AAABLJTO" varchar(12) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLKWH","AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG","AAABLKJB","AAABLKJC","AAABLXTG"),
  KEY "AAABLJUB" ("AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG","AAABLKJB","AAABLKJC","AAABLLEL"),
  KEY "AAABLJUJ" ("AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG","AAABLLEL"),
  KEY "AAABLJUP" ("AAABLKWH","AAABLXTG"),
  KEY "AAABLJUS" ("AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLKKD","AAABLKKE","AAABLKJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKWV" varchar(8) NOT NULL default '00000000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJQ" varchar(6) NOT NULL default '000000',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKID" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKII" varchar(3) NOT NULL default '000',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLJVY" varchar(5) NOT NULL default ' ',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABLKIO" varchar(5) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKIR" varchar(12) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKIT" varchar(12) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKJA" varchar(3) NOT NULL default '000',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLKWV"),
  KEY "AAABLJWW" ("AAABMRIL","AAABLKJO","AAABLKJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLJXH" varchar(22) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKKD","AAABLKKE","AAABLKTG","AAABLXTG"),
  KEY "AAABLJXR" ("AAABLKKD","AAABLKKE","AAABLKTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMLPO"),
  KEY "AAABLJYI" ("AAABLLEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLJZE" varchar(6) NOT NULL default '000000',
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
  "AAABLJZA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMLPO","AAABLJZE"),
  KEY "AAABLJZF" ("AAABLXTG","AAABMLPO","AAABLJZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLJZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKCF" varchar(1) NOT NULL default ' ',
  "AAABLLEY" varchar(3) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABLJZS" varchar(1) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKLK" varchar(1) NOT NULL default ' ',
  "AAABLKLL" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLO" varchar(2) NOT NULL default ' ',
  "AAABLKLP" varchar(8) NOT NULL default '00000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLKLR" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLT" varchar(6) NOT NULL default '000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  "AAABLKSB" varchar(2) NOT NULL default ' ',
  "AAABLKLW" decimal(5,0) NOT NULL default '0',
  "AAABLKLX" varchar(3) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLKMB" varchar(1) NOT NULL default ' ',
  "AAABLKSS" varchar(3) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLKMJ" varchar(8) NOT NULL default '00000000',
  "AAABLKMK" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABLKMQ" varchar(8) NOT NULL default '00000000',
  "AAABLKMR" varchar(6) NOT NULL default '000000',
  "AAABLKMS" varchar(8) NOT NULL default '00000000',
  "AAABLKMT" varchar(6) NOT NULL default '000000',
  "AAABLKMU" varchar(8) NOT NULL default '00000000',
  "AAABLKMV" varchar(6) NOT NULL default '000000',
  "AAABLKMW" varchar(8) NOT NULL default '00000000',
  "AAABLKMX" varchar(6) NOT NULL default '000000',
  "AAABLKMY" varchar(8) NOT NULL default '00000000',
  "AAABLKMZ" varchar(6) NOT NULL default '000000',
  "AAABLKNA" varchar(8) NOT NULL default '00000000',
  "AAABLKNB" varchar(6) NOT NULL default '000000',
  "AAABLKNC" varchar(8) NOT NULL default '00000000',
  "AAABLKND" varchar(6) NOT NULL default '000000',
  "AAABLKNE" varchar(8) NOT NULL default '00000000',
  "AAABLKNF" varchar(6) NOT NULL default '000000',
  "AAABLKNG" varchar(8) NOT NULL default '00000000',
  "AAABLKNH" varchar(6) NOT NULL default '000000',
  "AAABLKNI" varchar(8) NOT NULL default '00000000',
  "AAABLKNJ" varchar(6) NOT NULL default '000000',
  "AAABLKRK" varchar(20) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLKBW" varchar(8) NOT NULL default ' ',
  "AAABLKBX" varchar(8) NOT NULL default ' ',
  "AAABLKNN" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKJO"),
  KEY "AAABLKCB" ("AAABLXTG"),
  KEY "AAABLKCD" ("AAABLKJO","AAABLKCF","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJQ" varchar(6) NOT NULL default '000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKHV" varchar(1) NOT NULL default ' ',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKHX" varchar(6) NOT NULL default '000000',
  "AAABLKHY" varchar(6) NOT NULL default '000000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKID" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKIG" varchar(5) NOT NULL default '00000',
  "AAABLKIH" varchar(40) NOT NULL default ' ',
  "AAABLKII" varchar(3) NOT NULL default '000',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLKIL" varchar(5) NOT NULL default '00000',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABLKIO" varchar(5) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKIR" varchar(12) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  "AAABLKIT" varchar(12) NOT NULL default ' ',
  "AAABLKIU" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLKNN" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKJO","AAABLKJQ"),
  KEY "AAABLKDT" ("AAABMRIL","AAABLKKD","AAABLKKE"),
  KEY "AAABLKDX" ("AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKER" varchar(22) NOT NULL default ' ',
  "AAABLKEO" varchar(22) NOT NULL default ' ',
  "AAABLKED" varchar(2) NOT NULL default ' ',
  "AAABLKEP" varchar(1) NOT NULL default ' ',
  "AAABLKEF" varchar(2) NOT NULL default ' ',
  "AAABLKES" varchar(1) NOT NULL default ' ',
  "AAABLKJF" varchar(1) NOT NULL default ' ',
  "AAABLKJG" varchar(1) NOT NULL default ' ',
  "AAABLKJH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKER","AAABLKEO"),
  KEY "AAABLKEN" ("AAABLKEO","AAABLKEP"),
  KEY "AAABLKEQ" ("AAABLKER","AAABLKES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJP" varchar(6) NOT NULL default '000000',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLKJF" varchar(1) NOT NULL default ' ',
  "AAABLKJG" varchar(1) NOT NULL default ' ',
  "AAABLKJH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKJO","AAABLKJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJP" varchar(6) NOT NULL default '000000',
  "AAABLKJQ" varchar(6) NOT NULL default '000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKHV" varchar(1) NOT NULL default ' ',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKHX" varchar(6) NOT NULL default '000000',
  "AAABLKHY" varchar(6) NOT NULL default '000000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKID" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKIG" varchar(5) NOT NULL default '00000',
  "AAABLKIH" varchar(40) NOT NULL default ' ',
  "AAABLKII" varchar(3) NOT NULL default '000',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLKIL" varchar(5) NOT NULL default '00000',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABLKIO" varchar(5) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKIR" varchar(12) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  "AAABLKIT" varchar(12) NOT NULL default ' ',
  "AAABLKIU" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLKNN" varchar(22) NOT NULL default ' ',
  "AAABLKIY" varchar(40) NOT NULL default ' ',
  "AAABLKIZ" varchar(40) NOT NULL default ' ',
  "AAABLKJA" varchar(3) NOT NULL default '000',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  "AAABLKJD" varchar(1) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABLKJF" varchar(1) NOT NULL default ' ',
  "AAABLKJG" varchar(1) NOT NULL default ' ',
  "AAABLKJH" varchar(1) NOT NULL default ' ',
  "AAABLKHG" varchar(1) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default ' ',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLKJO","AAABLKJP","AAABLKJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKJO" varchar(12) NOT NULL default ' ',
  "AAABLKJP" varchar(6) NOT NULL default '000000',
  "AAABLKJQ" varchar(6) NOT NULL default '000000',
  "AAABLKHV" varchar(1) NOT NULL default ' ',
  "AAABLKHW" varchar(6) NOT NULL default '000000',
  "AAABLKHX" varchar(6) NOT NULL default '000000',
  "AAABLKHY" varchar(6) NOT NULL default '000000',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKID" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKIG" varchar(5) NOT NULL default '00000',
  "AAABLKIH" varchar(40) NOT NULL default ' ',
  "AAABLKII" varchar(3) NOT NULL default '000',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLKIL" varchar(5) NOT NULL default '00000',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABLKIO" varchar(5) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKIR" varchar(12) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  "AAABLKIT" varchar(12) NOT NULL default ' ',
  "AAABLKIU" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLKIY" varchar(40) NOT NULL default ' ',
  "AAABLKIZ" varchar(40) NOT NULL default ' ',
  "AAABLKJA" varchar(3) NOT NULL default '000',
  "AAABLKJB" varchar(10) NOT NULL default ' ',
  "AAABLKJC" varchar(3) NOT NULL default ' ',
  "AAABLKJD" varchar(1) NOT NULL default ' ',
  "AAABLKJE" varchar(1) NOT NULL default ' ',
  "AAABLKJF" varchar(1) NOT NULL default ' ',
  "AAABLKJG" varchar(1) NOT NULL default ' ',
  "AAABLKJH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default ' ',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLKJO","AAABLKJP","AAABLKJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLKKD" varchar(40) NOT NULL default ' ',
  "AAABLKKE" varchar(1) NOT NULL default ' ',
  "AAABLKJW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABLKKD","AAABLKKE"),
  KEY "AAABLKKB" ("AAABMRIL","AAABLKKD","AAABLKKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLKWI" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(16) NOT NULL default ' ',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABLKKP" varchar(8) NOT NULL default '00000000',
  "AAABLKKQ" varchar(6) NOT NULL default '000000',
  "AAABLKKR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKWH","AAABLKWI","AAABLXTG","AAABMLPO"),
  KEY "AAABLKKY" ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKNT" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLLHK" varchar(3) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKLH" varchar(1) NOT NULL default ' ',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  "AAABLKLJ" varchar(1) NOT NULL default ' ',
  "AAABLKLK" varchar(1) NOT NULL default ' ',
  "AAABLKLL" varchar(1) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKLO" varchar(2) NOT NULL default ' ',
  "AAABLKLP" varchar(8) NOT NULL default '00000000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLKLR" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLKLT" varchar(6) NOT NULL default '000000',
  "AAABLKLU" varchar(6) NOT NULL default '000000',
  "AAABLKSB" varchar(2) NOT NULL default ' ',
  "AAABLKLW" decimal(5,0) NOT NULL default '0',
  "AAABLKLX" varchar(3) NOT NULL default ' ',
  "AAABLKLY" varchar(1) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABLKMB" varchar(1) NOT NULL default ' ',
  "AAABLKSS" varchar(3) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABLKMJ" varchar(8) NOT NULL default '00000000',
  "AAABLKMK" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABLKMN" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABLKMQ" varchar(8) NOT NULL default '00000000',
  "AAABLKMR" varchar(6) NOT NULL default '000000',
  "AAABLKMS" varchar(8) NOT NULL default '00000000',
  "AAABLKMT" varchar(6) NOT NULL default '000000',
  "AAABLKMU" varchar(8) NOT NULL default '00000000',
  "AAABLKMV" varchar(6) NOT NULL default '000000',
  "AAABLKMW" varchar(8) NOT NULL default '00000000',
  "AAABLKMX" varchar(6) NOT NULL default '000000',
  "AAABLKMY" varchar(8) NOT NULL default '00000000',
  "AAABLKMZ" varchar(6) NOT NULL default '000000',
  "AAABLKNA" varchar(8) NOT NULL default '00000000',
  "AAABLKNB" varchar(6) NOT NULL default '000000',
  "AAABLKNC" varchar(8) NOT NULL default '00000000',
  "AAABLKND" varchar(6) NOT NULL default '000000',
  "AAABLKNE" varchar(8) NOT NULL default '00000000',
  "AAABLKNF" varchar(6) NOT NULL default '000000',
  "AAABLKNG" varchar(8) NOT NULL default '00000000',
  "AAABLKNH" varchar(6) NOT NULL default '000000',
  "AAABLKNI" varchar(8) NOT NULL default '00000000',
  "AAABLKNJ" varchar(6) NOT NULL default '000000',
  "AAABLKRK" varchar(20) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLKNM" varchar(8) NOT NULL default ' ',
  "AAABLKNN" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKNT"),
  KEY "AAABLKNQ" ("AAABLXTG"),
  KEY "AAABLKNS" ("AAABLKNT","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKOB" varchar(10) NOT NULL default ' ',
  "AAABLKOC" varchar(3) NOT NULL default ' ',
  "AAABLKOD" varchar(1) NOT NULL default ' ',
  "AAABLKOE" varchar(1) NOT NULL default ' ',
  "AAABLKOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKTG","AAABLKPA"),
  KEY "AAABLKOK" ("AAABLLIM","AAABLKTG","AAABLKPA","AAABLKOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKPA" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKOY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKPA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKTH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKPY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKTG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKQR" varchar(10) NOT NULL default ' ',
  "AAABLKQS" varchar(3) NOT NULL default ' ',
  "AAABLKQT" varchar(10) NOT NULL default ' ',
  "AAABLKQU" varchar(3) NOT NULL default ' ',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKTG","AAABLKQR","AAABLKQS","AAABLKQT","AAABLKQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKRK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKRK" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKRH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKRK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKSB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKSB" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKRY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKSB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKSS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKSS" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKSP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKSS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKTD" varchar(4) NOT NULL default ' ',
  "AAABLKTG" varchar(1) NOT NULL default ' ',
  "AAABLKTH" varchar(3) NOT NULL default ' ',
  "AAABLKTA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKTD"),
  KEY "AAABLKTE" ("AAABLLIM","AAABLKTG","AAABLKTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKYP" varchar(20) NOT NULL default ' ',
  "AAABLLCZ" varchar(8) NOT NULL default '00000000',
  "AAABLKWV" varchar(8) NOT NULL default '00000000',
  "AAABLKTO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKYP","AAABLLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKUI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKUI" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLKUF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKUI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLKWI" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(16) NOT NULL default ' ',
  "AAABLKXW" varchar(1) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  "AAABLKWC" varchar(1) NOT NULL default '0',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAABLKUZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKWH","AAABLKWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLKWI" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLKWH","AAABLKWI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLKWH" varchar(1) NOT NULL default ' ',
  "AAABLKWI" varchar(4) NOT NULL default ' ',
  "AAABLKXW" varchar(1) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABMEEL" tinyint(3) unsigned NOT NULL default '0',
  "AAABMEEU" varchar(1) NOT NULL default ' ',
  "AAABMEEV" varchar(1) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  "AAABLKWC" varchar(1) NOT NULL default '0',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABLLIM","AAABLKWH","AAABLKWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLLAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLKWV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLKYM" varchar(1) NOT NULL default ' ',
  "AAABLKYN" varchar(1) NOT NULL default ' ',
  "AAABLKZZ" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(2) NOT NULL default ' ',
  "AAABLKXI" varchar(20) NOT NULL default ' ',
  "AAABLKXJ" varchar(20) NOT NULL default ' ',
  "AAABLKXK" varchar(20) NOT NULL default ' ',
  "AAABLKXL" varchar(20) NOT NULL default ' ',
  "AAABLKXM" varchar(2) NOT NULL default ' ',
  "AAABLKXN" varchar(10) NOT NULL default ' ',
  "AAABLKXO" varchar(2) NOT NULL default ' ',
  "AAABLKXP" varchar(2) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABLKXS" varchar(1) NOT NULL default ' ',
  "AAABLKXT" varchar(1) NOT NULL default ' ',
  "AAABLKXU" varchar(9) NOT NULL default ' ',
  "AAABLKXV" varchar(10) NOT NULL default ' ',
  "AAABLKXW" varchar(1) NOT NULL default ' ',
  "AAABLKXX" varchar(3) NOT NULL default ' ',
  "AAABLKXY" varchar(5) NOT NULL default ' ',
  "AAABLKXZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIN","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLGP" varchar(2) NOT NULL default ' ',
  "AAABLLGQ" varchar(1) NOT NULL default ' ',
  "AAABLLGR" varchar(2) NOT NULL default ' ',
  "AAABLLGS" varchar(1) NOT NULL default ' ',
  "AAABLKYL" varchar(1) NOT NULL default ' ',
  "AAABLKYM" varchar(1) NOT NULL default ' ',
  "AAABLKYN" varchar(1) NOT NULL default ' ',
  "AAABLKYO" varchar(8) NOT NULL default '00000000',
  "AAABLKYP" varchar(20) NOT NULL default ' ',
  "AAABLKYQ" varchar(5) NOT NULL default ' ',
  "AAABLKYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLGP","AAABLLGQ","AAABLLGR","AAABLLGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  "AAABLKZF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIN","AAABLLIO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLKZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAABLKZR" tinyint(3) unsigned NOT NULL default '0',
  "AAABLKZS" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(2) NOT NULL default ' ',
  "AAABLLAP" varchar(1) NOT NULL default ' ',
  "AAABLKZV" varchar(1) NOT NULL default ' ',
  "AAABLKZW" varchar(1) NOT NULL default ' ',
  "AAABLLAU" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABLKZZ" varchar(1) NOT NULL default ' ',
  "AAABLLAA" varchar(1) NOT NULL default ' ',
  "AAABLVJJ" varchar(14) NOT NULL default ' ',
  "AAABLLAC" varchar(5) NOT NULL default ' ',
  "AAABLLAD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIN","AAABLLIO","AAABLLEF"),
  KEY "AAABLLAJ" ("AAABLLEF"),
  KEY "AAABLLAL" ("AAABLLIM","AAABLLIN","AAABLLIO","AAABLLAP"),
  KEY "AAABLLAQ" ("AAABLLIM","AAABLLIN","AAABLLIO","AAABLLAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLBE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLBG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLBL" varchar(1) NOT NULL default ' ',
  "AAABLLBM" varchar(1) NOT NULL default ' ',
  "AAABLLBN" int(11) NOT NULL default '0',
  "AAABLLBO" int(11) NOT NULL default '0',
  "AAABLLBP" int(11) NOT NULL default '0',
  "AAABLLBQ" varchar(1) NOT NULL default ' ',
  "AAABLLBR" varchar(1) NOT NULL default ' ',
  "AAABLLBS" varchar(1) NOT NULL default ' ',
  "AAABLLBT" varchar(1) NOT NULL default ' ',
  "AAABLLBU" varchar(1) NOT NULL default ' ',
  "AAABLLBV" varchar(1) NOT NULL default ' ',
  "AAABLLBW" varchar(40) NOT NULL default ' ',
  "AAABLLBX" varchar(40) NOT NULL default ' ',
  "AAABLLBY" varchar(40) NOT NULL default ' ',
  "AAABLLBZ" varchar(40) NOT NULL default ' ',
  "AAABLLCA" varchar(1) NOT NULL default ' ',
  "AAABLLCB" varchar(1) NOT NULL default ' ',
  "AAABLLCC" varchar(1) NOT NULL default ' ',
  "AAABLLCD" varchar(1) NOT NULL default ' ',
  "AAABLLCE" varchar(1) NOT NULL default ' ',
  "AAABLLCF" varchar(1) NOT NULL default ' ',
  "AAABLLCG" varchar(1) NOT NULL default ' ',
  "AAABLLCH" varchar(1) NOT NULL default ' ',
  "AAABLLCI" varchar(1) NOT NULL default ' ',
  "AAABLLCJ" varchar(1) NOT NULL default ' ',
  "AAABLLCK" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLCZ" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLCX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLCZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLDG" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLDG","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLDN" varchar(5) NOT NULL default ' ',
  "AAABLLDO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLLDN","AAABLLDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLGD" varchar(5) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLGD","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLEY" varchar(3) NOT NULL default ' ',
  "AAABLLEA" varchar(1) NOT NULL default ' ',
  "AAABLLEB" varchar(1) NOT NULL default ' ',
  "AAABLLEC" varchar(1) NOT NULL default ' ',
  "AAABLLED" varchar(1) NOT NULL default ' ',
  "AAABLLEE" varchar(1) NOT NULL default ' ',
  "AAABLLEF" varchar(10) NOT NULL default ' ',
  "AAABLLEG" varchar(10) NOT NULL default ' ',
  "AAABLLEH" varchar(10) NOT NULL default ' ',
  "AAABLLEI" varchar(10) NOT NULL default ' ',
  "AAABLLEJ" varchar(1) NOT NULL default ' ',
  "AAABLLEK" varchar(1) NOT NULL default ' ',
  "AAABLLEL" varchar(1) NOT NULL default ' ',
  "AAABLLEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLEY" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLEV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLEY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLFP" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLFM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLFP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLLGD" varchar(5) NOT NULL default ' ',
  "AAABLLFX" varchar(5) NOT NULL default ' ',
  "AAABLLFY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIN","AAABLLIO","AAABLLGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLGP" varchar(2) NOT NULL default ' ',
  "AAABLLGQ" varchar(1) NOT NULL default ' ',
  "AAABLLGR" varchar(2) NOT NULL default ' ',
  "AAABLLGS" varchar(1) NOT NULL default ' ',
  "AAABLLGT" varchar(5) NOT NULL default ' ',
  "AAABLLGU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLGP","AAABLLGQ","AAABLLGR","AAABLLGS","AAABLLGT","AAABLLGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLHK" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLHH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLHK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIB" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLHY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABLLIO" varchar(1) NOT NULL default ' ',
  "AAABLLIP" varchar(3) NOT NULL default ' ',
  "AAABLLIJ" varchar(1) NOT NULL default ' ',
  "AAABLLIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIM","AAABLLIN","AAABLLIO","AAABLLIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLKP" varchar(10) NOT NULL default ' ',
  "AAABLLIT" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(18) NOT NULL default ' ',
  "AAABLLIV" varchar(255) NOT NULL default ' ',
  "AAABLLIW" varchar(70) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABLLIY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLKP" varchar(10) NOT NULL default ' ',
  "AAABLLJZ" varchar(2) NOT NULL default '00',
  "AAABLLJF" varchar(1) NOT NULL default ' ',
  "AAABLLJG" varchar(3) NOT NULL default '000',
  "AAABLLJH" varchar(2) NOT NULL default '00',
  "AAABLLJI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLKP","AAABLLJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLKP" varchar(10) NOT NULL default ' ',
  "AAABLLJZ" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLLJR" varchar(3) NOT NULL default ' ',
  "AAABLLJS" varchar(255) NOT NULL default ' ',
  "AAABLLJT" varchar(70) NOT NULL default ' ',
  "AAABLLJU" varchar(24) NOT NULL default ' ',
  "AAABLLJV" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLKP","AAABLLJZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLKP" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLKF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLKP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLKP" varchar(10) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLLKR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLKP","AAABLZKF","AAABLLKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLKS" (
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
  PRIMARY KEY ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLLL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLLS" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABLLMC" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLMG" (
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
  UNIQUE KEY "AAABLLMT" ("AAABLLNL"),
  KEY "AAABLLMV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLMX" (
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
  UNIQUE KEY "AAABLLNK" ("AAABLLNL"),
  KEY "AAABLLNM" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLNO" (
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNR","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNV" varchar(1) NOT NULL default ' ',
  "AAABLLNW" varchar(1) NOT NULL default ' ',
  "AAABLMFD" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLLWY" varchar(30) NOT NULL default ' ',
  "AAABLLOD" decimal(15,3) NOT NULL default '0.000',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAABLLOF" decimal(15,3) NOT NULL default '0.000',
  "AAABLLOG" decimal(15,3) NOT NULL default '0.000',
  "AAABLLOH" decimal(15,3) NOT NULL default '0.000',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLLOQ" varchar(6) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  "AAABLOOL" varchar(5) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABLOMS" decimal(5,0) NOT NULL default '0',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDW" decimal(11,2) NOT NULL default '0.00',
  "AAABLONQ" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEA" decimal(11,2) NOT NULL default '0.00',
  "AAABLONO" decimal(11,2) NOT NULL default '0.00',
  "AAABLONI" decimal(11,2) NOT NULL default '0.00',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLLPG" varchar(8) NOT NULL default '00000000',
  "AAABLLPH" varchar(8) NOT NULL default '00000000',
  "AAABLMFH" varchar(8) NOT NULL default '00000000',
  "AAABLMFI" varchar(8) NOT NULL default '00000000',
  "AAABLLPK" varchar(8) NOT NULL default '00000000',
  "AAABLLPL" varchar(8) NOT NULL default '00000000',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLLPO" varchar(1) NOT NULL default ' ',
  "AAABLLPP" varchar(40) NOT NULL default ' ',
  "AAABLLPQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMGI" varchar(10) NOT NULL default ' ',
  "AAABLLWL" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLLWR" int(11) NOT NULL default '0',
  "AAABLRYH" varchar(1) NOT NULL default ' ',
  "AAABLLPZ" varchar(4) NOT NULL default ' ',
  "AAABLLQA" varchar(4) NOT NULL default ' ',
  "AAABLLQB" varchar(2) NOT NULL default ' ',
  "AAABLLQC" varchar(2) NOT NULL default ' ',
  "AAABLLQD" varchar(3) NOT NULL default ' ',
  "AAABLLQE" varchar(10) NOT NULL default ' ',
  "AAABLLQF" varchar(10) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLMDQ" varchar(2) NOT NULL default ' ',
  "AAABLMDR" varchar(2) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLMDY" varchar(1) NOT NULL default ' ',
  "AAABLMDZ" varchar(1) NOT NULL default ' ',
  "AAABLMEA" varchar(1) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAABLMEC" varchar(1) NOT NULL default ' ',
  "AAABLMED" varchar(1) NOT NULL default ' ',
  "AAABLMEE" varchar(40) NOT NULL default ' ',
  "AAABLMEF" varchar(1) NOT NULL default ' ',
  "AAABLMEG" varchar(8) NOT NULL default '00000000',
  "AAABLMEH" varchar(1) NOT NULL default ' ',
  "AAABLMEI" varchar(8) NOT NULL default '00000000',
  "AAABLMEJ" varchar(8) NOT NULL default '00000000',
  "AAABLMEK" varchar(8) NOT NULL default '00000000',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABLMEM" varchar(1) NOT NULL default ' ',
  "AAABLMEN" varchar(1) NOT NULL default ' ',
  "AAABLMEO" varchar(1) NOT NULL default ' ',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLMEQ" varchar(10) NOT NULL default ' ',
  "AAABLUHF" varchar(10) NOT NULL default ' ',
  "AAABLUHG" varchar(5) NOT NULL default '00000',
  "AAABLMET" varchar(10) NOT NULL default ' ',
  "AAABLMEU" varchar(6) NOT NULL default '000000',
  "AAABLMEV" varchar(10) NOT NULL default ' ',
  "AAABLMEW" decimal(13,3) NOT NULL default '0.000',
  "AAABLMEX" varchar(3) NOT NULL default ' ',
  "AAABLMEY" decimal(13,3) NOT NULL default '0.000',
  "AAABLMEZ" varchar(3) NOT NULL default ' ',
  "AAABLMFA" varchar(1) NOT NULL default ' ',
  "AAABLMFB" varchar(1) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABLMFD" varchar(2) NOT NULL default ' ',
  "AAABLMFE" varchar(1) NOT NULL default ' ',
  "AAABLMFF" varchar(1) NOT NULL default ' ',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABLMFH" varchar(8) NOT NULL default '00000000',
  "AAABLMFI" varchar(8) NOT NULL default '00000000',
  "AAABLMFJ" varchar(8) NOT NULL default '00000000',
  "AAABLMFK" varchar(8) NOT NULL default '00000000',
  "AAABLMFL" varchar(8) NOT NULL default '00000000',
  "AAABLMFM" varchar(8) NOT NULL default '00000000',
  "AAABLMFN" varchar(8) NOT NULL default '00000000',
  "AAABLMFO" varchar(8) NOT NULL default '00000000',
  "AAABLMFP" varchar(8) NOT NULL default '00000000',
  "AAABLMFQ" varchar(6) NOT NULL default '000000',
  "AAABLMFR" varchar(6) NOT NULL default '000000',
  "AAABLMFS" smallint(6) NOT NULL default '0',
  "AAABLMFT" decimal(4,2) NOT NULL default '0.00',
  "AAABLMFU" varchar(1) NOT NULL default ' ',
  "AAABLMFV" varchar(1) NOT NULL default ' ',
  "AAABLMFW" varchar(1) NOT NULL default ' ',
  "AAABLMFX" varchar(1) NOT NULL default ' ',
  "AAABLMFY" varchar(1) NOT NULL default ' ',
  "AAABLMFZ" varchar(1) NOT NULL default ' ',
  "AAABLMGA" varchar(1) NOT NULL default ' ',
  "AAABLMGB" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(1) NOT NULL default ' ',
  "AAABLMGD" varchar(1) NOT NULL default ' ',
  "AAABLMGE" varchar(2) NOT NULL default ' ',
  "AAABLQZP" varchar(1) NOT NULL default ' ',
  "AAABLMGG" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLLSZ" varchar(1) NOT NULL default ' ',
  "AAABLLTA" varchar(20) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLLTC" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLLTG" varchar(8) NOT NULL default '00000000',
  "AAABLLTH" varchar(2) NOT NULL default ' ',
  "AAABLLXD" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLLTO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLTQ" varchar(1) NOT NULL default ' ',
  "AAABLLTR" varchar(1) NOT NULL default ' ',
  "AAABLLTS" varchar(1) NOT NULL default ' ',
  "AAABLOLT" varchar(2) NOT NULL default ' ',
  "AAABLOOU" varchar(2) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLLTY" varchar(1) NOT NULL default ' ',
  "AAABLLTZ" varchar(1) NOT NULL default ' ',
  "AAABLLUA" varchar(1) NOT NULL default ' ',
  "AAABLLUB" varchar(1) NOT NULL default ' ',
  "AAABLLUC" varchar(1) NOT NULL default ' ',
  "AAABLLUD" varchar(1) NOT NULL default ' ',
  "AAABLLUE" varchar(1) NOT NULL default ' ',
  "AAABLLUF" varchar(1) NOT NULL default ' ',
  "AAABLLUG" varchar(1) NOT NULL default ' ',
  "AAABLLUH" varchar(1) NOT NULL default ' ',
  "AAABLLUI" varchar(1) NOT NULL default ' ',
  "AAABLLUJ" varchar(1) NOT NULL default ' ',
  "AAABLLUK" varchar(20) NOT NULL default ' ',
  "AAABLLUL" varchar(40) NOT NULL default ' ',
  "AAABLLUM" varchar(40) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLLUR" varchar(8) NOT NULL default '00000000',
  "AAABLLUS" varchar(6) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMGI" varchar(10) NOT NULL default ' ',
  "AAABLMGJ" varchar(5) NOT NULL default '00000',
  "AAABLMGK" varchar(5) NOT NULL default '00000',
  "AAABLMGL" varchar(5) NOT NULL default '00000',
  "AAABLLWE" varchar(4) NOT NULL default '0000',
  "AAABLLWL" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLLWN" varchar(5) NOT NULL default '00000',
  "AAABLLVG" varchar(5) NOT NULL default '00000',
  "AAABLLVH" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLWR" int(11) NOT NULL default '0',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  "AAABLLWT" varchar(5) NOT NULL default '00000',
  "AAABLLWU" varchar(5) NOT NULL default '00000',
  "AAABLLWV" varchar(5) NOT NULL default '00000',
  "AAABLLVQ" varchar(4) NOT NULL default '0000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLLVS" decimal(13,3) NOT NULL default '0.000',
  "AAABLMET" varchar(8) NOT NULL default '00000000',
  "AAABLMEU" varchar(6) NOT NULL default '000000',
  "AAABLLVV" varchar(3) NOT NULL default '000',
  "AAABLLYE" decimal(13,3) NOT NULL default '0.000',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABLMFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMGI","AAABLMGJ","AAABLMGK","AAABLMGL","AAABLLWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLLWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMGI" varchar(10) NOT NULL default ' ',
  "AAABLMGJ" varchar(5) NOT NULL default '00000',
  "AAABLMGK" varchar(5) NOT NULL default '00000',
  "AAABLMGL" varchar(5) NOT NULL default '00000',
  "AAABLLWL" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLLWN" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLLWR" int(11) NOT NULL default '0',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  "AAABLLWT" varchar(5) NOT NULL default '00000',
  "AAABLLWU" varchar(5) NOT NULL default '00000',
  "AAABLLWV" varchar(5) NOT NULL default '00000',
  "AAABLLWW" varchar(1) NOT NULL default ' ',
  "AAABLLWX" varchar(10) NOT NULL default ' ',
  "AAABLLWY" varchar(30) NOT NULL default ' ',
  "AAABLLWZ" varchar(25) NOT NULL default ' ',
  "AAABLLXA" varchar(4) NOT NULL default ' ',
  "AAABLLXB" varchar(30) NOT NULL default ' ',
  "AAABLLXC" varchar(25) NOT NULL default ' ',
  "AAABLLXD" varchar(10) NOT NULL default ' ',
  "AAABLLXE" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLLXN" varchar(5) NOT NULL default '00000',
  "AAABLLXO" varchar(1) NOT NULL default ' ',
  "AAABLLXP" varchar(1) NOT NULL default ' ',
  "AAABLLXQ" decimal(13,3) NOT NULL default '0.000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABLLXS" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVO" varchar(3) NOT NULL default ' ',
  "AAABLLXV" decimal(5,0) NOT NULL default '0',
  "AAABLLXW" decimal(5,0) NOT NULL default '0',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLXPL" decimal(13,2) NOT NULL default '0.00',
  "AAABLLYC" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLLYE" decimal(13,3) NOT NULL default '0.000',
  "AAABLLYF" varchar(3) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLUHX" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLLYO" varchar(10) NOT NULL default ' ',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABLLYQ" varchar(8) NOT NULL default '00000000',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABLLYV" varchar(1) NOT NULL default ' ',
  "AAABLLYW" varchar(1) NOT NULL default ' ',
  "AAABLLYX" varchar(1) NOT NULL default ' ',
  "AAABLLYY" decimal(3,0) NOT NULL default '0',
  "AAABLLYZ" decimal(3,0) NOT NULL default '0',
  "AAABLLZA" decimal(3,0) NOT NULL default '0',
  "AAABLLZB" decimal(3,0) NOT NULL default '0',
  "AAABLXPE" decimal(3,1) NOT NULL default '0.0',
  "AAABLXPD" varchar(1) NOT NULL default ' ',
  "AAABLXPF" decimal(3,1) NOT NULL default '0.0',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLLZH" varchar(1) NOT NULL default ' ',
  "AAABLLZI" varchar(3) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABLLZK" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLLZO" decimal(13,3) NOT NULL default '0.000',
  "AAABLLZP" decimal(13,3) NOT NULL default '0.000',
  "AAABLLZQ" varchar(32) NOT NULL default ' ',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDW" decimal(11,2) NOT NULL default '0.00',
  "AAABLLZT" decimal(11,2) NOT NULL default '0.00',
  "AAABLLZU" decimal(11,2) NOT NULL default '0.00',
  "AAABLONQ" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEA" decimal(11,2) NOT NULL default '0.00',
  "AAABLONP" decimal(11,2) NOT NULL default '0.00',
  "AAABLONO" decimal(11,2) NOT NULL default '0.00',
  "AAABLONI" decimal(11,2) NOT NULL default '0.00',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  "AAABLOOL" varchar(5) NOT NULL default ' ',
  "AAABLMAE" varchar(5) NOT NULL default ' ',
  "AAABLMAF" decimal(13,3) NOT NULL default '0.000',
  "AAABLMAG" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAH" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAI" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAJ" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAK" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAL" decimal(13,3) NOT NULL default '0.000',
  "AAABLMAM" decimal(13,3) NOT NULL default '0.000',
  "AAABLMAN" decimal(15,3) NOT NULL default '0.000',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAABLMAP" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAQ" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAR" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAS" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAT" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAU" decimal(13,3) NOT NULL default '0.000',
  "AAABLMAV" decimal(13,3) NOT NULL default '0.000',
  "AAABLMAW" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAX" varchar(3) NOT NULL default ' ',
  "AAABLMAY" decimal(15,3) NOT NULL default '0.000',
  "AAABLMAZ" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBA" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBB" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBC" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBD" varchar(3) NOT NULL default ' ',
  "AAABLMBE" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBF" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBG" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBH" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBI" decimal(15,3) NOT NULL default '0.000',
  "AAABLMBJ" varchar(3) NOT NULL default ' ',
  "AAABLMBK" decimal(11,2) NOT NULL default '0.00',
  "AAABLMBL" decimal(11,2) NOT NULL default '0.00',
  "AAABLMBM" decimal(11,2) NOT NULL default '0.00',
  "AAABLMBN" varchar(5) NOT NULL default ' ',
  "AAABLMBO" varchar(5) NOT NULL default ' ',
  "AAABLMBP" varchar(5) NOT NULL default ' ',
  "AAABLMBQ" varchar(8) NOT NULL default ' ',
  "AAABLMBR" varchar(16) NOT NULL default ' ',
  "AAABLMBS" varchar(32) NOT NULL default ' ',
  "AAABLMBT" varchar(8) NOT NULL default ' ',
  "AAABLMBU" varchar(16) NOT NULL default ' ',
  "AAABLMBV" varchar(32) NOT NULL default ' ',
  "AAABLMBW" varchar(8) NOT NULL default ' ',
  "AAABLMBX" varchar(8) NOT NULL default ' ',
  "AAABLMBY" varchar(8) NOT NULL default ' ',
  "AAABLMBZ" varchar(8) NOT NULL default ' ',
  "AAABLMCA" varchar(8) NOT NULL default ' ',
  "AAABLMCB" varchar(1) NOT NULL default ' ',
  "AAABLMCC" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCD" varchar(3) NOT NULL default '000',
  "AAABLMCE" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCF" varchar(3) NOT NULL default ' ',
  "AAABLMCG" varchar(3) NOT NULL default ' ',
  "AAABLMCH" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCI" varchar(8) NOT NULL default '00000000',
  "AAABLMCJ" varchar(6) NOT NULL default '000000',
  "AAABLMCK" varchar(3) NOT NULL default '000',
  "AAABLMCL" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCM" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCN" varchar(8) NOT NULL default '00000000',
  "AAABLMCO" varchar(6) NOT NULL default '000000',
  "AAABLMCP" varchar(3) NOT NULL default '000',
  "AAABLMCQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCR" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCS" varchar(8) NOT NULL default '00000000',
  "AAABLMCT" varchar(6) NOT NULL default '000000',
  "AAABLMCU" varchar(3) NOT NULL default '000',
  "AAABLMCV" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCW" decimal(13,3) NOT NULL default '0.000',
  "AAABLMCX" varchar(8) NOT NULL default '00000000',
  "AAABLMCY" varchar(6) NOT NULL default '000000',
  "AAABLMCZ" varchar(3) NOT NULL default '000',
  "AAABLMDA" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDB" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDC" varchar(8) NOT NULL default '00000000',
  "AAABLMDD" varchar(6) NOT NULL default '000000',
  "AAABLMDE" varchar(3) NOT NULL default '000',
  "AAABLMDF" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDG" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDH" varchar(8) NOT NULL default '00000000',
  "AAABLMDI" varchar(6) NOT NULL default '000000',
  "AAABLMDJ" varchar(3) NOT NULL default '000',
  "AAABLMDK" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDL" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDM" varchar(8) NOT NULL default '00000000',
  "AAABLMDN" varchar(6) NOT NULL default '000000',
  "AAABLMDO" varchar(3) NOT NULL default '000',
  "AAABLMDP" decimal(13,3) NOT NULL default '0.000',
  "AAABLMDQ" varchar(2) NOT NULL default ' ',
  "AAABLMDR" varchar(2) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLMDT" varchar(4) NOT NULL default ' ',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAABLMDV" varchar(4) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLMDY" varchar(1) NOT NULL default ' ',
  "AAABLMDZ" varchar(1) NOT NULL default ' ',
  "AAABLMEA" varchar(1) NOT NULL default ' ',
  "AAABLMEB" varchar(4) NOT NULL default ' ',
  "AAABLMEC" varchar(1) NOT NULL default ' ',
  "AAABLMED" varchar(1) NOT NULL default ' ',
  "AAABLMEE" varchar(40) NOT NULL default ' ',
  "AAABLMEF" varchar(1) NOT NULL default ' ',
  "AAABLMEG" varchar(8) NOT NULL default '00000000',
  "AAABLMEH" varchar(1) NOT NULL default ' ',
  "AAABLMEI" varchar(8) NOT NULL default '00000000',
  "AAABLMEJ" varchar(8) NOT NULL default '00000000',
  "AAABLMEK" varchar(8) NOT NULL default '00000000',
  "AAABLMEL" varchar(8) NOT NULL default '00000000',
  "AAABLMEM" varchar(1) NOT NULL default ' ',
  "AAABLMEN" varchar(1) NOT NULL default ' ',
  "AAABLMEO" varchar(1) NOT NULL default ' ',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLMEQ" varchar(10) NOT NULL default ' ',
  "AAABLUHF" varchar(10) NOT NULL default ' ',
  "AAABLUHG" varchar(5) NOT NULL default '00000',
  "AAABLMET" varchar(10) NOT NULL default ' ',
  "AAABLMEU" varchar(6) NOT NULL default '000000',
  "AAABLMEV" varchar(10) NOT NULL default ' ',
  "AAABLMEW" decimal(13,3) NOT NULL default '0.000',
  "AAABLMEX" varchar(3) NOT NULL default ' ',
  "AAABLMEY" decimal(13,3) NOT NULL default '0.000',
  "AAABLMEZ" varchar(3) NOT NULL default ' ',
  "AAABLMFA" varchar(1) NOT NULL default ' ',
  "AAABLMFB" varchar(1) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABLMFD" varchar(2) NOT NULL default ' ',
  "AAABLMFE" varchar(1) NOT NULL default ' ',
  "AAABLMFF" varchar(1) NOT NULL default ' ',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABLMFH" varchar(8) NOT NULL default '00000000',
  "AAABLMFI" varchar(8) NOT NULL default '00000000',
  "AAABLMFJ" varchar(8) NOT NULL default '00000000',
  "AAABLMFK" varchar(8) NOT NULL default '00000000',
  "AAABLMFL" varchar(8) NOT NULL default '00000000',
  "AAABLMFM" varchar(8) NOT NULL default '00000000',
  "AAABLMFN" varchar(8) NOT NULL default '00000000',
  "AAABLMFO" varchar(8) NOT NULL default '00000000',
  "AAABLMFP" varchar(8) NOT NULL default '00000000',
  "AAABLMFQ" varchar(6) NOT NULL default '000000',
  "AAABLMFR" varchar(6) NOT NULL default '000000',
  "AAABLMFS" smallint(6) NOT NULL default '0',
  "AAABLMFT" decimal(4,2) NOT NULL default '0.00',
  "AAABLMFU" varchar(1) NOT NULL default ' ',
  "AAABLMFV" varchar(1) NOT NULL default ' ',
  "AAABLMFW" varchar(1) NOT NULL default ' ',
  "AAABLMFX" varchar(1) NOT NULL default ' ',
  "AAABLMFY" varchar(1) NOT NULL default ' ',
  "AAABLMFZ" varchar(1) NOT NULL default ' ',
  "AAABLMGA" varchar(1) NOT NULL default ' ',
  "AAABLMGB" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(1) NOT NULL default ' ',
  "AAABLMGD" varchar(1) NOT NULL default ' ',
  "AAABLMGE" varchar(2) NOT NULL default ' ',
  "AAABLQZP" varchar(1) NOT NULL default ' ',
  "AAABLMGG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMGI","AAABLMGJ","AAABLMGK","AAABLMGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHF" varchar(16) NOT NULL default '0000000000000000',
  "AAABLMHA" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLMGR" varchar(14) NOT NULL default ' ',
  "AAABLMHC" varchar(14) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHF" varchar(16) NOT NULL default '0000000000000000',
  "AAABLMHG" varchar(10) NOT NULL default ' ',
  "AAABLMHA" varchar(10) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLMHC" varchar(14) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHF","AAABLMHG"),
  KEY "AAABLMHH" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMJR" varchar(14) NOT NULL default '00000000000000',
  "AAABLQDA" varchar(8) NOT NULL default '00000000',
  "AAABLMJK" varchar(6) NOT NULL default '000000',
  "AAABLMIF" varchar(10) NOT NULL default ' ',
  "AAABLMIG" varchar(3) NOT NULL default ' ',
  "AAABLMHQ" varchar(8) NOT NULL default '00000000',
  "AAABLMHR" varchar(8) NOT NULL default '00000000',
  "AAABLMHS" varchar(6) NOT NULL default '000000',
  "AAABLMHT" varchar(6) NOT NULL default '000000',
  "AAABLMIH" varchar(1) NOT NULL default ' ',
  "AAABLMHV" int(11) NOT NULL default '0',
  "AAABLMHW" varchar(6) NOT NULL default '000000',
  "AAABLMJH" varchar(1) NOT NULL default ' ',
  "AAABLMJI" varchar(2) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLMIK" varchar(1) NOT NULL default ' ',
  "AAABLMIB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMJR"),
  KEY "AAABLMIE" ("AAABLMIF","AAABLMIG","AAABLMIH","AAABLMJI","AAABLMJH","AAABLMIK","AAABLQDA","AAABLMJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMJR" varchar(14) NOT NULL default '00000000000000',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABLXYU" varchar(40) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMJR","AAABMRCU","AAABLXYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMJR" varchar(14) NOT NULL default '00000000000000',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABLQCN" varchar(8) NOT NULL default ' ',
  "AAABLXQO" varchar(40) NOT NULL default ' ',
  "AAABLMJE" varchar(40) NOT NULL default ' ',
  "AAABLMJF" varchar(6) NOT NULL default '000000',
  "AAABLMJG" varchar(6) NOT NULL default '000000',
  "AAABLMJH" varchar(1) NOT NULL default ' ',
  "AAABLMJI" varchar(2) NOT NULL default ' ',
  "AAABLQDA" varchar(8) NOT NULL default '00000000',
  "AAABLMJK" varchar(6) NOT NULL default '000000',
  "AAABLMJL" varchar(1) NOT NULL default ' ',
  "AAABLMJM" varchar(1) NOT NULL default ' ',
  "AAABLMJN" varchar(6) NOT NULL default '000000',
  "AAABLMJO" varchar(10) NOT NULL default ' ',
  "AAABLMJP" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMJR","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMJT" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABLMJX" longtext,
  PRIMARY KEY ("AAABLZKF","AAABLYZH","AAABLNYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABLMKJ" varchar(1) NOT NULL default ' ',
  "AAABLMKK" varchar(1) NOT NULL default '0',
  "AAABLMKL" varchar(1) NOT NULL default ' ',
  "AAABLMKM" varchar(1) NOT NULL default '0',
  "AAABLMKN" varchar(3) NOT NULL default '000',
  "AAABLMKO" varchar(1) NOT NULL default ' ',
  "AAABLMKP" varchar(1) NOT NULL default ' ',
  "AAABLMKQ" varchar(1) NOT NULL default ' ',
  "AAABLMKR" varchar(1) NOT NULL default ' ',
  "AAABLMKS" varchar(1) NOT NULL default ' ',
  "AAABLMKT" varchar(1) NOT NULL default ' ',
  "AAABLMKU" varchar(1) NOT NULL default ' ',
  "AAABLMKV" varchar(1) NOT NULL default ' ',
  "AAABLMKW" varchar(1) NOT NULL default ' ',
  "AAABLMKX" varchar(1) NOT NULL default ' ',
  "AAABLMKY" varchar(1) NOT NULL default ' ',
  "AAABLMKZ" varchar(3) NOT NULL default '000',
  "AAABLMLA" varchar(1) NOT NULL default ' ',
  "AAABLMLB" varchar(5) NOT NULL default '00000',
  "AAABLMLC" longtext,
  PRIMARY KEY ("AAABMRIL","AAABLUFN","AAABLXOB","AAABMLCZ","AAABLMLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMLI" (
  "AAABLMLP" varchar(30) NOT NULL default ' ',
  "AAABLMLR" int(11) NOT NULL default '0',
  "AAABLMLL" varchar(30) NOT NULL default ' ',
  "AAABLMLM" varchar(30) NOT NULL default ' ',
  "AAABLMLN" varchar(30) NOT NULL default ' ',
  "AAABLUXQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMLP"),
  UNIQUE KEY "AAABLMLQ" ("AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMLS" (
  "AAABLMMC" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMMC","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMLY" (
  "AAABLMMC" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMMC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMME" (
  "AAABLMMP" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMMH" varchar(30) NOT NULL default ' ',
  "AAABLMMI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMMP","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMML" (
  "AAABLMMP" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMMP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMMR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMNA" (
  "AAABLMNP" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLMND" varchar(30) NOT NULL default ' ',
  "AAABLMNE" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLMNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMNL" (
  "AAABLMNP" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMNR" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABLMNV" varchar(30) NOT NULL default ' ',
  "AAABLMNW" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLNSJ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMOG" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMOM" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLMOS" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNMD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLMOS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMOU" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMOY" varchar(2) NOT NULL default '00',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLMPB" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMPJ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMPR" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMPW" varchar(30) NOT NULL default ' ',
  "AAABLMPX" int(11) NOT NULL default '0',
  "AAABLNRX" varchar(1) NOT NULL default '0',
  "AAABLMPZ" varchar(30) NOT NULL default ' ',
  "AAABLMQA" varchar(30) NOT NULL default ' ',
  "AAABLMQB" varchar(30) NOT NULL default ' ',
  "AAABLMQC" varchar(30) NOT NULL default ' ',
  "AAABLMQD" varchar(30) NOT NULL default ' ',
  "AAABLMQE" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLMQG" longtext,
  "AAABLMQH" varchar(1) NOT NULL default '0',
  "AAABLMQI" varchar(30) NOT NULL default ' ',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLMTJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMSB","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMQQ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMSB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMRA" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMZS" int(11) NOT NULL default '0',
  "AAABLMZT" varchar(1) NOT NULL default '0',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLMZW" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMSB","AAABLNEE","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMRR" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABMLQA","AAABLMSB","AAABLNEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMSD" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABLNLM" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMSQ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMUR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNMH" varchar(30) NOT NULL default ' ',
  "AAABLMSW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMUR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMTB" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMVO" varchar(30) NOT NULL default ' ',
  "AAABLMUD" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMTH" varchar(30) NOT NULL default ' ',
  "AAABLMTI" int(11) NOT NULL default '0',
  "AAABLMTJ" varchar(1) NOT NULL default ' ',
  "AAABLNNZ" varchar(255) NOT NULL default ' ',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMVO","AAABLMUD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMTT" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMVX" varchar(30) NOT NULL default ' ',
  "AAABLMVO" varchar(30) NOT NULL default ' ',
  "AAABLMUD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMVX","AAABLMVO","AAABLMUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMUE" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMUR" varchar(30) NOT NULL default ' ',
  "AAABLMUS" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMUK" varchar(2) NOT NULL default '00',
  "AAABLMUL" varchar(30) NOT NULL default ' ',
  "AAABLMUM" varchar(30) NOT NULL default ' ',
  "AAABLMUN" varchar(30) NOT NULL default ' ',
  "AAABLMUO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMUR","AAABLMUS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMUU" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMVO" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMUZ" varchar(30) NOT NULL default ' ',
  "AAABLMVA" varchar(30) NOT NULL default ' ',
  "AAABLMVB" int(11) NOT NULL default '0',
  "AAABLMVC" varchar(30) NOT NULL default ' ',
  "AAABLMVD" varchar(2) NOT NULL default '00',
  "AAABLNAW" varchar(2) NOT NULL default '00',
  "AAABLMVF" varchar(2) NOT NULL default '00',
  "AAABLMVG" varchar(1) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLMVI" varchar(30) NOT NULL default ' ',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLMVL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMVO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMVQ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLMVX" varchar(30) NOT NULL default ' ',
  "AAABLMVU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB","AAABLMVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMVY" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLMWD" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLMWD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMWF" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNAN" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNAN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMWP" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNAN" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMWU" varchar(30) NOT NULL default ' ',
  "AAABLMWV" varchar(1) NOT NULL default '0',
  "AAABLMWW" varchar(1) NOT NULL default '0',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNAN","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMXC" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNRO","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMXJ" (
  "AAABLMYH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMXM" (
  "AAABLMYH" varchar(30) NOT NULL default ' ',
  "AAABLMXR" varchar(30) NOT NULL default ' ',
  "AAABLMXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMYH","AAABLMXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMXS" (
  "AAABLMYH" varchar(30) NOT NULL default ' ',
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAABLMXV" varchar(30) NOT NULL default ' ',
  "AAABLMXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMYH","AAABLMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMXZ" (
  "AAABLMYH" varchar(30) NOT NULL default ' ',
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAABLMYD" varchar(1) NOT NULL default ' ',
  "AAABLNAW" varchar(2) NOT NULL default '00',
  "AAABLMYF" varchar(1) NOT NULL default ' ',
  "AAABLMYG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMYH","AAABLMYI","AAABLNRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMYK" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLMZJ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMYP" varchar(30) NOT NULL default ' ',
  "AAABLMYQ" varchar(30) NOT NULL default ' ',
  "AAABLMYR" varchar(30) NOT NULL default ' ',
  "AAABLMYS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLMZJ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMYX" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLMZJ" varchar(30) NOT NULL default ' ',
  "AAABLMZK" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMZD" varchar(30) NOT NULL default ' ',
  "AAABLMZE" varchar(30) NOT NULL default ' ',
  "AAABLMZF" varchar(30) NOT NULL default ' ',
  "AAABLMZG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLMZJ","AAABLMZK","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLMZM" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNAN" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLMZS" int(11) NOT NULL default '0',
  "AAABLMZT" varchar(1) NOT NULL default '0',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLMZW" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNAN","AAABLNEE","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNAD" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLNAN" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABMLQA","AAABLNAN","AAABLNEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNAP" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNRQ" varchar(30) NOT NULL default ' ',
  "AAABLNAT" varchar(30) NOT NULL default ' ',
  "AAABLNAU" varchar(60) NOT NULL default ' ',
  "AAABLNAV" varchar(60) NOT NULL default ' ',
  "AAABLNAW" varchar(2) NOT NULL default '00',
  "AAABLNAX" varchar(1) NOT NULL default ' ',
  "AAABLNAY" varchar(30) NOT NULL default ' ',
  "AAABLNAZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNBD" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNGH" varchar(132) NOT NULL default ' ',
  "AAABLNEQ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNBX" varchar(30) NOT NULL default ' ',
  "AAABLNBK" varchar(30) NOT NULL default ' ',
  "AAABLNBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNGH","AAABLNEQ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNBR" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNGH" varchar(132) NOT NULL default ' ',
  "AAABLNEQ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNBX" varchar(30) NOT NULL default ' ',
  "AAABLNDZ" varchar(30) NOT NULL default ' ',
  "AAABLNBZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNGH","AAABLNEQ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNCF" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNGH" varchar(132) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNCK" int(11) NOT NULL default '0',
  "AAABLNCL" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(132) NOT NULL default ' ',
  "AAABLNCN" varchar(60) NOT NULL default ' ',
  "AAABLNCO" varchar(132) NOT NULL default ' ',
  "AAABLNRQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNGH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNCU" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNCX" varchar(1) NOT NULL default ' ',
  "AAABLNCY" varchar(1) NOT NULL default ' ',
  "AAABLNCZ" varchar(30) NOT NULL default ' ',
  "AAABLNDA" varchar(60) NOT NULL default ' ',
  "AAABLROG" varchar(60) NOT NULL default ' ',
  "AAABLNDC" varchar(60) NOT NULL default ' ',
  "AAABLNDD" varchar(1) NOT NULL default ' ',
  "AAABLNSA" varchar(60) NOT NULL default ' ',
  "AAABLNEW" varchar(30) NOT NULL default ' ',
  "AAABLNDG" varchar(30) NOT NULL default ' ',
  "AAABLNDH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLNRO","AAABLNRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNDK" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(30) NOT NULL default ' ',
  "AAABLNED" varchar(30) NOT NULL default ' ',
  "AAABLNDO" varchar(2) NOT NULL default '00',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNDT" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(30) NOT NULL default ' ',
  "AAABLNED" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABLNDY" varchar(1) NOT NULL default '0',
  "AAABLNDZ" varchar(132) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNED","AAABLNEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNEF" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNGH" varchar(132) NOT NULL default ' ',
  "AAABLNEQ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNEL" varchar(30) NOT NULL default ' ',
  "AAABLNEM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNGH","AAABLNEQ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNES" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(60) NOT NULL default ' ',
  "AAABLNEW" varchar(30) NOT NULL default ' ',
  "AAABLNEX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNFA" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNFH" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNFH","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNFJ" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLNFM" varchar(30) NOT NULL default ' ',
  "AAABLNFN" longtext,
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLNRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNFT" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNFY" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNGH" varchar(132) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNHB" varchar(30) NOT NULL default ' ',
  "AAABLNGE" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNGH","AAABLTXI","AAABLNHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNGK" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNHB" varchar(60) NOT NULL default ' ',
  "AAABLNGO" longtext,
  "AAABLNGP" varchar(2) NOT NULL default '00',
  "AAABLNGQ" varchar(2) NOT NULL default '00',
  "AAABLNGR" varchar(1) NOT NULL default ' ',
  "AAABLNGS" varchar(1) NOT NULL default ' ',
  "AAABLNGT" varchar(1) NOT NULL default ' ',
  "AAABLNGU" varchar(1) NOT NULL default ' ',
  "AAABLNGV" varchar(1) NOT NULL default ' ',
  "AAABMOHM" longtext,
  "AAABLNSA" varchar(60) NOT NULL default ' ',
  "AAABLNGY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNHC" (
  "AAABLNOV" varchar(30) NOT NULL default ' ',
  "AAABLNHJ" varchar(30) NOT NULL default ' ',
  "AAABLNOA" varchar(30) NOT NULL default ' ',
  "AAABLNOB" varchar(30) NOT NULL default ' ',
  "AAABLNOC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNOV","AAABLNHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNHK" (
  "AAABLNHT" varchar(30) NOT NULL default ' ',
  "AAABLNHM" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNHP" (
  "AAABLNHT" varchar(30) NOT NULL default ' ',
  "AAABLNHU" varchar(30) NOT NULL default ' ',
  "AAABLNHV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNHT","AAABLNHU","AAABLNHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNHW" (
  "AAABLNRF" varchar(70) NOT NULL default ' ',
  "AAABLNQZ" varchar(30) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRF","AAABLNQZ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNIE" (
  "AAABLNRF" varchar(70) NOT NULL default ' ',
  "AAABLNQZ" varchar(30) NOT NULL default ' ',
  "AAABLNRA" varchar(30) NOT NULL default ' ',
  "AAABLNII" varchar(1) NOT NULL default ' ',
  "AAABLNIJ" varchar(1) NOT NULL default ' ',
  "AAABLNIK" varchar(30) NOT NULL default ' ',
  "AAABLNIL" varchar(30) NOT NULL default ' ',
  "AAABLNIM" varchar(1) NOT NULL default ' ',
  "AAABLNIN" varchar(30) NOT NULL default ' ',
  "AAABLNIO" varchar(1) NOT NULL default ' ',
  "AAABLNIP" varchar(30) NOT NULL default ' ',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAABLNIR" varchar(30) NOT NULL default ' ',
  "AAABLRXL" varchar(30) NOT NULL default ' ',
  "AAABLNIT" tinyint(3) unsigned NOT NULL default '0',
  "AAABLNIU" tinyint(3) unsigned NOT NULL default '0',
  "AAABLNIV" varchar(1) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  "AAABLNIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRF","AAABLNQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNJA" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(25) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLTVR","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNJJ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  "AAABLNJO" varchar(2) NOT NULL default '00',
  "AAABLNJP" varchar(1) NOT NULL default '0',
  "AAABLNJQ" varchar(30) NOT NULL default ' ',
  "AAABLNJR" varchar(132) NOT NULL default ' ',
  "AAABLNJS" varchar(1) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNKB" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNKJ" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNLC" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNKO" varchar(132) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNLC","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNKU" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABLNLC" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABLNLC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNLE" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMGHA" varchar(4) NOT NULL default '0000',
  "AAABLNLM" varchar(132) NOT NULL default ' ',
  "AAABLNLN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNOS","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNLS" (
  "AAABLNOV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNLV" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLNNR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNMA" varchar(30) NOT NULL default ' ',
  "AAABLNMB" varchar(30) NOT NULL default ' ',
  "AAABLNMC" varchar(30) NOT NULL default ' ',
  "AAABLNMD" varchar(30) NOT NULL default ' ',
  "AAABLNME" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLNMG" varchar(30) NOT NULL default ' ',
  "AAABLNMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLNNR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNMM" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLNNR" varchar(30) NOT NULL default ' ',
  "AAABLNNC" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNMS" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  "AAABLNMU" varchar(30) NOT NULL default ' ',
  "AAABLNMV" varchar(30) NOT NULL default ' ',
  "AAABLNNK" int(11) NOT NULL default '0',
  "AAABLNNL" int(11) NOT NULL default '0',
  "AAABLNNM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLNNR","AAABLNNC","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNNE" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLNNR" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNOE" varchar(30) NOT NULL default ' ',
  "AAABLNNK" int(11) NOT NULL default '0',
  "AAABLNNL" int(11) NOT NULL default '0',
  "AAABLNNM" int(11) NOT NULL default '0',
  "AAABLNNN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLNNR","AAABLTXI","AAABLNOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNNU" (
  "AAABLNOV" varchar(30) NOT NULL default ' ',
  "AAABLNOE" varchar(30) NOT NULL default ' ',
  "AAABLNNX" varchar(30) NOT NULL default ' ',
  "AAABLNNY" varchar(132) NOT NULL default ' ',
  "AAABLNNZ" longtext,
  "AAABLNOA" varchar(30) NOT NULL default ' ',
  "AAABLNOB" varchar(30) NOT NULL default ' ',
  "AAABLNOC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNOV","AAABLNOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNOF" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLNOI" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNPB"),
  UNIQUE KEY "AAABLNOM" ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNOO" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLNOV" varchar(30) NOT NULL default ' ',
  "AAABLNOS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLNOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNOW" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLNPG" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNPB" varchar(30) NOT NULL default ' ',
  "AAABLNPC" varchar(2) NOT NULL default '00',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLNPG","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNPI" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLNRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABMNJW","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNPT" (
  "AAABLNQV" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNQQ" varchar(30) NOT NULL default ' ',
  "AAABLNPX" varchar(30) NOT NULL default ' ',
  "AAABLNPY" varchar(1) NOT NULL default ' ',
  "AAABLOCJ" varchar(3) NOT NULL default '000',
  "AAABLNQA" varchar(50) NOT NULL default ' ',
  "AAABLNQB" varchar(50) NOT NULL default ' ',
  "AAABLNQC" varchar(50) NOT NULL default ' ',
  "AAABLNQD" varchar(50) NOT NULL default ' ',
  "AAABLNQE" varchar(50) NOT NULL default ' ',
  "AAABLNQF" varchar(60) NOT NULL default ' ',
  "AAABLNQG" varchar(50) NOT NULL default ' ',
  "AAABLNQH" varchar(50) NOT NULL default ' ',
  "AAABLNQI" varchar(50) NOT NULL default ' ',
  "AAABLNQJ" varchar(3) NOT NULL default '000',
  "AAABLNQK" varchar(50) NOT NULL default ' ',
  "AAABLNQL" varchar(50) NOT NULL default ' ',
  "AAABLNQM" varchar(50) NOT NULL default ' ',
  "AAABLNQN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNQV","AAABLNRP","AAABLNQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNQR" (
  "AAABLNQV" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNQU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNQV","AAABLNRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNQX" (
  "AAABLNRF" varchar(70) NOT NULL default ' ',
  "AAABLNQZ" varchar(30) NOT NULL default ' ',
  "AAABLNRA" varchar(30) NOT NULL default ' ',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAABLNRC" varchar(1) NOT NULL default ' ',
  "AAABLNRD" varchar(1) NOT NULL default ' ',
  "AAABLNRE" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNRG" (
  "AAABLNRO" varchar(30) NOT NULL default ' ',
  "AAABLNRP" varchar(60) NOT NULL default ' ',
  "AAABLNRQ" varchar(30) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLNRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNRO","AAABLNRP","AAABLNRQ","AAABMNJW","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNRT" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLNRX" varchar(2) NOT NULL default '00',
  "AAABLNRY" varchar(30) NOT NULL default ' ',
  "AAABLNRZ" varchar(30) NOT NULL default ' ',
  "AAABLNSA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNSE" (
  "AAABLNSJ" varchar(30) NOT NULL default ' ',
  "AAABLNSK" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNSJ","AAABLNSK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNTY" varchar(10) NOT NULL default ' ',
  "AAABLNSP" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLNTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNTY" varchar(10) NOT NULL default ' ',
  "AAABLNTZ" varchar(5) NOT NULL default '00000',
  "AAABLNTA" varchar(4) NOT NULL default ' ',
  "AAABLNTB" varchar(5) NOT NULL default '00000',
  "AAABLNTC" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABLNTL" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLNTO" decimal(11,2) NOT NULL default '0.00',
  "AAABLNTP" varchar(5) NOT NULL default ' ',
  "AAABLNTQ" decimal(5,0) NOT NULL default '0',
  "AAABLNTR" varchar(3) NOT NULL default ' ',
  "AAABLUIJ" varchar(8) NOT NULL default '00000000',
  "AAABLUIK" varchar(8) NOT NULL default '00000000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLOOD" varchar(1) NOT NULL default ' ',
  "AAABLNTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNTY","AAABLNTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNUG" varchar(10) NOT NULL default ' ',
  "AAABLNUH" varchar(18) NOT NULL default ' ',
  "AAABLNUI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNUG","AAABLNUH","AAABLNUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNVC" varchar(2) NOT NULL default ' ',
  "AAABLNVD" varchar(2) NOT NULL default ' ',
  "AAABLNVE" varchar(50) NOT NULL default ' ',
  "AAABLNVF" varchar(5) NOT NULL default ' ',
  "AAABLNVG" varchar(8) NOT NULL default '00000000',
  "AAABLNUQ" varchar(5) NOT NULL default ' ',
  "AAABLNUR" varchar(8) NOT NULL default '00000000',
  "AAABLNVM" varchar(12) NOT NULL default ' ',
  "AAABLNVJ" varchar(12) NOT NULL default ' ',
  "AAABLNUU" varchar(14) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLNUW" varchar(8) NOT NULL default '00000000',
  "AAABLNVP" varchar(8) NOT NULL default '00000000',
  "AAABLNVO" varchar(1) NOT NULL default ' ',
  "AAABLNUZ" varchar(6) NOT NULL default '000000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNVC","AAABLNVD","AAABLNVE","AAABLNVF","AAABLNVG"),
  KEY "AAABLNVH" ("AAABMRIL","AAABLNVJ"),
  KEY "AAABLNVK" ("AAABMRIL","AAABLNVM"),
  KEY "AAABLNVN" ("AAABLNVO","AAABLNVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNVQ" (
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNWD" varchar(4) NOT NULL default '0000',
  "AAABLNWF" varchar(10) NOT NULL default ' ',
  "AAABLNVW" varchar(2) NOT NULL default ' ',
  "AAABLNVX" varchar(10) NOT NULL default ' ',
  "AAABLNVY" varchar(3) NOT NULL default '000',
  "AAABLNWG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOAK","AAABMRIL","AAABLOAM","AAABLNWD"),
  KEY "AAABLNWE" ("AAABLNWF","AAABLNWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLNWR" varchar(8) NOT NULL default '00000000',
  "AAABLNWM" decimal(13,3) NOT NULL default '0.000',
  "AAABLNWN" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMLCZ","AAABLNWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNWS" (
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNXL" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  "AAABMLFE" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLOAK","AAABMRIL","AAABLOAM","AAABLNXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNXM" (
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNXQ" varchar(1) NOT NULL default ' ',
  "AAABLNZJ" varchar(12) NOT NULL default ' ',
  "AAABLNZP" varchar(10) NOT NULL default ' ',
  "AAABLNZQ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOAK","AAABMRIL","AAABLOAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABLNYC" varchar(1) NOT NULL default ' ',
  "AAABLOJO" varchar(1) NOT NULL default ' ',
  "AAABLNYE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABLTKC","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNYJ" (
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNYN" varchar(10) NOT NULL default ' ',
  "AAABLNYO" varchar(3) NOT NULL default '000',
  "AAABMAKT" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNYQ" varchar(2) NOT NULL default '00',
  "AAABLNYR" varchar(10) NOT NULL default ' ',
  "AAABLNYS" varchar(3) NOT NULL default '000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  "AAABLNYX" varchar(1) NOT NULL default ' ',
  "AAABLNYY" varchar(10) NOT NULL default ' ',
  "AAABLNZP" varchar(10) NOT NULL default ' ',
  "AAABLNZQ" varchar(14) NOT NULL default ' ',
  "AAABLNZB" varchar(1) NOT NULL default ' ',
  "AAABLNZC" varchar(1) NOT NULL default ' ',
  "AAABLNZV" varchar(1) NOT NULL default '0',
  "AAABLNZE" varchar(8) NOT NULL default '00000000',
  "AAABLNZF" varchar(8) NOT NULL default '00000000',
  "AAABLNZG" varchar(1) NOT NULL default ' ',
  "AAABLNZH" varchar(3) NOT NULL default ' ',
  "AAABLNZI" varchar(5) NOT NULL default '00000',
  "AAABLNZJ" varchar(12) NOT NULL default ' ',
  "AAABLOAJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOAK","AAABMRIL","AAABLOAM"),
  KEY "AAABLNZO" ("AAABLNZP","AAABLNZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLNZR" (
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLNZV" varchar(1) NOT NULL default '0',
  "AAABLNZW" varchar(8) NOT NULL default '00000000',
  "AAABLNZX" varchar(6) NOT NULL default '000000',
  "AAABLNZY" varchar(12) NOT NULL default ' ',
  "AAABLNZZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLOAK","AAABMRIL","AAABLOAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOAD" (
  "AAABLOAJ" varchar(12) NOT NULL default ' ',
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAM" varchar(16) NOT NULL default '0000000000000000',
  "AAABLOAI" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLOAJ","AAABLOAK","AAABMRIL","AAABLOAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOCP" varchar(4) NOT NULL default ' ',
  "AAABLOCQ" varchar(10) NOT NULL default ' ',
  "AAABLOAR" varchar(4) NOT NULL default ' ',
  "AAABLOAS" varchar(16) NOT NULL default ' ',
  "AAABLOAT" varchar(4) NOT NULL default ' ',
  "AAABLOAU" varchar(10) NOT NULL default ' ',
  "AAABLOAV" varchar(10) NOT NULL default ' ',
  "AAABLOAW" varchar(4) NOT NULL default ' ',
  "AAABLOAX" varchar(4) NOT NULL default ' ',
  "AAABLOCH" varchar(4) NOT NULL default '0000',
  "AAABLOAZ" varchar(25) NOT NULL default ' ',
  "AAABLOBA" varchar(8) NOT NULL default '00000000',
  "AAABLOBB" varchar(12) NOT NULL default ' ',
  "AAABLOBC" varchar(1) NOT NULL default ' ',
  "AAABLOCI" varchar(20) NOT NULL default ' ',
  "AAABLOCJ" varchar(3) NOT NULL default '000',
  "AAABLOCK" varchar(50) NOT NULL default ' ',
  "AAABLOCL" varchar(50) NOT NULL default ' ',
  "AAABLOCM" varchar(50) NOT NULL default ' ',
  "AAABLOCN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOCP","AAABLOCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOCP" varchar(4) NOT NULL default ' ',
  "AAABLOCQ" varchar(10) NOT NULL default ' ',
  "AAABLOCR" varchar(5) NOT NULL default ' ',
  "AAABLOBR" varchar(5) NOT NULL default '00000',
  "AAABLOBS" varchar(18) NOT NULL default ' ',
  "AAABLOBT" varchar(18) NOT NULL default ' ',
  "AAABLOBU" varchar(2) NOT NULL default ' ',
  "AAABLOBV" decimal(13,3) NOT NULL default '0.000',
  "AAABLOBW" varchar(3) NOT NULL default ' ',
  "AAABLOBX" decimal(13,3) NOT NULL default '0.000',
  "AAABLOBY" varchar(3) NOT NULL default ' ',
  "AAABMLFE" varchar(10) NOT NULL default ' ',
  "AAABLOCA" varchar(1) NOT NULL default ' ',
  "AAABLOCB" varchar(8) NOT NULL default '00000000',
  "AAABLOCC" varchar(1) NOT NULL default ' ',
  "AAABLOCD" varchar(50) NOT NULL default ' ',
  "AAABLOCE" varchar(18) NOT NULL default ' ',
  "AAABLOCF" varchar(1) NOT NULL default ' ',
  "AAABLOCG" decimal(23,4) NOT NULL default '0.0000',
  "AAABLOCH" varchar(4) NOT NULL default '0000',
  "AAABLOCI" varchar(20) NOT NULL default ' ',
  "AAABLOCJ" varchar(3) NOT NULL default '000',
  "AAABLOCK" varchar(50) NOT NULL default ' ',
  "AAABLOCL" varchar(50) NOT NULL default ' ',
  "AAABLOCM" varchar(50) NOT NULL default ' ',
  "AAABLOCN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOCP","AAABLOCQ","AAABLOCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(2) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABLOCX" varchar(14) NOT NULL default ' ',
  "AAABLOCY" varchar(14) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABLODG" varchar(1) NOT NULL default ' ',
  "AAABLODR" varchar(30) NOT NULL default ' ',
  "AAABLODI" varchar(30) NOT NULL default ' ',
  "AAABLODJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABLYAS","AAABLXYV"),
  KEY "AAABLODO" ("AAABMRIL","AAABLODQ","AAABLODR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLODS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLODX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLTNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCOV" varchar(4) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCOV","AAABLTNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOFD" (
  "AAABLOFL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOFG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOFL" varchar(4) NOT NULL default ' ',
  "AAABLOFJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLOFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOGD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOGD" varchar(6) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLOGG" varchar(6) NOT NULL default ' ',
  "AAABLOGD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLOGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLOGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGT" varchar(1) NOT NULL default ' ',
  "AAABLOGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHC" varchar(1) NOT NULL default ' ',
  "AAABLOHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHF" varchar(1) NOT NULL default ' ',
  "AAABLOHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHK" varchar(15) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLOHM" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLOHO" varchar(10) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABLOHS" decimal(5,2) NOT NULL default '0.00',
  "AAABLOHT" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHU" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHV" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHW" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHX" varchar(15) NOT NULL default ' ',
  "AAABLOHY" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIA" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIB" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIC" decimal(15,2) NOT NULL default '0.00',
  "AAABLOID" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIE" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIF" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIG" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIH" decimal(15,2) NOT NULL default '0.00',
  "AAABLOII" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIK" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIL" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIM" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIN" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIO" decimal(7,4) NOT NULL default '0.0000',
  "AAABLOIP" varchar(1) NOT NULL default ' ',
  "AAABLOIQ" varchar(1) NOT NULL default ' ',
  "AAABLOIR" varchar(1) NOT NULL default ' ',
  "AAABLOIS" varchar(1) NOT NULL default ' ',
  "AAABLOIT" varchar(10) NOT NULL default ' ',
  "AAABLOIU" varchar(1) NOT NULL default ' ',
  "AAABLOIV" varchar(1) NOT NULL default ' ',
  "AAABLOIW" varchar(8) NOT NULL default '00000000',
  "AAABLOIX" varchar(12) NOT NULL default ' ',
  "AAABLOIY" varchar(8) NOT NULL default '00000000',
  "AAABLOIZ" varchar(1) NOT NULL default ' ',
  "AAABLOJA" varchar(3) NOT NULL default '000',
  "AAABLOJB" varchar(8) NOT NULL default '00000000',
  "AAABLOJC" decimal(13,2) NOT NULL default '0.00',
  "AAABLOJD" varchar(10) NOT NULL default ' ',
  "AAABLOJE" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABLTNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOJL" (
  "AAABLOLL" varchar(30) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  "AAABLOJO" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLOJQ" varchar(30) NOT NULL default ' ',
  "AAABLOJR" varchar(30) NOT NULL default ' ',
  "AAABLOJS" varchar(30) NOT NULL default ' ',
  "AAABLOJT" varchar(30) NOT NULL default ' ',
  "AAABLOJU" varchar(30) NOT NULL default ' ',
  "AAABLOJV" varchar(30) NOT NULL default ' ',
  "AAABLOKP" varchar(30) NOT NULL default ' ',
  "AAABLOJX" varchar(20) NOT NULL default ' ',
  "AAABLOJY" varchar(20) NOT NULL default ' ',
  "AAABLOJZ" varchar(40) NOT NULL default ' ',
  "AAABLQJH" varchar(12) NOT NULL default ' ',
  "AAABLQJI" varchar(8) NOT NULL default '00000000',
  "AAABLQJJ" varchar(6) NOT NULL default '000000',
  "AAABLOKD" decimal(15,0) NOT NULL default '0',
  "AAABLQJL" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLOKH" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLOLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOKJ" (
  "AAABLOLL" varchar(30) NOT NULL default ' ',
  "AAABLOLM" varchar(2) NOT NULL default '00',
  "AAABLOKM" varchar(30) NOT NULL default ' ',
  "AAABLOKN" varchar(30) NOT NULL default ' ',
  "AAABLOKO" varchar(30) NOT NULL default ' ',
  "AAABLOKP" varchar(30) NOT NULL default ' ',
  "AAABLOKQ" varchar(30) NOT NULL default ' ',
  "AAABLOKR" varchar(30) NOT NULL default ' ',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOLL","AAABLOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOKV" (
  "AAABLOLL" varchar(30) NOT NULL default ' ',
  "AAABLOLM" varchar(2) NOT NULL default '00',
  "AAABLOKY" varchar(8) NOT NULL default '00000000',
  "AAABLOKZ" varchar(6) NOT NULL default '000000',
  "AAABLOLA" varchar(12) NOT NULL default ' ',
  "AAABLOLB" varchar(1) NOT NULL default ' ',
  "AAABLOLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOLL","AAABLOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOLF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOLL" varchar(30) NOT NULL default ' ',
  "AAABLOLM" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLOLL","AAABLOLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUJB" varchar(10) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLOLS" varchar(1) NOT NULL default ' ',
  "AAABLOLT" varchar(2) NOT NULL default ' ',
  "AAABLOLU" varchar(4) NOT NULL default ' ',
  "AAABLOLV" varchar(1) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLOLZ" varchar(2) NOT NULL default ' ',
  "AAABLOMA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUJB" varchar(10) NOT NULL default ' ',
  "AAABLUJC" varchar(5) NOT NULL default '00000',
  "AAABLOMH" varchar(8) NOT NULL default '00000000',
  "AAABLOMI" varchar(8) NOT NULL default '00000000',
  "AAABLUIJ" varchar(8) NOT NULL default '00000000',
  "AAABLUIK" varchar(8) NOT NULL default '00000000',
  "AAABLOML" varchar(10) NOT NULL default ' ',
  "AAABLOMM" varchar(10) NOT NULL default ' ',
  "AAABLOMN" varchar(2) NOT NULL default ' ',
  "AAABLOMO" varchar(6) NOT NULL default ' ',
  "AAABLOMP" varchar(6) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLOMS" decimal(5,0) NOT NULL default '0',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLPDW" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLPED" decimal(11,2) NOT NULL default '0.00',
  "AAABLONG" decimal(11,2) NOT NULL default '0.00',
  "AAABLONH" decimal(11,2) NOT NULL default '0.00',
  "AAABLONI" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDY" decimal(11,2) NOT NULL default '0.00',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  "AAABLPDZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEA" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEB" decimal(11,2) NOT NULL default '0.00',
  "AAABLONO" decimal(11,2) NOT NULL default '0.00',
  "AAABLONP" decimal(11,2) NOT NULL default '0.00',
  "AAABLONQ" decimal(11,2) NOT NULL default '0.00',
  "AAABLONR" decimal(11,2) NOT NULL default '0.00',
  "AAABLONS" decimal(11,2) NOT NULL default '0.00',
  "AAABLONT" decimal(11,2) NOT NULL default '0.00',
  "AAABLONU" decimal(11,2) NOT NULL default '0.00',
  "AAABLONV" decimal(11,2) NOT NULL default '0.00',
  "AAABLONW" decimal(11,2) NOT NULL default '0.00',
  "AAABLONX" decimal(11,2) NOT NULL default '0.00',
  "AAABLONY" decimal(11,2) NOT NULL default '0.00',
  "AAABLONZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLOOA" decimal(11,2) NOT NULL default '0.00',
  "AAABLOOB" decimal(11,2) NOT NULL default '0.00',
  "AAABLOOC" decimal(11,2) NOT NULL default '0.00',
  "AAABLOOD" varchar(1) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABLOOF" varchar(8) NOT NULL default '00000000',
  "AAABLOOG" varchar(8) NOT NULL default '00000000',
  "AAABLOOH" varchar(1) NOT NULL default ' ',
  "AAABLOOI" varchar(1) NOT NULL default ' ',
  "AAABLOOJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLOOK" varchar(10) NOT NULL default ' ',
  "AAABLOOL" varchar(5) NOT NULL default ' ',
  "AAABLOOM" varchar(5) NOT NULL default ' ',
  "AAABLOON" varchar(1) NOT NULL default ' ',
  "AAABLOOO" decimal(11,2) NOT NULL default '0.00',
  "AAABLOQA" varchar(10) NOT NULL default ' ',
  "AAABLOOQ" varchar(2) NOT NULL default ' ',
  "AAABLOOR" varchar(1) NOT NULL default ' ',
  "AAABLOOS" varchar(1) NOT NULL default ' ',
  "AAABLOOT" varchar(30) NOT NULL default ' ',
  "AAABLOOU" varchar(2) NOT NULL default ' ',
  "AAABLOOV" varchar(1) NOT NULL default ' ',
  "AAABLOOW" varchar(5) NOT NULL default ' ',
  "AAABLOOX" varchar(1) NOT NULL default ' ',
  "AAABLOOY" varchar(4) NOT NULL default ' ',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLQBX" varchar(10) NOT NULL default ' ',
  "AAABLOPB" varchar(2) NOT NULL default ' ',
  "AAABLPZJ" varchar(18) NOT NULL default ' ',
  "AAABLOPD" varchar(1) NOT NULL default ' ',
  "AAABLOPE" varchar(1) NOT NULL default ' ',
  "AAABLOPF" varchar(6) NOT NULL default ' ',
  "AAABLOPG" varchar(6) NOT NULL default ' ',
  "AAABLOPH" varchar(1) NOT NULL default ' ',
  "AAABLOPI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUJB","AAABLUJC"),
  KEY "AAABLOPM" ("AAABMRIL","AAABMLCZ","AAABMQII","AAABMJXV","AAABMCVI"),
  KEY "AAABLOPS" ("AAABMRIL","AAABMLCZ","AAABMCSR","AAABMCVI","AAABLURS"),
  KEY "AAABLOPY" ("AAABMRIL","AAABLOQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLOXJ" varchar(10) NOT NULL default '0000000000',
  "AAABLOXK" varchar(10) NOT NULL default '0000000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABLOQI" varchar(10) NOT NULL default '0000000000',
  "AAABLOQJ" varchar(4) NOT NULL default '0000',
  "AAABLOQK" varchar(5) NOT NULL default ' ',
  "AAABLOQL" decimal(13,3) NOT NULL default '0.000',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLOQN" varchar(10) NOT NULL default '0000000000',
  "AAABLPYU" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKQ","AAABLOXJ","AAABLOXK"),
  KEY "AAABLOQT" ("AAABMRIL","AAABMLCZ","AAABLUKQ"),
  KEY "AAABLOQX" ("AAABMRIL","AAABLPYU","AAABLUKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLORB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABLORE" varchar(1) NOT NULL default ' ',
  "AAABLORF" varchar(30) NOT NULL default ' ',
  "AAABLORG" varchar(30) NOT NULL default ' ',
  "AAABLORH" varchar(30) NOT NULL default ' ',
  "AAABLORI" varchar(30) NOT NULL default ' ',
  "AAABLORJ" varchar(30) NOT NULL default ' ',
  "AAABLORK" varchar(1) NOT NULL default ' ',
  "AAABLORL" varchar(14) NOT NULL default ' ',
  "AAABLORM" varchar(8) NOT NULL default ' ',
  "AAABLORN" varchar(8) NOT NULL default ' ',
  "AAABLORO" varchar(132) NOT NULL default ' ',
  "AAABLORP" varchar(1) NOT NULL default ' ',
  "AAABLORQ" smallint(6) NOT NULL default '0',
  "AAABLORR" decimal(31,0) NOT NULL default '0',
  "AAABLORS" varchar(20) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABLORU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLORX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABLOSV" varchar(7) NOT NULL default ' ',
  "AAABLOSB" tinyint(3) unsigned NOT NULL default '0',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABLOSD" varchar(30) NOT NULL default ' ',
  "AAABLOSE" varchar(30) NOT NULL default ' ',
  "AAABLOSF" varchar(1) NOT NULL default ' ',
  "AAABLOSG" varchar(2) NOT NULL default ' ',
  "AAABLOSH" varchar(45) NOT NULL default ' ',
  "AAABLOSI" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOSU","AAABLOSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOSU" varchar(40) NOT NULL default ' ',
  "AAABLOSV" varchar(7) NOT NULL default ' ',
  "AAABMOGI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOSU","AAABLOSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOSW" (
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABLOSY" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOTC" varchar(1) NOT NULL default ' ',
  "AAABLOTD" varchar(1) NOT NULL default ' ',
  "AAABLOTE" varchar(1) NOT NULL default ' ',
  "AAABLOTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRWE" varchar(10) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLRYV" varchar(18) NOT NULL default ' ',
  "AAABLOVB" varchar(1) NOT NULL default ' ',
  "AAABLRYU" varchar(1) NOT NULL default ' ',
  "AAABLOVD" varchar(3) NOT NULL default '000',
  "AAABLRVF" varchar(8) NOT NULL default '00000000',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAABLRWA" varchar(10) NOT NULL default ' ',
  "AAABLRWB" varchar(4) NOT NULL default ' ',
  "AAABLOVI" varchar(1) NOT NULL default ' ',
  "AAABLOVJ" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLOUA" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRWE","AAABLRMN","AAABLZJP","AAABMCJZ","AAABMRCU"),
  KEY "AAABLOUH" ("AAABMRIL","AAABLRMN"),
  KEY "AAABLOUK" ("AAABMRIL","AAABLRWE","AAABMNZR"),
  KEY "AAABLOUO" ("AAABMRIL","AAABLRYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLRYV" varchar(18) NOT NULL default ' ',
  "AAABLOVB" varchar(1) NOT NULL default ' ',
  "AAABLRYU" varchar(1) NOT NULL default ' ',
  "AAABLOVD" varchar(3) NOT NULL default '000',
  "AAABLRVF" varchar(8) NOT NULL default '00000000',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAABLRWA" varchar(10) NOT NULL default ' ',
  "AAABLRWB" varchar(4) NOT NULL default ' ',
  "AAABLOVI" varchar(1) NOT NULL default ' ',
  "AAABLOVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABLRMN","AAABLZJP","AAABMCJZ","AAABMRCU"),
  KEY "AAABLOVR" ("AAABLRMN"),
  KEY "AAABLOVT" ("AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRVF" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLOWD" varchar(1) NOT NULL default ' ',
  "AAABLOWE" varchar(1) NOT NULL default ' ',
  "AAABLOWF" varchar(8) NOT NULL default '00000000',
  "AAABLOWG" varchar(8) NOT NULL default '00000000',
  "AAABLOWH" varchar(2) NOT NULL default ' ',
  "AAABLOWI" varchar(8) NOT NULL default '00000000',
  "AAABLOWJ" varchar(1) NOT NULL default ' ',
  "AAABLOWK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMQII","AAABMJXV","AAABMCVI"),
  KEY "AAABLOWQ" ("AAABMRIL","AAABMQII","AAABMJXV","AAABMCVI"),
  KEY "AAABLOWV" ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLOXJ" varchar(10) NOT NULL default '0000000000',
  "AAABLOXK" varchar(10) NOT NULL default '0000000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOXS" varchar(2) NOT NULL default ' ',
  "AAABLOXN" varchar(2) NOT NULL default '00',
  "AAABLOXT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKQ","AAABLOXJ","AAABLOXK","AAABMRHS","AAABLOXS","AAABLOXN"),
  KEY "AAABLOXO" ("AAABMRIL","AAABMRHS","AAABLUKQ","AAABLOXS","AAABLOXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABLRWO" varchar(4) NOT NULL default '0000',
  "AAABLOXY" varchar(2) NOT NULL default ' ',
  "AAABLOXZ" varchar(8) NOT NULL default '00000000',
  "AAABLOYA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLRWN","AAABLRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABLOYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABLRWO" varchar(4) NOT NULL default '0000',
  "AAABLOYR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKC","AAABLRWN","AAABLRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLOZD" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABLOZF" varchar(5) NOT NULL default ' ',
  "AAABLOZR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPET","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLOZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABLOZR" varchar(8) NOT NULL default '00000000',
  "AAABLOZS" varchar(8) NOT NULL default '00000000',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABLPCX" varchar(1) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default ' ',
  "AAABLOZW" varchar(4) NOT NULL default ' ',
  "AAABLOZX" varchar(10) NOT NULL default ' ',
  "AAABLOZY" varchar(5) NOT NULL default ' ',
  "AAABLOZZ" varchar(4) NOT NULL default ' ',
  "AAABLPAA" varchar(2) NOT NULL default ' ',
  "AAABLPAB" varchar(2) NOT NULL default ' ',
  "AAABLPAC" varchar(10) NOT NULL default ' ',
  "AAABLPAD" varchar(4) NOT NULL default ' ',
  "AAABLPAE" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPAP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPET","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPET","AAABMQII","AAABMJXV","AAABLQAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLPBQ" varchar(5) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLPDN" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDR" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDS" varchar(5) NOT NULL default ' ',
  "AAABLPDT" decimal(5,0) NOT NULL default '0',
  "AAABLPDU" varchar(3) NOT NULL default ' ',
  "AAABLPDV" varchar(5) NOT NULL default ' ',
  "AAABLPDW" decimal(11,2) NOT NULL default '0.00',
  "AAABLPCG" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDY" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEP" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDP" decimal(13,2) NOT NULL default '0.00',
  "AAABLPDQ" decimal(13,2) NOT NULL default '0.00',
  "AAABLPDO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPEO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPCP" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEQ" decimal(13,2) NOT NULL default '0.00',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLXNZ" varchar(18) NOT NULL default ' ',
  "AAABLPCX" varchar(1) NOT NULL default ' ',
  "AAABLPEC" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLPET","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPET" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLPEV" varchar(6) NOT NULL default '000000',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLPDI" decimal(15,2) NOT NULL default '0.00',
  "AAABLPDJ" decimal(13,2) NOT NULL default '0.00',
  "AAABLPFM" decimal(15,2) NOT NULL default '0.00',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLPDM" varchar(1) NOT NULL default ' ',
  "AAABLPDN" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPDP" decimal(13,2) NOT NULL default '0.00',
  "AAABLPDQ" decimal(13,2) NOT NULL default '0.00',
  "AAABLPDR" decimal(13,3) NOT NULL default '0.000',
  "AAABLPDS" varchar(5) NOT NULL default ' ',
  "AAABLPDT" decimal(5,0) NOT NULL default '0',
  "AAABLPDU" varchar(3) NOT NULL default ' ',
  "AAABLPDV" varchar(5) NOT NULL default ' ',
  "AAABLPDW" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDX" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDY" decimal(11,2) NOT NULL default '0.00',
  "AAABLPDZ" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEA" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEB" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEC" decimal(13,2) NOT NULL default '0.00',
  "AAABLPED" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEE" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEF" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEG" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEH" decimal(11,2) NOT NULL default '0.00',
  "AAABLPEI" varchar(1) NOT NULL default ' ',
  "AAABLPFK" smallint(6) NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLPEO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPEP" decimal(13,3) NOT NULL default '0.000',
  "AAABLPEQ" decimal(13,2) NOT NULL default '0.00',
  "AAABLPER" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLPET","AAABMLTD","AAABLPEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLPFK" smallint(6) NOT NULL default '0',
  "AAABLPFL" decimal(15,2) NOT NULL default '0.00',
  "AAABLPFM" decimal(15,2) NOT NULL default '0.00',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPGA","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPFY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPGA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLPHC" varchar(10) NOT NULL default ' ',
  "AAABLPGG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABLPHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLPHC" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABLPHC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLPHC" varchar(10) NOT NULL default ' ',
  "AAABLPHZ" varchar(4) NOT NULL default '0000',
  "AAABLPGZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABLPHC","AAABLPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLPHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPHY" varchar(10) NOT NULL default ' ',
  "AAABLPHZ" varchar(4) NOT NULL default '0000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPHY","AAABLPHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJC" varchar(22) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLPIE" varchar(50) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLPIM" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJC" varchar(22) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMDBA" varchar(70) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJC","AAABLPJD","AAABMDBA","AAABMLTD"),
  KEY "AAABLPJG" ("AAABMRIL","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  "AAABLPTG" varchar(1) NOT NULL default ' ',
  "AAABLPTH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABLPTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPJT" (
  "AAABLPNZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPJW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNZ" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLPMB" varchar(6) NOT NULL default ' ',
  "AAABLPKI" varchar(1) NOT NULL default ' ',
  "AAABLPNT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPKY" varchar(1) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLPLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNF","AAABLPPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPNF","AAABLPPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABLPMB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNF","AAABLPMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABLPMB" varchar(6) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPNF","AAABLPMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPMK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPNF","AAABLPPS","AAABLPQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLPNF" varchar(10) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABLPTE" varchar(12) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPNL" (
  "AAABLPNT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPNO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNT" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLPNY" varchar(2) NOT NULL default '00',
  "AAABLPNZ" varchar(20) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLRBQ" varchar(4) NOT NULL default ' ',
  "AAABLPSX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  "AAABLPOK" varchar(20) NOT NULL default ' ',
  "AAABLPOL" varchar(20) NOT NULL default ' ',
  "AAABLPOM" varchar(20) NOT NULL default ' ',
  "AAABLPON" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  "AAABLPOW" varchar(1) NOT NULL default ' ',
  "AAABLPQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPTK","AAABLPRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPTK","AAABLPRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABLPRS" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  "AAABLPPU" varchar(10) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLPPW" varchar(4) NOT NULL default ' ',
  "AAABLRBQ" varchar(4) NOT NULL default ' ',
  "AAABMLTD" int(11) NOT NULL default '0',
  "AAABLPPZ" int(11) NOT NULL default '0',
  "AAABLPQA" int(11) NOT NULL default '0',
  "AAABLPQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPTK","AAABLPRR","AAABLPRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABLPRS" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPTK","AAABLPRR","AAABLPRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABLPRS" varchar(2) NOT NULL default '00',
  "AAABLPRT" varchar(2) NOT NULL default '00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABLPQZ" varchar(1) NOT NULL default ' ',
  "AAABLPRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPTK","AAABLPRR","AAABLPRS","AAABLPRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABLPRR" varchar(10) NOT NULL default ' ',
  "AAABLPRS" varchar(2) NOT NULL default '00',
  "AAABLPRT" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPTK","AAABLPRR","AAABLPRS","AAABLPRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPSX" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPSX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSX" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPSX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPSX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPSX" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLPTK" varchar(10) NOT NULL default ' ',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABLPTE" varchar(12) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABLPTG" varchar(1) NOT NULL default ' ',
  "AAABLPTH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABLPTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABLPYE" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABMQDU","AAABLPVQ"),
  KEY "AAABLPTX" ("AAABMRIL","AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABLPYA" varchar(132) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABLPYE" varchar(12) NOT NULL default ' ',
  "AAABLPYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABLPYP" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABLPYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLPUZ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLPYO","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAABLPVJ" varchar(30) NOT NULL default ' ',
  "AAABLPVK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABMQDU","AAABLPVQ"),
  KEY "AAABLPVP" ("AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLPVW" varchar(2) NOT NULL default '00',
  "AAABLPVX" varchar(2) NOT NULL default ' ',
  "AAABLPVY" varchar(132) NOT NULL default ' ',
  "AAABLPVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABMQDU","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLPYP" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABMQDU","AAABMLPO","AAABLPYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLPWW" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLPYO","AAABMQDU","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPXF" varchar(1) NOT NULL default ' ',
  "AAABLPXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLPXS" varchar(8) NOT NULL default '00000000',
  "AAABLPXT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABLXIY","AAABLPXS","AAABLPXT"),
  KEY "AAABLPXU" ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABLPYA" varchar(132) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABLPYE" varchar(12) NOT NULL default ' ',
  "AAABLPYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYO" varchar(32) NOT NULL default ' ',
  "AAABLPYP" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYO","AAABLPYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPYU" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLPZJ" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLPZA" varchar(1) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW"),
  KEY "AAABLPZH" ("AAABMRIL","AAABLPZJ","AAABLZJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPZP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLPZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLQBX" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLQAJ" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLQBI" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW","AAABLQBX","AAABMQII","AAABMJXV","AAABLQAJ","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLQBX" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW","AAABLQBX","AAABMLCZ","AAABLZJP"),
  KEY "AAABLQAW" ("AAABMRIL","AAABMLCZ","AAABLZJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLQBX" varchar(10) NOT NULL default ' ',
  "AAABLQBE" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLQBG" varchar(1) NOT NULL default ' ',
  "AAABLQBH" varchar(1) NOT NULL default ' ',
  "AAABLQBI" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLQBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW","AAABLQBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQBW" varchar(10) NOT NULL default ' ',
  "AAABLQBX" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQBU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQBW","AAABLQBX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABLUHL" varchar(6) NOT NULL default '000000',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABLQCG" varchar(45) NOT NULL default ' ',
  "AAABLQCH" varchar(45) NOT NULL default ' ',
  "AAABLQCI" varchar(45) NOT NULL default ' ',
  "AAABLQCJ" varchar(45) NOT NULL default ' ',
  "AAABLRUK" varchar(10) NOT NULL default ' ',
  "AAABLQDA" varchar(8) NOT NULL default '00000000',
  "AAABLQCM" varchar(1) NOT NULL default ' ',
  "AAABLQCN" varchar(8) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLQCP" varchar(30) NOT NULL default ' ',
  "AAABLQCQ" varchar(50) NOT NULL default ' ',
  "AAABLQCR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSEB","AAABLUHL","AAABLXDR","AAABMLDW","AAABMRCU"),
  KEY "AAABLQCY" ("AAABMRIL","AAABLQDA","AAABLRUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQDC" (
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABLQDE" varchar(30) NOT NULL default ' ',
  "AAABLQDF" varchar(1) NOT NULL default ' ',
  "AAABLQDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQDI" (
  "AAABLQFK" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABLQDN" varchar(1) NOT NULL default ' ',
  "AAABLQDO" varchar(1) NOT NULL default ' ',
  "AAABLQDP" varchar(30) NOT NULL default ' ',
  "AAABLQDQ" varchar(30) NOT NULL default ' ',
  "AAABLQFL" varchar(2) NOT NULL default '00',
  "AAABLQDS" varchar(1) NOT NULL default ' ',
  "AAABLQDT" varchar(30) NOT NULL default ' ',
  "AAABLQDU" varchar(30) NOT NULL default ' ',
  "AAABLQDV" varchar(20) NOT NULL default ' ',
  "AAABLQDW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQFK","AAABMREF","AAABLQEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQEN" varchar(1) NOT NULL default ' ',
  "AAABLQEO" varchar(30) NOT NULL default ' ',
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLQER" varchar(10) NOT NULL default '0000000000',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQEN","AAABLQEO","AAABLQEP","AAABMREF","AAABLQER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQES" (
  "AAABLQFK" varchar(10) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQEW" (
  "AAABLQFK" varchar(10) NOT NULL default ' ',
  "AAABLQFL" varchar(2) NOT NULL default '00',
  "AAABLQEZ" varchar(4) NOT NULL default ' ',
  "AAABLQFA" varchar(40) NOT NULL default ' ',
  "AAABLQFB" varchar(30) NOT NULL default ' ',
  "AAABLQFC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQFK","AAABLQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQFF" (
  "AAABLQFK" varchar(10) NOT NULL default ' ',
  "AAABLQFL" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLQFJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQFK","AAABLQFL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLQFQ" varchar(255) NOT NULL default ' ',
  "AAABLQFR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABMDRZ" varchar(8) NOT NULL default ' ',
  "AAABLQGK" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(4) NOT NULL default ' ',
  "AAABLQGM" varchar(10) NOT NULL default ' ',
  "AAABLQGN" varchar(10) NOT NULL default ' ',
  "AAABLQGO" int(11) NOT NULL default '0',
  "AAABLQGP" int(11) NOT NULL default '0',
  "AAABLQGE" int(11) NOT NULL default '0',
  "AAABMRCU" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSEB","AAABMDRZ","AAABLQGK","AAABLQGL","AAABLQGM","AAABLQGN","AAABLQGO","AAABLQGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLRUK" varchar(10) NOT NULL default ' ',
  "AAABLQHF" varchar(4) NOT NULL default ' ',
  "AAABLQHG" varchar(3) NOT NULL default ' ',
  "AAABLQHH" varchar(35) NOT NULL default ' ',
  "AAABLQHI" varchar(35) NOT NULL default ' ',
  "AAABLQHJ" varchar(35) NOT NULL default ' ',
  "AAABLQHK" varchar(35) NOT NULL default ' ',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABLRUK","AAABLQHF","AAABLQHG","AAABLQHH","AAABLQHI","AAABLQHJ","AAABLQHK","AAABLSEB","AAABMLDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLQIO" varchar(6) NOT NULL default '000000',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQKM","AAABMLPO","AAABLQIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABLQIO" varchar(6) NOT NULL default '000000',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLQKM","AAABMLPO","AAABLQIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABLQJH" varchar(12) NOT NULL default ' ',
  "AAABLQJI" varchar(8) NOT NULL default '00000000',
  "AAABLQJJ" varchar(6) NOT NULL default '000000',
  "AAABLQJK" varchar(4) NOT NULL default ' ',
  "AAABLQJL" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLQJO" varchar(6) NOT NULL default '000000',
  "AAABLQJP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABLQJH" varchar(12) NOT NULL default ' ',
  "AAABLQJI" varchar(8) NOT NULL default '00000000',
  "AAABLQJJ" varchar(6) NOT NULL default '000000',
  "AAABLQJK" varchar(4) NOT NULL default ' ',
  "AAABLQJL" varchar(12) NOT NULL default ' ',
  "AAABLQJM" varchar(8) NOT NULL default '00000000',
  "AAABLQJN" varchar(6) NOT NULL default '000000',
  "AAABLQJO" varchar(6) NOT NULL default '000000',
  "AAABLQJP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLQKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQKM","AAABMRHS","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQKM" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLQKM","AAABMRHS","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQKV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRZS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQKV","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLQLA" varchar(8) NOT NULL default '00000000',
  "AAABLQLB" varchar(8) NOT NULL default '00000000',
  "AAABLQLC" varchar(8) NOT NULL default '00000000',
  "AAABLQLD" varchar(8) NOT NULL default '00000000',
  "AAABLQLE" varchar(1) NOT NULL default ' ',
  "AAABLRBQ" varchar(4) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLQPD" varchar(4) NOT NULL default ' ',
  "AAABLQLI" varchar(2) NOT NULL default ' ',
  "AAABLQLJ" varchar(3) NOT NULL default ' ',
  "AAABLQLK" varchar(6) NOT NULL default '000000',
  "AAABLQLL" varchar(6) NOT NULL default '000000',
  "AAABLQLM" varchar(6) NOT NULL default '000000',
  "AAABLQLN" varchar(6) NOT NULL default '000000',
  "AAABLQLO" varchar(6) NOT NULL default '000000',
  "AAABLQLP" varchar(6) NOT NULL default '000000',
  "AAABLQLQ" varchar(6) NOT NULL default '000000',
  "AAABLQLR" varchar(6) NOT NULL default '000000',
  "AAABLQLS" varchar(6) NOT NULL default '000000',
  "AAABLQLT" varchar(6) NOT NULL default '000000',
  "AAABLQLU" varchar(6) NOT NULL default '000000',
  "AAABLQLV" varchar(6) NOT NULL default '000000',
  "AAABLQLW" varchar(6) NOT NULL default '000000',
  "AAABLQLX" varchar(6) NOT NULL default '000000',
  "AAABLQLY" varchar(6) NOT NULL default '000000',
  "AAABLQLZ" varchar(6) NOT NULL default '000000',
  "AAABLQMA" varchar(6) NOT NULL default '000000',
  "AAABLQMB" varchar(6) NOT NULL default '000000',
  "AAABLQMC" varchar(6) NOT NULL default '000000',
  "AAABLQMD" varchar(6) NOT NULL default '000000',
  "AAABLQME" varchar(6) NOT NULL default '000000',
  "AAABLQMF" varchar(6) NOT NULL default '000000',
  "AAABLQMG" varchar(6) NOT NULL default '000000',
  "AAABLQMH" varchar(6) NOT NULL default '000000',
  "AAABLQMI" varchar(6) NOT NULL default '000000',
  "AAABLQMJ" varchar(6) NOT NULL default '000000',
  "AAABLQMK" varchar(6) NOT NULL default '000000',
  "AAABLQML" varchar(6) NOT NULL default '000000',
  "AAABLQPA" decimal(6,0) NOT NULL default '0',
  "AAABLQPB" varchar(3) NOT NULL default ' ',
  "AAABLQMO" varchar(2) NOT NULL default ' ',
  "AAABLQMP" varchar(4) NOT NULL default ' ',
  "AAABLQMQ" varchar(4) NOT NULL default ' ',
  "AAABLRBJ" varchar(1) NOT NULL default ' ',
  "AAABLQMS" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLRBK" varchar(1) NOT NULL default ' ',
  "AAABLQMW" varchar(4) NOT NULL default ' ',
  "AAABLQMX" varchar(4) NOT NULL default ' ',
  "AAABLQMY" varchar(2) NOT NULL default ' ',
  "AAABLQMZ" varchar(4) NOT NULL default ' ',
  "AAABLQNA" varchar(4) NOT NULL default ' ',
  "AAABLRYH" varchar(1) NOT NULL default ' ',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABLRDX" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLQNG" decimal(5,2) NOT NULL default '0.00',
  "AAABLQNH" decimal(5,0) NOT NULL default '0',
  "AAABLQNI" varchar(5) NOT NULL default ' ',
  "AAABLQNJ" varchar(4) NOT NULL default ' ',
  "AAABLQNK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG"),
  KEY "AAABLQNN" ("AAABMRIL","AAABLRYH"),
  KEY "AAABLQNQ" ("AAABMRIL","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLQOZ" varchar(25) NOT NULL default ' ',
  "AAABLQNY" varchar(10) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLQOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLQOQ" varchar(4) NOT NULL default ' ',
  "AAABLQOK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLXOB","AAABMCJZ","AAABMNZR","AAABLQOQ"),
  KEY "AAABLQOR" ("AAABMRIL","AAABLXOB","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABLQOZ" varchar(25) NOT NULL default ' ',
  "AAABLQPA" decimal(6,0) NOT NULL default '0',
  "AAABLQPB" varchar(3) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLQPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLQPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABLQPQ" varchar(15) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLQPP","AAABLQPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABLQPW" varchar(13) NOT NULL default '0000000000000',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  "AAABMAZB" varchar(1) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQQA" varchar(4) NOT NULL default ' ',
  "AAABLQQB" varchar(1) NOT NULL default ' ',
  "AAABLQQC" varchar(4) NOT NULL default ' ',
  "AAABLQQD" varchar(1) NOT NULL default ' ',
  "AAABLQYN" varchar(2) NOT NULL default ' ',
  "AAABLQQF" varchar(4) NOT NULL default ' ',
  "AAABLQQG" varchar(2) NOT NULL default ' ',
  "AAABLQQH" varchar(2) NOT NULL default ' ',
  "AAABLQQI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLRYG","AAABLXOB"),
  KEY "AAABLQQM" ("AAABMRIL","AAABLXOB","AAABLRYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQQQ" (
  "AAABLQRP" varchar(10) NOT NULL default ' ',
  "AAABLQQS" varchar(26) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLQQW" varchar(26) NOT NULL default ' ',
  "AAABLQQX" varchar(30) NOT NULL default ' ',
  "AAABLQQY" varchar(26) NOT NULL default ' ',
  "AAABLQQZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQRB" (
  "AAABLQRP" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLQRF" varchar(3) NOT NULL default ' ',
  "AAABLQRG" varchar(1) NOT NULL default ' ',
  "AAABLQRH" varchar(30) NOT NULL default ' ',
  "AAABLQRI" varchar(10) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLQRK" varchar(30) NOT NULL default ' ',
  "AAABLQRL" varchar(30) NOT NULL default ' ',
  "AAABLQRM" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABLQRO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQRP","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQRT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLUTZ" varchar(7) NOT NULL default '0000000',
  "AAABLQTD" varchar(8) NOT NULL default '00000000',
  "AAABLQSL" varchar(12) NOT NULL default ' ',
  "AAABLQSM" smallint(6) NOT NULL default '0',
  "AAABLQSD" varchar(16) NOT NULL default '0000000000000000',
  "AAABLQSE" varchar(16) NOT NULL default '0000000000000000',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLXQ","AAABLUTZ","AAABLQTD","AAABLQSL","AAABLQSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLUTZ" varchar(7) NOT NULL default '0000000',
  "AAABLQTD" varchar(8) NOT NULL default '00000000',
  "AAABLQST" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLQSV" smallint(6) NOT NULL default '0',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLXQ","AAABLUTZ","AAABLQTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLQTH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLQUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQZV" varchar(10) NOT NULL default '0000000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  "AAABLQUM" varchar(10) NOT NULL default ' ',
  "AAABLQUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQZV","AAABMPLT","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQZV" varchar(10) NOT NULL default '0000000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQZV","AAABMPLT","AAABMLCZ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLQZP" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYI" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLQYF" decimal(3,0) NOT NULL default '0',
  "AAABLQYG" decimal(3,0) NOT NULL default '0',
  "AAABLQYH" varchar(1) NOT NULL default ' ',
  "AAABLQZQ" varchar(1) NOT NULL default ' ',
  "AAABLQVY" varchar(1) NOT NULL default ' ',
  "AAABLQVZ" varchar(1) NOT NULL default ' ',
  "AAABLQWA" varchar(1) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLQYJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYK" decimal(13,3) NOT NULL default '0.000',
  "AAABLQWE" decimal(3,0) NOT NULL default '0',
  "AAABLQWF" varchar(9) NOT NULL default ' ',
  "AAABLQWG" varchar(1) NOT NULL default ' ',
  "AAABLQWH" decimal(3,0) NOT NULL default '0',
  "AAABLQWI" decimal(3,0) NOT NULL default '0',
  "AAABLQWJ" varchar(3) NOT NULL default ' ',
  "AAABLQWK" varchar(1) NOT NULL default ' ',
  "AAABLQWL" varchar(2) NOT NULL default ' ',
  "AAABLQWM" varchar(4) NOT NULL default ' ',
  "AAABLQWN" varchar(4) NOT NULL default ' ',
  "AAABLQWO" varchar(10) NOT NULL default ' ',
  "AAABLQWP" varchar(1) NOT NULL default ' ',
  "AAABLQWQ" varchar(4) NOT NULL default ' ',
  "AAABLQWR" varchar(1) NOT NULL default ' ',
  "AAABLQWS" varchar(1) NOT NULL default ' ',
  "AAABLQWT" varchar(1) NOT NULL default ' ',
  "AAABLQWU" varchar(1) NOT NULL default ' ',
  "AAABLQWV" varchar(2) NOT NULL default '00',
  "AAABLQWW" varchar(1) NOT NULL default ' ',
  "AAABLQWX" varchar(18) NOT NULL default ' ',
  "AAABLQWY" varchar(1) NOT NULL default ' ',
  "AAABLQWZ" varchar(1) NOT NULL default ' ',
  "AAABLQXA" varchar(4) NOT NULL default ' ',
  "AAABLQXB" varchar(2) NOT NULL default ' ',
  "AAABLQXC" varchar(2) NOT NULL default ' ',
  "AAABLQXD" varchar(1) NOT NULL default ' ',
  "AAABLQXE" varchar(3) NOT NULL default ' ',
  "AAABLQXF" varchar(4) NOT NULL default ' ',
  "AAABLQXG" varchar(4) NOT NULL default ' ',
  "AAABLQXH" varchar(4) NOT NULL default ' ',
  "AAABLQXI" decimal(30,14) NOT NULL default '0.00000000000000',
  "AAABLQXJ" decimal(30,14) NOT NULL default '0.00000000000000',
  "AAABLQXK" varchar(3) NOT NULL default ' ',
  "AAABLQXL" varchar(1) NOT NULL default ' ',
  "AAABLQXM" decimal(13,3) NOT NULL default '0.000',
  "AAABLQXN" varchar(1) NOT NULL default ' ',
  "AAABLQXO" decimal(13,3) NOT NULL default '0.000',
  "AAABLQXP" varchar(3) NOT NULL default ' ',
  "AAABLQXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMLCZ"),
  KEY "AAABLQXU" ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLQYN" varchar(2) NOT NULL default ' ',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLQYF" decimal(3,0) NOT NULL default '0',
  "AAABLQYG" decimal(3,0) NOT NULL default '0',
  "AAABLQYH" varchar(1) NOT NULL default ' ',
  "AAABLQYI" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYK" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABLQYN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQZV" varchar(10) NOT NULL default '0000000000',
  "AAABLQYR" decimal(15,0) NOT NULL default '0',
  "AAABLQYS" varchar(12) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLQYV" int(11) NOT NULL default '0',
  "AAABLQYW" varchar(1) NOT NULL default ' ',
  "AAABLQYX" varchar(1) NOT NULL default ' ',
  "AAABLQYY" varchar(1) NOT NULL default ' ',
  "AAABLQYZ" varchar(1) NOT NULL default ' ',
  "AAABLQZA" varchar(8) NOT NULL default '00000000',
  "AAABLQZB" varchar(1) NOT NULL default ' ',
  "AAABLQZC" varchar(1) NOT NULL default ' ',
  "AAABLQZD" varchar(1) NOT NULL default ' ',
  "AAABLQZE" varchar(1) NOT NULL default ' ',
  "AAABLQZF" varchar(1) NOT NULL default ' ',
  "AAABLQZG" decimal(3,0) NOT NULL default '0',
  "AAABLQZH" varchar(1) NOT NULL default ' ',
  "AAABLQZI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLQZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLQZP" decimal(13,3) NOT NULL default '0.000',
  "AAABLQZQ" varchar(1) NOT NULL default ' ',
  "AAABLQZR" decimal(15,0) NOT NULL default '0',
  "AAABLQZS" varchar(6) NOT NULL default ' ',
  "AAABLQZT" decimal(15,0) NOT NULL default '0',
  "AAABLQZU" varchar(6) NOT NULL default ' ',
  "AAABLQZV" varchar(10) NOT NULL default '0000000000',
  "AAABLQZW" decimal(13,3) NOT NULL default '0.000',
  "AAABLQZX" varchar(1) NOT NULL default ' ',
  "AAABLQZY" int(11) NOT NULL default '0',
  "AAABLQZZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAA" varchar(8) NOT NULL default '00000000',
  "AAABLRAB" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAC" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAD" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAE" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAF" varchar(3) NOT NULL default ' ',
  "AAABLRAG" varchar(4) NOT NULL default ' ',
  "AAABLRAH" varchar(10) NOT NULL default ' ',
  "AAABLRAI" varchar(8) NOT NULL default '00000000',
  "AAABLRAJ" varchar(6) NOT NULL default '000000',
  "AAABLRAK" varchar(30) NOT NULL default ' ',
  "AAABLRAL" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAM" varchar(3) NOT NULL default ' ',
  "AAABLRAN" varchar(4) NOT NULL default ' ',
  "AAABLRAO" varchar(10) NOT NULL default ' ',
  "AAABLRAP" varchar(2) NOT NULL default ' ',
  "AAABLRAQ" varchar(70) NOT NULL default ' ',
  "AAABLRAR" varchar(6) NOT NULL default '000000',
  "AAABLRAS" varchar(8) NOT NULL default '00000000',
  "AAABLRAT" varchar(6) NOT NULL default '000000',
  "AAABLRAU" varchar(1) NOT NULL default ' ',
  "AAABLRAV" varchar(4) NOT NULL default ' ',
  "AAABLRAW" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAX" decimal(13,3) NOT NULL default '0.000',
  "AAABLRAY" varchar(3) NOT NULL default ' ',
  "AAABLRAZ" varchar(2) NOT NULL default ' ',
  "AAABLRBA" varchar(70) NOT NULL default ' ',
  "AAABLRBB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABLRBJ" varchar(1) NOT NULL default ' ',
  "AAABLRBK" varchar(1) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLRBQ" varchar(1) NOT NULL default ' ',
  "AAABLRBR" varchar(1) NOT NULL default ' ',
  "AAABLRCB" varchar(1) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLRBU" varchar(1) NOT NULL default ' ',
  "AAABLRBV" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKC"),
  KEY "AAABLRBZ" ("AAABMRIL","AAABLRCB","AAABMQII","AAABMJXV"),
  KEY "AAABLRCE" ("AAABMRIL","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLRCL" varchar(8) NOT NULL default '00000000',
  "AAABLRCM" varchar(6) NOT NULL default '000000',
  "AAABLRCN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABMAZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKC","AAABLXOB"),
  KEY "AAABLRCZ" ("AAABMRIL","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLZCP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLSKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLRDT" varchar(1) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLRDW" varchar(1) NOT NULL default ' ',
  "AAABLRDX" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLSKC","AAABMRCU"),
  KEY "AAABLREG" ("AAABMRIL","AAABLRYG"),
  KEY "AAABLREJ" ("AAABMRIL","AAABMPLT"),
  KEY "AAABLREM" ("AAABMRIL","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLREQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABLRFA" varchar(20) NOT NULL default ' ',
  "AAABLRFB" varchar(2) NOT NULL default ' ',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMKFH","AAABLRFA","AAABLRFB","AAABLRFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLRFH" varchar(1) NOT NULL default ' ',
  "AAABLRFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRGA" varchar(40) NOT NULL default ' ',
  "AAABLRFY" varchar(100) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRFW" (
  "AAABLRGA" varchar(40) NOT NULL default ' ',
  "AAABLRFY" varchar(100) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLRGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRJS" varchar(40) NOT NULL default ' ',
  "AAABMKXW" varchar(40) NOT NULL default ' ',
  "AAABLRJQ" varchar(4) NOT NULL default '0000',
  "AAABLRGH" varchar(4) NOT NULL default ' ',
  "AAABLRGI" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRJS","AAABMKXW"),
  KEY "AAABLRGO" ("AAABMCVI","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRIF" varchar(3) NOT NULL default '000',
  "AAABLRJZ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRIF" varchar(3) NOT NULL default '000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRIF","AAABLRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRIF" varchar(3) NOT NULL default '000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRIF","AAABLRXN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRIF" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRIF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRIT" varchar(40) NOT NULL default ' ',
  "AAABLRIS" varchar(2) NOT NULL default '00',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRIQ" (
  "AAABLRIT" varchar(40) NOT NULL default ' ',
  "AAABLRIS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLRIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRJS" varchar(40) NOT NULL default ' ',
  "AAABMKXW" varchar(40) NOT NULL default ' ',
  "AAABLRJP" varchar(2) NOT NULL default '00',
  "AAABLRJL" varchar(40) NOT NULL default ' ',
  "AAABLRJM" varchar(5) NOT NULL default '00000',
  "AAABLRJN" varchar(5) NOT NULL default '00000',
  "AAABLRJO" varchar(1) NOT NULL default ' ',
  "AAABLRJR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRJS","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRJH" (
  "AAABLRJS" varchar(40) NOT NULL default ' ',
  "AAABMKXW" varchar(40) NOT NULL default ' ',
  "AAABLRJK" varchar(1) NOT NULL default ' ',
  "AAABLRJL" varchar(40) NOT NULL default ' ',
  "AAABLRJM" varchar(5) NOT NULL default '00000',
  "AAABLRJN" varchar(5) NOT NULL default '00000',
  "AAABLRJO" varchar(1) NOT NULL default ' ',
  "AAABLRJP" varchar(2) NOT NULL default '00',
  "AAABLRJQ" varchar(4) NOT NULL default '0000',
  "AAABLRJR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRJS","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRKV" varchar(3) NOT NULL default '000',
  "AAABLRJY" varchar(1) NOT NULL default ' ',
  "AAABLRJZ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRKV" varchar(3) NOT NULL default '000',
  "AAABMDBL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRKV","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRKV" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLRKV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLRLL" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMDBL" varchar(4) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAABLRLG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLWZD","AAABMLCZ","AAABLRLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRLM" (
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLRLV" varchar(4) NOT NULL default ' ',
  "AAABLRLW" varchar(4) NOT NULL default ' ',
  "AAABLRLX" varchar(1) NOT NULL default ' ',
  "AAABLRLY" varchar(1) NOT NULL default ' ',
  "AAABLRLZ" varchar(8) NOT NULL default '00000000',
  "AAABLRMA" varchar(1) NOT NULL default ' ',
  "AAABLRMB" varchar(8) NOT NULL default '00000000',
  "AAABLRMC" varchar(1) NOT NULL default ' ',
  "AAABLRMD" varchar(1) NOT NULL default ' ',
  "AAABLRME" varchar(8) NOT NULL default '00000000',
  "AAABLRMF" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLSKS","AAABLSKR","AAABLRMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRMX" varchar(35) NOT NULL default ' ',
  "AAABLRMY" varchar(35) NOT NULL default ' ',
  "AAABLRMZ" varchar(18) NOT NULL default ' ',
  "AAABLRNA" varchar(35) NOT NULL default ' ',
  "AAABLRNB" varchar(8) NOT NULL default '00000000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRMX","AAABLRMY","AAABLRMZ","AAABLRNA","AAABLRNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABLRNM" varchar(8) NOT NULL default '00000000',
  "AAABLRNN" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLURE","AAABLRNM","AAABLRNN","AAABLRNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROU" varchar(22) NOT NULL default ' ',
  "AAABLROX" varchar(10) NOT NULL default ' ',
  "AAABLRNT" varchar(1) NOT NULL default ' ',
  "AAABLZKI" varchar(8) NOT NULL default '00000000',
  "AAABLRNV" varchar(6) NOT NULL default '000000',
  "AAABLRNW" varchar(8) NOT NULL default '00000000',
  "AAABLRNX" varchar(6) NOT NULL default '000000',
  "AAABLRNY" varchar(8) NOT NULL default '00000000',
  "AAABLRNZ" varchar(6) NOT NULL default '000000',
  "AAABLROA" varchar(8) NOT NULL default '00000000',
  "AAABLROB" varchar(6) NOT NULL default '000000',
  "AAABLROC" varchar(10) NOT NULL default '0000000000',
  "AAABLROD" varchar(1) NOT NULL default ' ',
  "AAABLROE" varchar(10) NOT NULL default ' ',
  "AAABLROF" varchar(20) NOT NULL default ' ',
  "AAABLROG" varchar(10) NOT NULL default ' ',
  "AAABLROH" varchar(20) NOT NULL default ' ',
  "AAABLROI" varchar(4) NOT NULL default ' ',
  "AAABLROJ" varchar(20) NOT NULL default ' ',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAABLROL" varchar(20) NOT NULL default ' ',
  "AAABLROM" varchar(6) NOT NULL default ' ',
  "AAABLRON" varchar(20) NOT NULL default ' ',
  "AAABLROO" varchar(9) NOT NULL default ' ',
  "AAABLROP" varchar(20) NOT NULL default ' ',
  "AAABMDCZ" varchar(10) NOT NULL default ' ',
  "AAABMDDF" varchar(10) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROU"),
  KEY "AAABLROV" ("AAABMRIL","AAABLROX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLROY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLRQD" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRQF" varchar(1) NOT NULL default ' ',
  "AAABLRPQ" varchar(20) NOT NULL default ' ',
  "AAABLRPR" varchar(10) NOT NULL default '0000000000',
  "AAABLRPG" varchar(2) NOT NULL default ' ',
  "AAABLRPH" varchar(10) NOT NULL default '0000000000',
  "AAABLRPI" varchar(10) NOT NULL default '0000000000',
  "AAABLRPJ" varchar(2) NOT NULL default ' ',
  "AAABLRPK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABLRQD","AAABMCRJ","AAABLRQF","AAABLRPQ","AAABLRPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLRQD" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRQF" varchar(1) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLRPZ" varchar(12) NOT NULL default ' ',
  "AAABLRQA" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABLRQD","AAABMCRJ","AAABLRQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRQG" (
  "AAABLSNB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLRQJ" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABLRQO" varchar(32) NOT NULL default ' ',
  "AAABLRQP" varchar(32) NOT NULL default ' ',
  "AAABLRQQ" varchar(255) NOT NULL default ' ',
  "AAABLRQV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSNB","AAABLTXI"),
  KEY "AAABLRQU" ("AAABLRQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRSJ" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLRRK" varchar(20) NOT NULL default ' ',
  "AAABLRRL" varchar(132) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRSJ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRSJ" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLRRK" varchar(20) NOT NULL default ' ',
  "AAABLRRL" varchar(132) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRSJ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLRSJ" varchar(20) NOT NULL default ' ',
  "AAABLRRU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRSJ" varchar(20) NOT NULL default ' ',
  "AAABLRSB" varchar(1) NOT NULL default ' ',
  "AAABLRSC" varchar(1) NOT NULL default ' ',
  "AAABLRSD" varchar(1) NOT NULL default ' ',
  "AAABLRSE" varchar(1) NOT NULL default ' ',
  "AAABLRSF" varchar(1) NOT NULL default ' ',
  "AAABLRSG" varchar(1) NOT NULL default ' ',
  "AAABLRSH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
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
CREATE TABLE "AAABLRTC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLRTU" varchar(8) NOT NULL default '00000000',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAABLRTJ" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLRTL" varchar(30) NOT NULL default ' ',
  "AAABLRTM" varchar(2) NOT NULL default ' ',
  "AAABLRTN" varchar(1) NOT NULL default ' ',
  "AAABLRTO" varchar(1) NOT NULL default ' ',
  "AAABLRTP" varchar(2) NOT NULL default '00',
  "AAABLRTQ" varchar(4) NOT NULL default ' ',
  "AAABLRTR" varchar(1) NOT NULL default ' ',
  "AAABLRTS" decimal(8,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABLUXQ","AAABLRTU","AAABLRTV","AAABLRTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRUK" varchar(10) NOT NULL default ' ',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLRWI" varchar(9) NOT NULL default ' ',
  "AAABLRVD" varchar(1) NOT NULL default ' ',
  "AAABLRUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRUK","AAABLXVF","AAABMAWF"),
  KEY "AAABLRUN" ("AAABMRIL","AAABLXVF"),
  KEY "AAABLRUQ" ("AAABLRWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABLRVX" varchar(1) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLRYU" varchar(1) NOT NULL default ' ',
  "AAABLRVC" varchar(1) NOT NULL default ' ',
  "AAABLRVD" varchar(1) NOT NULL default ' ',
  "AAABLRVE" varchar(1) NOT NULL default ' ',
  "AAABLRVF" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLRWA" varchar(10) NOT NULL default ' ',
  "AAABLRWB" varchar(4) NOT NULL default ' ',
  "AAABLRWE" varchar(10) NOT NULL default ' ',
  "AAABLRWI" varchar(9) NOT NULL default ' ',
  "AAABLRWL" varchar(10) NOT NULL default ' ',
  "AAABLRVM" varchar(1) NOT NULL default ' ',
  "AAABLRVN" varchar(4) NOT NULL default ' ',
  "AAABLRVO" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLRVQ" varchar(1) NOT NULL default ' ',
  "AAABLRWN" varchar(10) NOT NULL default ' ',
  "AAABLRWO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLXVF"),
  KEY "AAABLRVV" ("AAABMRIL","AAABLRVX"),
  KEY "AAABLRVY" ("AAABMRIL","AAABLRWA","AAABLRWB"),
  KEY "AAABLRWC" ("AAABMRIL","AAABLRWE","AAABLRWI"),
  KEY "AAABLRWG" ("AAABMRIL","AAABLRWI"),
  KEY "AAABLRWJ" ("AAABMRIL","AAABLRWL"),
  KEY "AAABLRWM" ("AAABLRWN","AAABLRWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYA" varchar(32) NOT NULL default ' ',
  "AAABLRWS" varchar(1) NOT NULL default ' ',
  "AAABLRWT" varchar(20) NOT NULL default ' ',
  "AAABLRWU" varchar(4) NOT NULL default '0000',
  "AAABLRWV" varchar(5) NOT NULL default '00000',
  "AAABLRWW" varchar(1) NOT NULL default ' ',
  "AAABLRWX" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYA" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLRXL" varchar(18) NOT NULL default ' ',
  "AAABLRXM" varchar(1) NOT NULL default ' ',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLRYA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYA" varchar(32) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLRXY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYA","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRYJ" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLRYG" varchar(10) NOT NULL default ' ',
  "AAABLRYH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABLRZI" varchar(18) NOT NULL default ' ',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAABLRYU" varchar(1) NOT NULL default ' ',
  "AAABLRYV" varchar(1) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABLRYY" varchar(1) NOT NULL default ' ',
  "AAABLRYZ" varchar(1) NOT NULL default ' ',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABLRZB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXVF","AAABLRZI","AAABMAWF"),
  KEY "AAABLRZG" ("AAABMRIL","AAABLRZI"),
  KEY "AAABLRZJ" ("AAABMRIL","AAABLRZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRZO" varchar(1) NOT NULL default ' ',
  "AAABLRZP" varchar(1) NOT NULL default ' ',
  "AAABLRZQ" int(11) NOT NULL default '0',
  "AAABLRZR" int(11) NOT NULL default '0',
  "AAABLRZS" varchar(1) NOT NULL default ' ',
  "AAABLRZT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLRZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVF" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXVF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOG","AAABLSAK","AAABLSAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSAM" (
  "AAABLSNB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLSAX" varchar(128) NOT NULL default ' ',
  "AAABLSAY" varchar(4) NOT NULL default ' ',
  "AAABLSAZ" varchar(30) NOT NULL default ' ',
  "AAABLSBA" varchar(4) NOT NULL default '0000',
  "AAABLSAT" varchar(128) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSNB","AAABLTXI","AAABLSAX","AAABLSAY","AAABLSAZ","AAABLSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLSBE" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAABLSBY" longtext,
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSBT" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAABLSBY" longtext,
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSCA" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLSCW" varchar(8) NOT NULL default '00000000',
  "AAABLSCL" varchar(6) NOT NULL default '000000',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLSCO" varchar(1) NOT NULL default ' ',
  "AAABLSCP" varchar(1) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSEB"),
  KEY "AAABLSCU" ("AAABMCVI","AAABLSCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLSEN" varchar(8) NOT NULL default '00000000',
  "AAABLSEI" varchar(6) NOT NULL default '000000',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABLSEK" varchar(4) NOT NULL default '0000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABLSDK" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABLUZZ" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLSDO" varchar(1) NOT NULL default ' ',
  "AAABMCVL" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVM" varchar(3) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLSDT" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABLSDZ" varchar(1) NOT NULL default ' ',
  "AAABLSEA" varchar(18) NOT NULL default ' ',
  "AAABLSEB" varchar(16) NOT NULL default '0000000000000000',
  "AAABLSEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ","AAABMCEO","AAABLSEN","AAABLSEI","AAABMCIM","AAABLSEK"),
  KEY "AAABLSEL" ("AAABMRIL","AAABLSEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLSJM" varchar(5) NOT NULL default '00000',
  "AAABLSEX" decimal(10,0) NOT NULL default '0',
  "AAABLSEY" decimal(10,0) NOT NULL default '0',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSKC","AAABLSKD","AAABLSFX","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSFX" varchar(10) NOT NULL default ' ',
  "AAABLSMV" varchar(18) NOT NULL default ' ',
  "AAABLSGZ" varchar(3) NOT NULL default '000',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  "AAABLSGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSKC","AAABLSKD","AAABLSFX","AAABLSMV","AAABLSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABLSGZ" varchar(3) NOT NULL default '000',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSJR","AAABLSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSGY" varchar(1) NOT NULL default ' ',
  "AAABLSGZ" varchar(3) NOT NULL default '000',
  "AAABLSGS" varchar(8) NOT NULL default '00000000',
  "AAABLSGT" varchar(1) NOT NULL default ' ',
  "AAABLSGU" varchar(6) NOT NULL default '000000',
  "AAABLSGV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSGY","AAABLSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSHL" varchar(4) NOT NULL default ' ',
  "AAABLSHM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSKC","AAABLSKD","AAABLSHL","AAABLSHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSLS" varchar(18) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  "AAABLSHT" varchar(10) NOT NULL default ' ',
  "AAABLSHU" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLSHY" varchar(1) NOT NULL default ' ',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLSLS","AAABLSKR","AAABLSKS","AAABLSKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSJM" varchar(5) NOT NULL default '00000',
  "AAABMCMG" varchar(22) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(73) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSJM","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSJM" varchar(5) NOT NULL default '00000',
  "AAABLSJA" varchar(5) NOT NULL default '00000',
  "AAABLSLQ" varchar(10) NOT NULL default ' ',
  "AAABLSJC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSJR" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABLSJM" varchar(5) NOT NULL default '00000',
  "AAABLSJN" decimal(10,0) NOT NULL default '0',
  "AAABLSJO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSJR","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSKB" varchar(10) NOT NULL default '0000000000',
  "AAABLSKC" varchar(10) NOT NULL default ' ',
  "AAABLSKD" varchar(2) NOT NULL default '00',
  "AAABLSJY" varchar(20) NOT NULL default ' ',
  "AAABLSJZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLSKB","AAABLSKC","AAABLSKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLSKI" decimal(19,2) NOT NULL default '0.00',
  "AAABLSKJ" decimal(19,2) NOT NULL default '0.00',
  "AAABLSKK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSLS" varchar(18) NOT NULL default ' ',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAABLSKT" varchar(2) NOT NULL default ' ',
  "AAABLSKU" varchar(22) NOT NULL default ' ',
  "AAABLSKV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSMV" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSMV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSLS" varchar(18) NOT NULL default ' ',
  "AAABLSMV" varchar(18) NOT NULL default ' ',
  "AAABLSLK" varchar(18) NOT NULL default ' ',
  "AAABLSLL" varchar(10) NOT NULL default '0000000000',
  "AAABLSLM" varchar(1) NOT NULL default ' ',
  "AAABLSLN" varchar(10) NOT NULL default '0000000000',
  "AAABLSLO" varchar(10) NOT NULL default '0000000000',
  "AAABLSLP" varchar(10) NOT NULL default '0000000000',
  "AAABLSLQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSLS","AAABLSMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSMV" varchar(18) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLSMA" decimal(11,2) NOT NULL default '0.00',
  "AAABLSMB" decimal(11,2) NOT NULL default '0.00',
  "AAABLSMC" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLSME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSMV","AAABLURS","AAABMCVI","AAABMCSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSMV" varchar(18) NOT NULL default ' ',
  "AAABLSMW" varchar(18) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAABLSMP" varchar(4) NOT NULL default '0000',
  "AAABLSMQ" varchar(4) NOT NULL default '0000',
  "AAABLSMR" varchar(4) NOT NULL default '0000',
  "AAABLSMS" varchar(4) NOT NULL default '0000',
  "AAABMPTM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLSMV","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSMX" (
  "AAABLSNB" varchar(30) NOT NULL default ' ',
  "AAABLSND" varchar(250) NOT NULL default ' ',
  "AAABLSNA" varchar(37) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSNB"),
  KEY "AAABLSNC" ("AAABLSND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSNU" varchar(12) NOT NULL default ' ',
  "AAABLSNV" varchar(4) NOT NULL default ' ',
  "AAABLSNW" int(11) NOT NULL default '0',
  "AAABLSNX" int(11) NOT NULL default '0',
  "AAABLSNK" varchar(1) NOT NULL default ' ',
  "AAABLSNL" varchar(64) NOT NULL default ' ',
  "AAABLSNM" varchar(2) NOT NULL default ' ',
  "AAABLSNN" varchar(20) NOT NULL default ' ',
  "AAABLSNO" varchar(20) NOT NULL default ' ',
  "AAABLSNP" varchar(1) NOT NULL default ' ',
  "AAABLSNQ" varchar(1) NOT NULL default ' ',
  "AAABLSNR" varchar(2) NOT NULL default ' ',
  "AAABLSNS" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSNU","AAABLSNV","AAABLSNW","AAABLSNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLSOC" varchar(1) NOT NULL default ' ',
  "AAABLSOD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSOF","AAABLSOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPLT","AAABLTHR","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSTP" varchar(10) NOT NULL default ' ',
  "AAABLSTQ" varchar(6) NOT NULL default '000000',
  "AAABLSTR" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABLSQW" varchar(4) NOT NULL default '0000',
  "AAABLSTE" varchar(22) NOT NULL default ' ',
  "AAABLSTB" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSRB" decimal(13,3) NOT NULL default '0.000',
  "AAABLSRC" varchar(3) NOT NULL default ' ',
  "AAABLSTZ" varchar(10) NOT NULL default '0000000000',
  "AAABLSUA" varchar(18) NOT NULL default ' ',
  "AAABLSUB" varchar(3) NOT NULL default ' ',
  "AAABLTAI" varchar(4) NOT NULL default ' ',
  "AAABLSUD" varchar(1) NOT NULL default ' ',
  "AAABLSUE" varchar(1) NOT NULL default ' ',
  "AAABLSUF" varchar(1) NOT NULL default ' ',
  "AAABLSRK" varchar(35) NOT NULL default ' ',
  "AAABLSUQ" varchar(4) NOT NULL default ' ',
  "AAABLSTA" varchar(2) NOT NULL default ' ',
  "AAABLSRN" varchar(1) NOT NULL default ' ',
  "AAABLSRO" int(11) NOT NULL default '0',
  "AAABLSRP" varchar(2) NOT NULL default ' ',
  "AAABLSRQ" varchar(8) NOT NULL default '00000000',
  "AAABLSRR" varchar(8) NOT NULL default '00000000',
  "AAABLSRS" varchar(8) NOT NULL default '00000000',
  "AAABLSRT" varchar(16) NOT NULL default '0000000000000000',
  "AAABLSRU" varchar(10) NOT NULL default ' ',
  "AAABLSRV" varchar(5) NOT NULL default '00000',
  "AAABLSRW" varchar(4) NOT NULL default ' ',
  "AAABLSRX" varchar(4) NOT NULL default ' ',
  "AAABLSRY" varchar(10) NOT NULL default ' ',
  "AAABLSRZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLSSA" varchar(10) NOT NULL default ' ',
  "AAABLSSB" varchar(35) NOT NULL default ' ',
  "AAABLSSC" varchar(1) NOT NULL default ' ',
  "AAABLSSD" varchar(1) NOT NULL default ' ',
  "AAABLSSE" varchar(1) NOT NULL default ' ',
  "AAABLSSF" varchar(1) NOT NULL default ' ',
  "AAABLSSG" varchar(1) NOT NULL default ' ',
  "AAABLSSH" varchar(1) NOT NULL default ' ',
  "AAABLSSI" varchar(1) NOT NULL default ' ',
  "AAABLSSJ" varchar(1) NOT NULL default ' ',
  "AAABLSSK" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLSSM" varchar(8) NOT NULL default '00000000',
  "AAABLSSN" varchar(6) NOT NULL default '000000',
  "AAABLSSO" varchar(12) NOT NULL default ' ',
  "AAABLSSP" varchar(8) NOT NULL default '00000000',
  "AAABLSSQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLSTP","AAABLSTQ","AAABLSTR","AAABMRCU"),
  KEY "AAABLSSW" ("AAABMLCZ","AAABLSTA"),
  KEY "AAABLSSZ" ("AAABLSTA","AAABLSTB"),
  KEY "AAABLSTC" ("AAABMRIL","AAABLSTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSTP" varchar(10) NOT NULL default ' ',
  "AAABLSTQ" varchar(6) NOT NULL default '000000',
  "AAABLSTR" varchar(4) NOT NULL default '0000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSTU" varchar(4) NOT NULL default '0000',
  "AAABLSTN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSTP","AAABLSTQ","AAABLSTR","AAABMRCU","AAABMRHS","AAABLSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABLSTZ" varchar(10) NOT NULL default '0000000000',
  "AAABLSUA" varchar(18) NOT NULL default ' ',
  "AAABLSUB" varchar(3) NOT NULL default ' ',
  "AAABLTAI" varchar(4) NOT NULL default ' ',
  "AAABLSUD" varchar(1) NOT NULL default ' ',
  "AAABLSUE" varchar(1) NOT NULL default ' ',
  "AAABLSUF" varchar(1) NOT NULL default ' ',
  "AAABLSUG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLTAI" varchar(4) NOT NULL default ' ',
  "AAABLSUQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMPLT","AAABLTAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMPLT","AAABLTHR","AAABLTNT","AAABLTNU","AAABLTHM","AAABLTHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLTHQ" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTHM","AAABLTHN","AAABMREX","AAABMPLT","AAABLTHQ","AAABLTHR","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLSZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABLTAI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURS","AAABMCSR","AAABLURT","AAABLTAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSR","AAABLTHR","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSR","AAABLTHR","AAABLTNT","AAABLTNU","AAABLTHM","AAABLTHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLTHQ" varchar(10) NOT NULL default ' ',
  "AAABLTHR" varchar(4) NOT NULL default '0000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLTFP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTFZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGK" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGL" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGM" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGN" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGO" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGT" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGU" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGV" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGW" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGX" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGY" decimal(15,2) NOT NULL default '0.00',
  "AAABLTGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHA" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHB" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHC" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHE" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHF" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHG" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHH" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHI" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHJ" decimal(15,2) NOT NULL default '0.00',
  "AAABLTHK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTHM","AAABLTHN","AAABMREX","AAABMCSR","AAABLTHQ","AAABLTHR","AAABLTNT","AAABLTNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLTHZ" varchar(1) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX"),
  KEY "AAABLTIE" ("AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  "AAABLTIX" varchar(20) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMAMP","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default '0',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTKM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTLR","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLTLM" varchar(6) NOT NULL default '000000',
  "AAABLTLN" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABLTLX" varchar(1) NOT NULL default ' ',
  "AAABLTLY" varchar(1) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTMA" varchar(2) NOT NULL default ' ',
  "AAABLTMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMAMP","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTMR" varchar(1) NOT NULL default ' ',
  "AAABLTMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLTOK" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX","AAABLTOK","AAABMOHM","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLTOK" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX","AAABLTOK","AAABMCSR","AAABMOHM","AAABMCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(8) NOT NULL default ' ',
  "AAABLTOK" varchar(10) NOT NULL default ' ',
  "AAABLTOL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTOK","AAABLTOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTOM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLTPQ" varchar(10) NOT NULL default ' ',
  "AAABLTPE" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(60) NOT NULL default ' ',
  "AAABLTOS" varchar(1) NOT NULL default ' ',
  "AAABLTOT" varchar(32) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLTOV" int(11) NOT NULL default '0',
  "AAABLTOW" varchar(35) NOT NULL default ' ',
  "AAABLTOX" int(11) NOT NULL default '0',
  "AAABLTOY" varchar(35) NOT NULL default ' ',
  "AAABLTOZ" int(11) NOT NULL default '0',
  "AAABLTPA" varchar(35) NOT NULL default ' ',
  "AAABLTPB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMONG","AAABLTPQ","AAABLTPE","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTPQ" varchar(10) NOT NULL default ' ',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTPM" varchar(6) NOT NULL default ' ',
  "AAABLTPN" varchar(1) NOT NULL default ' ',
  "AAABLTPO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLTUT" varchar(6) NOT NULL default ' ',
  "AAABLTUU" varchar(6) NOT NULL default '000000',
  "AAABLTUZ" varchar(8) NOT NULL default '00000000',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLTQC" varchar(1) NOT NULL default ' ',
  "AAABLTQD" varchar(1) NOT NULL default ' ',
  "AAABLTQE" varchar(1) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAABLTQH" varchar(6) NOT NULL default '000000',
  "AAABLTQI" varchar(2) NOT NULL default '00',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABLTQK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABLTQM" varchar(2) NOT NULL default '00',
  "AAABLTQN" varchar(1) NOT NULL default '0',
  "AAABLTQO" varchar(1) NOT NULL default ' ',
  "AAABLTQP" varchar(3) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMJVD" varchar(75) NOT NULL default ' ',
  "AAABLTQS" varchar(2) NOT NULL default '00',
  "AAABLTQT" varchar(1) NOT NULL default '0',
  "AAABLTQU" varchar(1) NOT NULL default ' ',
  "AAABLTQV" varchar(3) NOT NULL default ' ',
  "AAABLTQW" varchar(1) NOT NULL default ' ',
  "AAABMJVE" varchar(75) NOT NULL default ' ',
  "AAABLTQY" varchar(2) NOT NULL default '00',
  "AAABLTQZ" varchar(1) NOT NULL default '0',
  "AAABLTRA" varchar(1) NOT NULL default ' ',
  "AAABLTRB" varchar(3) NOT NULL default ' ',
  "AAABLTRC" varchar(1) NOT NULL default ' ',
  "AAABMJVF" varchar(75) NOT NULL default ' ',
  "AAABLTRE" varchar(2) NOT NULL default '00',
  "AAABLTRF" varchar(1) NOT NULL default '0',
  "AAABLTRG" varchar(1) NOT NULL default ' ',
  "AAABLTRH" varchar(3) NOT NULL default ' ',
  "AAABLTRI" varchar(1) NOT NULL default ' ',
  "AAABMJVG" varchar(75) NOT NULL default ' ',
  "AAABLTRK" varchar(2) NOT NULL default '00',
  "AAABLTRL" varchar(1) NOT NULL default '0',
  "AAABLTRM" varchar(1) NOT NULL default ' ',
  "AAABLTRN" varchar(3) NOT NULL default ' ',
  "AAABLTRO" varchar(1) NOT NULL default ' ',
  "AAABMJVH" varchar(75) NOT NULL default ' ',
  "AAABLTRQ" varchar(2) NOT NULL default '00',
  "AAABLTRR" varchar(1) NOT NULL default '0',
  "AAABLTRS" varchar(1) NOT NULL default ' ',
  "AAABLTRT" varchar(3) NOT NULL default ' ',
  "AAABLTRU" varchar(1) NOT NULL default ' ',
  "AAABLTRV" varchar(75) NOT NULL default ' ',
  "AAABLTRW" varchar(2) NOT NULL default '00',
  "AAABLTRX" varchar(1) NOT NULL default '0',
  "AAABLTRY" varchar(1) NOT NULL default ' ',
  "AAABLTRZ" varchar(3) NOT NULL default ' ',
  "AAABLTSA" varchar(1) NOT NULL default ' ',
  "AAABLTSB" varchar(75) NOT NULL default ' ',
  "AAABLTSC" varchar(2) NOT NULL default '00',
  "AAABLTSD" varchar(1) NOT NULL default '0',
  "AAABLTSE" varchar(1) NOT NULL default ' ',
  "AAABLTSF" varchar(3) NOT NULL default ' ',
  "AAABLTSG" varchar(1) NOT NULL default ' ',
  "AAABLTSH" varchar(75) NOT NULL default ' ',
  "AAABLTSI" varchar(2) NOT NULL default '00',
  "AAABLTSJ" varchar(1) NOT NULL default '0',
  "AAABLTSK" varchar(1) NOT NULL default ' ',
  "AAABLTSL" varchar(3) NOT NULL default ' ',
  "AAABLTSM" varchar(1) NOT NULL default ' ',
  "AAABLTSN" varchar(75) NOT NULL default ' ',
  "AAABLTSO" varchar(2) NOT NULL default '00',
  "AAABLTSP" varchar(1) NOT NULL default '0',
  "AAABLTSQ" varchar(1) NOT NULL default ' ',
  "AAABLTSR" varchar(3) NOT NULL default ' ',
  "AAABLTSS" varchar(1) NOT NULL default ' ',
  "AAABLTST" varchar(75) NOT NULL default ' ',
  "AAABLTSU" varchar(1) NOT NULL default ' ',
  "AAABLTXX" varchar(1) NOT NULL default ' ',
  "AAABLTSW" varchar(1) NOT NULL default ' ',
  "AAABLTSX" varchar(1) NOT NULL default ' ',
  "AAABLTSY" varchar(1) NOT NULL default ' ',
  "AAABLTSZ" varchar(1) NOT NULL default ' ',
  "AAABLTTA" varchar(1) NOT NULL default ' ',
  "AAABLTTB" varchar(1) NOT NULL default ' ',
  "AAABLTTC" varchar(1) NOT NULL default ' ',
  "AAABLTTD" varchar(1) NOT NULL default ' ',
  "AAABLTTE" varchar(1) NOT NULL default ' ',
  "AAABLTTF" varchar(1) NOT NULL default ' ',
  "AAABLTTG" varchar(1) NOT NULL default ' ',
  "AAABLTTH" varchar(1) NOT NULL default ' ',
  "AAABLTTI" varchar(1) NOT NULL default ' ',
  "AAABLTTJ" varchar(1) NOT NULL default ' ',
  "AAABLTTK" varchar(1) NOT NULL default ' ',
  "AAABLTTL" varchar(1) NOT NULL default ' ',
  "AAABLTTM" varchar(1) NOT NULL default ' ',
  "AAABLTTN" varchar(1) NOT NULL default ' ',
  "AAABLTTO" varchar(1) NOT NULL default ' ',
  "AAABLTTP" varchar(1) NOT NULL default ' ',
  "AAABLTTQ" varchar(1) NOT NULL default ' ',
  "AAABLTTR" varchar(1) NOT NULL default ' ',
  "AAABLTTS" varchar(1) NOT NULL default ' ',
  "AAABLTTT" varchar(1) NOT NULL default ' ',
  "AAABLTTU" varchar(1) NOT NULL default ' ',
  "AAABLTTV" varchar(1) NOT NULL default ' ',
  "AAABLTTW" varchar(1) NOT NULL default ' ',
  "AAABLTTX" varchar(1) NOT NULL default ' ',
  "AAABLTTY" varchar(1) NOT NULL default ' ',
  "AAABLTTZ" varchar(1) NOT NULL default ' ',
  "AAABLTUA" varchar(1) NOT NULL default ' ',
  "AAABLTUB" varchar(1) NOT NULL default ' ',
  "AAABLTUC" varchar(2) NOT NULL default '00',
  "AAABLTUD" varchar(2) NOT NULL default '00',
  "AAABLTUE" varchar(2) NOT NULL default '00',
  "AAABLTUF" varchar(2) NOT NULL default '00',
  "AAABLTUG" varchar(2) NOT NULL default '00',
  "AAABLTUH" varchar(2) NOT NULL default '00',
  "AAABLTUI" varchar(2) NOT NULL default '00',
  "AAABLTUJ" varchar(2) NOT NULL default '00',
  "AAABLTUK" varchar(2) NOT NULL default '00',
  "AAABLTUL" varchar(2) NOT NULL default '00',
  "AAABLTUM" varchar(2) NOT NULL default '00',
  "AAABLTUN" varchar(46) NOT NULL default ' ',
  "AAABLTUO" varchar(5) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABLTUT","AAABLTUU"),
  KEY "AAABLTUV" ("AAABMRIL","AAABLTUX","AAABMLPV","AAABLTUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVF" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLTVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTVG" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMOHM","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVQ" varchar(40) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLTVQ","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVY" VARBINARY(255) NOT NULL,
  "AAABLTVZ" varchar(8) NOT NULL default '00000000',
  "AAABLTWA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVY"(255),"AAABLTVZ","AAABLTWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTWB" (
  "AAABLTWR" varchar(20) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABLTWQ" int(11) NOT NULL default '0',
  "AAABLTWK" varchar(40) NOT NULL default ' ',
  "AAABLTWM" varchar(40) NOT NULL default ' ',
  "AAABLTWH" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTWR"),
  KEY "AAABLTWJ" ("AAABLTWK"),
  KEY "AAABLTWL" ("AAABLTWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTWN" (
  "AAABLTWR" varchar(20) NOT NULL default ' ',
  "AAABLTWP" varchar(20) NOT NULL default ' ',
  "AAABLTWQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLTWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXII" varchar(10) NOT NULL default ' ',
  "AAABLTXD" varchar(6) NOT NULL default '000000',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(24) NOT NULL default ' ',
  "AAABLTXI" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLXII","AAABLTXD","AAABLTXE","AAABLTXF","AAABLTXG","AAABLUNJ","AAABLTXI"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXZ" varchar(30) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  "AAABLTXN" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(1) NOT NULL default ' ',
  "AAABMKAH" varchar(3) NOT NULL default ' ',
  "AAABLTXQ" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABLTXS" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default ' ',
  "AAABLTXU" varchar(60) NOT NULL default ' ',
  "AAABLTXV" varchar(4) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABLTXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXZ","AAABLTYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(30) NOT NULL default ' ',
  "AAABLTYG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTYY" VARBINARY(255) NOT NULL,
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(8) NOT NULL default '00000000',
  "AAABLTZD" VARBINARY(255),
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABLTYR" varchar(6) NOT NULL default ' ',
  "AAABLTYS" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  "AAABLTYV" varchar(8) NOT NULL default '00000000',
  "AAABLTYW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTYY"(255),"AAABMALG","AAABMAKT","AAABMLPO"),
  KEY "AAABLTZC" ("AAABLTZD"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAI" VARBINARY(255) NOT NULL,
  "AAABLTZT" VARBINARY(255) NOT NULL,
  "AAABLTZR" VARBINARY(255) NOT NULL,
  "AAABLUAB" varchar(4) NOT NULL default ' ',
  "AAABLTZK" varchar(40) NOT NULL default ' ',
  "AAABLTZW" varchar(40) NOT NULL default ' ',
  "AAABLTZM" varchar(8) NOT NULL default '00000000',
  "AAABLTZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAI"(255),"AAABLTZT"(255),"AAABLTZR"(255)),
  KEY "AAABLTZS" ("AAABLTZT"(255)),
  KEY "AAABLTZU" ("AAABLUAB","AAABLTZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAI" VARBINARY(255) NOT NULL,
  "AAABLUSV" varchar(5) NOT NULL default '00000',
  "AAABLUAB" varchar(4) NOT NULL default ' ',
  "AAABLUAC" varchar(5) NOT NULL default ' ',
  "AAABLUAD" varchar(1) NOT NULL default ' ',
  "AAABLUAE" varchar(2) NOT NULL default ' ',
  "AAABLUAF" varchar(40) NOT NULL default ' ',
  "AAABLUAG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAI"(255),"AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUAK" (
  "AAABLUAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUAN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUAS" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUAT" (
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUAW" (
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUAZ" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUBI" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUER","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUBQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUBW" (
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUBZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEB","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUCC" (
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEB","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUCH" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUCJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUCL" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEU" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUCR" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABLUEB","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUDD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUDJ" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABLUEB","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUDT" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEB" varchar(30) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABLUEB","AAABLUEQ","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUEE" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAABLUEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUEM" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLUEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUES" (
  "AAABLUEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHN" varchar(10) NOT NULL default ' ',
  "AAABLUHO" varchar(5) NOT NULL default '00000',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLUFJ" varchar(12) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLUFL" decimal(9,5) NOT NULL default '0.00000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAABLUFO" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUHX" varchar(10) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLUFU" varchar(35) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAABLUFY" varchar(3) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLUGB" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLUGE" varchar(1) NOT NULL default ' ',
  "AAABLUGF" varchar(10) NOT NULL default '0000000000',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABLUGK" varchar(1) NOT NULL default ' ',
  "AAABLUGL" varchar(1) NOT NULL default ' ',
  "AAABLUGM" varchar(1) NOT NULL default ' ',
  "AAABLUGN" varchar(1) NOT NULL default ' ',
  "AAABLUGO" varchar(1) NOT NULL default ' ',
  "AAABLUGP" varchar(1) NOT NULL default ' ',
  "AAABLUGQ" varchar(20) NOT NULL default ' ',
  "AAABLUGR" varchar(20) NOT NULL default ' ',
  "AAABLUGS" varchar(10) NOT NULL default '0000000000',
  "AAABLUGT" varchar(10) NOT NULL default '0000000000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABLUGX" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAABLUHF" varchar(10) NOT NULL default ' ',
  "AAABLUHG" varchar(5) NOT NULL default '00000',
  "AAABLUHH" varchar(70) NOT NULL default ' ',
  "AAABLUHI" varchar(6) NOT NULL default '000000',
  "AAABLUHJ" varchar(10) NOT NULL default ' ',
  "AAABLUHK" varchar(1) NOT NULL default ' ',
  "AAABLUHL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLUHN","AAABLUHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHX" varchar(10) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABLUHV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHX","AAABLUHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUJO" varchar(10) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLUIH" varchar(4) NOT NULL default ' ',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABLUIJ" varchar(8) NOT NULL default '00000000',
  "AAABLUIK" varchar(8) NOT NULL default '00000000',
  "AAABLUIL" varchar(1) NOT NULL default ' ',
  "AAABLUIM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUJO" varchar(10) NOT NULL default ' ',
  "AAABLUJH" varchar(6) NOT NULL default '000000',
  "AAABLUIT" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUIZ" decimal(10,3) NOT NULL default '0.000',
  "AAABLUJA" decimal(10,3) NOT NULL default '0.000',
  "AAABLUJB" varchar(10) NOT NULL default ' ',
  "AAABLUJC" varchar(5) NOT NULL default '00000',
  "AAABLUJD" varchar(1) NOT NULL default ' ',
  "AAABLUJE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLUJO","AAABLUJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUJO" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUJM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUJO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLULY" varchar(10) NOT NULL default ' ',
  "AAABLUJT" varchar(2) NOT NULL default ' ',
  "AAABLWZD" varchar(10) NOT NULL default ' ',
  "AAABLUKQ" varchar(10) NOT NULL default ' ',
  "AAABLUJW" varchar(3) NOT NULL default ' ',
  "AAABLUJX" varchar(1) NOT NULL default ' ',
  "AAABLXOT" varchar(8) NOT NULL default '00000000',
  "AAABLUJZ" varchar(8) NOT NULL default '00000000',
  "AAABLUKA" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLUKF" varchar(8) NOT NULL default '00000000',
  "AAABLUKG" varchar(5) NOT NULL default ' ',
  "AAABLUKH" varchar(1) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLUKJ" varchar(10) NOT NULL default ' ',
  "AAABLUKK" varchar(4) NOT NULL default ' ',
  "AAABLUKT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLULY"),
  KEY "AAABLUKO" ("AAABMRIL","AAABLUKQ"),
  KEY "AAABLUKR" ("AAABMRIL","AAABLUKT"),
  KEY "AAABLUKU" ("AAABMRIL","AAABLWZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLULY" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLULB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLULY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLULF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLULY" varchar(10) NOT NULL default ' ',
  "AAABLULZ" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLULL" varchar(18) NOT NULL default ' ',
  "AAABLULM" varchar(10) NOT NULL default '0000000000',
  "AAABLULN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLULY","AAABLULZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLULR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLULY" varchar(10) NOT NULL default ' ',
  "AAABLULZ" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLULW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLULY","AAABLULZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUMB" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUMF" varchar(1) NOT NULL default ' ',
  "AAABLUMG" varchar(1) NOT NULL default ' ',
  "AAABLUMH" varchar(12) NOT NULL default ' ',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABLUMJ" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLUNJ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUMQ" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUMT" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABMMJT" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNI","AAABLUNJ","AAABMLQA","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUND" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABMMJT" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNI","AAABLUNJ","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUNL" (
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLURT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABLUOC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCSR","AAABLURT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABLUOY" varchar(30) NOT NULL default ' ',
  "AAABLUOZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLUSC","AAABLUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABLUOY" varchar(30) NOT NULL default ' ',
  "AAABLUOZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABMLCZ","AAABLUSC","AAABLUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLUPZ" varchar(3) NOT NULL default '000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLUPP" varchar(10) NOT NULL default ' ',
  "AAABLUPQ" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLURS","AAABLURT","AAABMCVI","AAABMALG","AAABLUPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAABLUQH" varchar(1) NOT NULL default ' ',
  "AAABLUQI" varchar(1) NOT NULL default ' ',
  "AAABLUQJ" varchar(1) NOT NULL default ' ',
  "AAABLUQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLURS","AAABLURT","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUQY" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLURB" varchar(3) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLURD" decimal(3,0) NOT NULL default '0',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABLURF" varchar(3) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURH" varchar(3) NOT NULL default ' ',
  "AAABLURI" varchar(4) NOT NULL default ' ',
  "AAABLURJ" varchar(4) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  "AAABLURL" varchar(4) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAABLURN" varchar(4) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAABLURP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAABLURS","AAABLURT","AAABMCVI","AAABMRFQ","AAABMLES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLURX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABLUSA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUSD" (
  "AAABLUSR" varchar(120) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABLUSR","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUSL" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUSS" (
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABLUSU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUSW" (
  "AAABLUTA" varchar(4) NOT NULL default ' ',
  "AAABLUTC" decimal(12,0) NOT NULL default '0',
  "AAABLUSZ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUTA"),
  KEY "AAABLUTB" ("AAABLUTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUTD" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWM" varchar(32) NOT NULL default ' ',
  "AAABLUWN" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWM","AAABLUWN","AAABMOHM","AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUTQ" (
  "AAABLUTU" varchar(8) NOT NULL default ' ',
  "AAABLUTS" varchar(80) NOT NULL default ' ',
  "AAABLUTT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUTV" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAABLUUA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUUU","AAABLUUV","AAABLUVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUUE" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWW","AAABLUWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUUP" (
  "AAABLUVB" varchar(10) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABLUVE" int(11) NOT NULL default '0',
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABLUUV" varchar(8) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLUUZ" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVB","AAABMONG","AAABMKFU","AAABLUVE"),
  KEY "AAABLUVF" ("AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUVH" (
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUVK" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUVR" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWW","AAABLUWX","AAABLUWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUVY" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUWM" varchar(32) NOT NULL default ' ',
  "AAABLUWN" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLUWE" varchar(80) NOT NULL default ' ',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  "AAABLUWG" varchar(10) NOT NULL default ' ',
  "AAABLUWH" varchar(3) NOT NULL default ' ',
  "AAABLUWI" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ","AAABLUWM","AAABLUWN","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUWP" (
  "AAABLUXP" varchar(10) NOT NULL default '0000000000',
  "AAABLUWW" varchar(8) NOT NULL default ' ',
  "AAABLUWX" varchar(8) NOT NULL default ' ',
  "AAABLUWY" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUWW","AAABLUWX","AAABLUWY","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUXA" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUXD" varchar(4) NOT NULL default ' ',
  "AAABLUXE" varchar(4) NOT NULL default ' ',
  "AAABLUXF" varchar(1) NOT NULL default ' ',
  "AAABLUXG" varchar(4) NOT NULL default ' ',
  "AAABLUXH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUXL" (
  "AAABLUXP" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXP","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUXR" (
  "AAABLUXU" varchar(20) NOT NULL default ' ',
  "AAABLUXT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLUZL" varchar(1) NOT NULL default ' ',
  "AAABLUXZ" varchar(1) NOT NULL default ' ',
  "AAABLUYA" varchar(1) NOT NULL default ' ',
  "AAABLUYB" varchar(1) NOT NULL default ' ',
  "AAABLUYC" varchar(1) NOT NULL default ' ',
  "AAABLUYD" varchar(1) NOT NULL default ' ',
  "AAABLUYE" varchar(1) NOT NULL default ' ',
  "AAABLUYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJM" varchar(10) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAABMDGQ" varchar(13) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLUYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJM" varchar(10) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLYAT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYAX" varchar(1) NOT NULL default ' ',
  "AAABLVAM" varchar(3) NOT NULL default '000',
  "AAABLVAN" varchar(8) NOT NULL default '00000000',
  "AAABLUYZ" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUZI" varchar(1) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  "AAABLUZK" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(1) NOT NULL default ' ',
  "AAABLYAB" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLXZQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLUZP" decimal(19,2) NOT NULL default '0.00',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZL" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLUZV" decimal(13,3) NOT NULL default '0.000',
  "AAABLUZW" decimal(15,2) NOT NULL default '0.00',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXZJ" varchar(1) NOT NULL default ' ',
  "AAABLUZZ" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXJM","AAABLYAS","AAABLYAT","AAABMLCZ","AAABLYAX","AAABLVAM","AAABLVAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABLVBI" varchar(30) NOT NULL default ' ',
  "AAABLVBD" varchar(30) NOT NULL default ' ',
  "AAABLVBE" varchar(1) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVBH","AAABLVBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAI" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABLVBS" varchar(1) NOT NULL default ' ',
  "AAABLVBT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAI","AAABMPLS","AAABLZUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXHD" varchar(3) NOT NULL default ' ',
  "AAABLXYX" varchar(18) NOT NULL default ' ',
  "AAABLXHF" varchar(1) NOT NULL default ' ',
  "AAABLXHB" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXHD","AAABLXYX","AAABLXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCL" varchar(8) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABLVCN" varchar(2) NOT NULL default ' ',
  "AAABLVCO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVCQ" (
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCW","AAABMREF","AAABLVCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVDL" varchar(4) NOT NULL default ' ',
  "AAABLVDI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLVDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVFO" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLVFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABLVEJ" varchar(20) NOT NULL default ' ',
  "AAABLVEA" varchar(1) NOT NULL default ' ',
  "AAABLVEB" varchar(2) NOT NULL default '00',
  "AAABLVEC" varchar(2) NOT NULL default '00',
  "AAABLVED" varchar(55) NOT NULL default ' ',
  "AAABLVEE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMOIA","AAABMADG","AAABLVEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREL","AAABMOIA","AAABMADG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABLVFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVGV" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCRM" varchar(10) NOT NULL default ' ',
  "AAABLVGA" varchar(6) NOT NULL default '000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLVGC" varchar(6) NOT NULL default '000000',
  "AAABLVGD" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGE" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGF" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGG" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGH" decimal(15,3) NOT NULL default '0.000',
  "AAABLVJT" varchar(1) NOT NULL default ' ',
  "AAABMLTT" varchar(15) NOT NULL default ' ',
  "AAABLVGK" varchar(10) NOT NULL default ' ',
  "AAABLVGL" varchar(10) NOT NULL default ' ',
  "AAABLVGM" varchar(10) NOT NULL default ' ',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABMLEV" decimal(15,3) NOT NULL default '0.000',
  "AAABLXPY" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGR" decimal(15,3) NOT NULL default '0.000',
  "AAABLVGS" varchar(8) NOT NULL default '00000000',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVGV","AAABMRCU"),
  KEY "AAABLVGX" ("AAABMRIL","AAABMCRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABLVHJ" varchar(35) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHN" varchar(35) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHR" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVHT" varchar(30) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLVHW" varchar(16) NOT NULL default ' ',
  "AAABLVHX" varchar(30) NOT NULL default ' ',
  "AAABLVHY" varchar(15) NOT NULL default ' ',
  "AAABLVHZ" varchar(14) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABLVIC" decimal(12,3) NOT NULL default '0.000',
  "AAABLVID" varchar(20) NOT NULL default ' ',
  "AAABLVIW" tinyint(3) unsigned NOT NULL default '0',
  "AAABLVKB" varchar(4) NOT NULL default ' ',
  "AAABLVIG" varchar(40) NOT NULL default ' ',
  "AAABLVIH" decimal(13,2) NOT NULL default '0.00',
  "AAABMKEO" varchar(4) NOT NULL default ' ',
  "AAABLXTT" varchar(4) NOT NULL default ' ',
  "AAABLVIK" varchar(1) NOT NULL default ' ',
  "AAABLVKX" varchar(2) NOT NULL default ' ',
  "AAABLVIM" varchar(50) NOT NULL default ' ',
  "AAABLVIN" varchar(20) NOT NULL default ' ',
  "AAABLVIO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  "AAABLVIV" tinyint(3) unsigned NOT NULL default '0',
  "AAABLVIW" tinyint(3) unsigned NOT NULL default '0',
  "AAABLVIX" varchar(8) NOT NULL default '00000000',
  "AAABLVIY" varchar(20) NOT NULL default ' ',
  "AAABLVIZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJV" varchar(8) NOT NULL default ' ',
  "AAABLVJF" varchar(20) NOT NULL default ' ',
  "AAABLXOT" varchar(8) NOT NULL default '00000000',
  "AAABLVJH" varchar(4) NOT NULL default ' ',
  "AAABLVJI" varchar(30) NOT NULL default ' ',
  "AAABLVJJ" varchar(20) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABLVJL" varchar(4) NOT NULL default ' ',
  "AAABLVJM" varchar(20) NOT NULL default ' ',
  "AAABLVJN" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(30) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVJQ" varchar(6) NOT NULL default ' ',
  "AAABLVJR" varchar(40) NOT NULL default ' ',
  "AAABLVJS" varchar(10) NOT NULL default ' ',
  "AAABLVJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVKB" varchar(4) NOT NULL default ' ',
  "AAABLVJZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTT" varchar(4) NOT NULL default ' ',
  "AAABLVKF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKEO" varchar(4) NOT NULL default ' ',
  "AAABLVKL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLVKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVKX" varchar(2) NOT NULL default ' ',
  "AAABMJVD" varchar(60) NOT NULL default ' ',
  "AAABMJVE" varchar(60) NOT NULL default ' ',
  "AAABMJVF" varchar(60) NOT NULL default ' ',
  "AAABMJVG" varchar(60) NOT NULL default ' ',
  "AAABMJVH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
