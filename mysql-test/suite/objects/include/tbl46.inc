# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl46.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
--disable_query_log
CREATE TABLE "AAABLDCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDCS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDCS" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDDF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDDF" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDDG" (
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDDJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLDDM" varchar(20) NOT NULL default ' ',
  "AAABLDDN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDEC" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDED" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLDEI" varchar(10) NOT NULL default ' ',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLDEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABLDEM" varchar(24) NOT NULL default '000000000000000000000000',
  "AAABLDEN" varchar(14) NOT NULL default '00000000000000',
  "AAABLDEO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDER" (
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDFF" varchar(30) NOT NULL default ' ',
  "AAABLDEU" varchar(30) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAABLDEW" varchar(1) NOT NULL default ' ',
  "AAABLDEX" varchar(30) NOT NULL default ' ',
  "AAABLDEY" varchar(20) NOT NULL default ' ',
  "AAABLDEZ" varchar(20) NOT NULL default ' ',
  "AAABLDFA" varchar(20) NOT NULL default ' ',
  "AAABLDFB" varchar(20) NOT NULL default ' ',
  "AAABLDFC" int(11) NOT NULL default '0',
  "AAABLDFD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLDXX","AAABLDFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDFG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDUQ" varchar(20) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX","AAABLDUQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDFP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(20) NOT NULL default ' ',
  "AAABLDGN" varchar(50) NOT NULL default ' ',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDFU" varchar(10) NOT NULL default ' ',
  "AAABLDHJ" varchar(22) NOT NULL default ' ',
  "AAABLDRJ" varchar(10) NOT NULL default ' ',
  "AAABLDGV" varchar(80) NOT NULL default ' ',
  "AAABLDGQ" varchar(30) NOT NULL default ' ',
  "AAABLDGR" varchar(80) NOT NULL default ' ',
  "AAABLDGY" varchar(1) NOT NULL default ' ',
  "AAABLDGB" varchar(1) NOT NULL default ' ',
  "AAABLDGC" varchar(1) NOT NULL default ' ',
  "AAABLDGD" varchar(1) NOT NULL default ' ',
  "AAABLDHB" varchar(20) NOT NULL default ' ',
  "AAABLDHC" varchar(50) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  "AAABLDGI" varchar(22) NOT NULL default ' ',
  "AAABLDTX" varchar(22) NOT NULL default ' ',
  "AAABLDGK" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDUQ","AAABLDGN"),
  KEY "AAABLDGO" ("AAABLUXQ","AAABLDGQ","AAABLDGR"),
  KEY "AAABLDGS" ("AAABLUXQ","AAABLDRJ","AAABLDGV"),
  KEY "AAABLDGW" ("AAABLUXQ","AAABLDGY"),
  KEY "AAABLDGZ" ("AAABLUXQ","AAABLDHB","AAABLDHC"),
  KEY "AAABLDHD" ("AAABLUXQ","AAABLDXX","AAABLRPY"),
  KEY "AAABLDHH" ("AAABLUXQ","AAABLDHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDHK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDUQ" varchar(20) NOT NULL default ' ',
  "AAABLDRF" varchar(4) NOT NULL default '0000',
  "AAABLDRG" varchar(1) NOT NULL default ' ',
  "AAABLDHQ" varchar(1) NOT NULL default ' ',
  "AAABLDRJ" varchar(10) NOT NULL default ' ',
  "AAABLDHS" varchar(20) NOT NULL default ' ',
  "AAABLDHT" varchar(1) NOT NULL default ' ',
  "AAABLDHU" varchar(1) NOT NULL default ' ',
  "AAABLDHV" varchar(1) NOT NULL default ' ',
  "AAABLDRK" varchar(30) NOT NULL default ' ',
  "AAABLDRL" varchar(30) NOT NULL default ' ',
  "AAABLDRO" varchar(30) NOT NULL default ' ',
  "AAABLDRP" varchar(30) NOT NULL default ' ',
  "AAABLDRQ" varchar(1) NOT NULL default ' ',
  "AAABLDRR" varchar(20) NOT NULL default ' ',
  "AAABLDRS" varchar(30) NOT NULL default ' ',
  "AAABLDRT" varchar(30) NOT NULL default ' ',
  "AAABLDRU" varchar(1) NOT NULL default ' ',
  "AAABLDRV" varchar(20) NOT NULL default ' ',
  "AAABLDIG" varchar(1) NOT NULL default ' ',
  "AAABLDIH" varchar(30) NOT NULL default ' ',
  "AAABLDII" varchar(1) NOT NULL default ' ',
  "AAABLDIJ" varchar(30) NOT NULL default ' ',
  "AAABLDIK" varchar(30) NOT NULL default ' ',
  "AAABLDIL" varchar(20) NOT NULL default ' ',
  "AAABLDIM" varchar(1) NOT NULL default ' ',
  "AAABLDRW" varchar(1) NOT NULL default ' ',
  "AAABLDRX" varchar(20) NOT NULL default ' ',
  "AAABLDRY" varchar(20) NOT NULL default ' ',
  "AAABLDIQ" varchar(1) NOT NULL default ' ',
  "AAABLDRZ" varchar(1) NOT NULL default ' ',
  "AAABLDIS" varchar(1) NOT NULL default ' ',
  "AAABLDIT" varchar(1) NOT NULL default ' ',
  "AAABLDIU" varchar(30) NOT NULL default ' ',
  "AAABLDIV" varchar(20) NOT NULL default ' ',
  "AAABLDIW" varchar(20) NOT NULL default ' ',
  "AAABLDIX" varchar(20) NOT NULL default ' ',
  "AAABLDIY" varchar(20) NOT NULL default ' ',
  "AAABLDIZ" varchar(20) NOT NULL default ' ',
  "AAABLDJA" varchar(20) NOT NULL default ' ',
  "AAABLDJB" varchar(20) NOT NULL default ' ',
  "AAABLDRI" varchar(10) NOT NULL default ' ',
  "AAABLDJD" varchar(30) NOT NULL default ' ',
  "AAABLDJE" varchar(1) NOT NULL default ' ',
  "AAABLDJF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDJJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDUQ" varchar(20) NOT NULL default ' ',
  "AAABLDJO" varchar(80) NOT NULL default ' ',
  "AAABLFZO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDXX","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDJU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDJZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDKH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLDKV" varchar(20) NOT NULL default ' ',
  "AAABLDLB" varchar(8) NOT NULL default '00000000',
  "AAABLDLC" varchar(6) NOT NULL default '000000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDKQ"),
  KEY "AAABLDKR" ("AAABLUXQ","AAABLZKF","AAABLDLB","AAABLDKV","AAABLDLC"),
  KEY "AAABLDKX" ("AAABLUXQ","AAABLDXX","AAABLZKF","AAABLDLB","AAABLDLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDLD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLDLX" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDWD","AAABLDWE","AAABLDLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDLM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLDLX" varchar(18) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDWD","AAABLDWE","AAABLDLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDLY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLGDB" varchar(32) NOT NULL default ' ',
  "AAABLDNT" varchar(3) NOT NULL default '000',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDWD","AAABLDWE","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDMJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLGDB" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDWD","AAABLDWE","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDMV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDNF" varchar(22) NOT NULL default ' ',
  "AAABLDNI" varchar(20) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default ' ',
  "AAABMLEK" varchar(10) NOT NULL default ' ',
  "AAABLDNL" varchar(16) NOT NULL default ' ',
  "AAABLDNM" varchar(30) NOT NULL default ' ',
  "AAABLDNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDNF"),
  KEY "AAABLDNG" ("AAABLUXQ","AAABLDNI","AAABMLTD","AAABMLEK","AAABLDNL","AAABLDNM","AAABLDNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDNO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABLDOO" varchar(1) NOT NULL default ' ',
  "AAABLGDB" varchar(32) NOT NULL default ' ',
  "AAABLDNT" varchar(3) NOT NULL default '000',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  "AAABLDNV" varchar(1) NOT NULL default ' ',
  "AAABLDNW" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAABLDOO","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDOE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(10) NOT NULL default ' ',
  "AAABLDOO" varchar(1) NOT NULL default ' ',
  "AAABLGDB" varchar(32) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON","AAABLDOO","AAABLGDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDOQ" (
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDXQ" varchar(1) NOT NULL default ' ',
  "AAABLDYF" varchar(1) NOT NULL default ' ',
  "AAABLDUF" varchar(1) NOT NULL default ' ',
  "AAABLDUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDOX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLDXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUY" varchar(1) NOT NULL default ' ',
  "AAABLDUZ" varchar(20) NOT NULL default ' ',
  "AAABLDPU" varchar(1) NOT NULL default ' ',
  "AAABLDPV" varchar(3) NOT NULL default '000',
  "AAABLDPJ" varchar(1) NOT NULL default ' ',
  "AAABLDPK" varchar(255) NOT NULL default ' ',
  "AAABLDPL" varchar(1) NOT NULL default ' ',
  "AAABLDPM" varchar(255) NOT NULL default ' ',
  "AAABLDPN" varchar(1) NOT NULL default ' ',
  "AAABLDPO" varchar(2) NOT NULL default ' ',
  "AAABMNMV" varchar(3) NOT NULL default ' ',
  "AAABLDPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUY","AAABLDUZ","AAABLDPU","AAABLDPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUY" varchar(1) NOT NULL default ' ',
  "AAABLDUZ" varchar(20) NOT NULL default ' ',
  "AAABLDRK" varchar(30) NOT NULL default ' ',
  "AAABLDRL" varchar(30) NOT NULL default ' ',
  "AAABLDRM" varchar(30) NOT NULL default ' ',
  "AAABLDRN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUY","AAABLDUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLDUY" varchar(1) NOT NULL default ' ',
  "AAABLDUZ" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW","AAABLDUY","AAABLDUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDQY" varchar(12) NOT NULL default ' ',
  "AAABLDQZ" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDRA" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDQY","AAABLDQZ","AAABLDRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDRB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDVV" varchar(20) NOT NULL default ' ',
  "AAABLDRF" varchar(4) NOT NULL default '0000',
  "AAABLDRG" varchar(1) NOT NULL default ' ',
  "AAABLDRH" varchar(1) NOT NULL default ' ',
  "AAABLDRI" varchar(10) NOT NULL default ' ',
  "AAABLDRJ" varchar(10) NOT NULL default ' ',
  "AAABLDRK" varchar(30) NOT NULL default ' ',
  "AAABLDRL" varchar(30) NOT NULL default ' ',
  "AAABLDRM" varchar(30) NOT NULL default ' ',
  "AAABLDRN" varchar(30) NOT NULL default ' ',
  "AAABLDRO" varchar(30) NOT NULL default ' ',
  "AAABLDRP" varchar(30) NOT NULL default ' ',
  "AAABLDRQ" varchar(1) NOT NULL default ' ',
  "AAABLDRR" varchar(20) NOT NULL default ' ',
  "AAABLDRS" varchar(30) NOT NULL default ' ',
  "AAABLDRT" varchar(30) NOT NULL default ' ',
  "AAABLDRU" varchar(1) NOT NULL default ' ',
  "AAABLDRV" varchar(20) NOT NULL default ' ',
  "AAABLDRW" varchar(1) NOT NULL default ' ',
  "AAABLDRX" varchar(20) NOT NULL default ' ',
  "AAABLDRY" varchar(20) NOT NULL default ' ',
  "AAABLDRZ" varchar(1) NOT NULL default ' ',
  "AAABLDSA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX","AAABLDVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDSE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDVV" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  "AAABLFZO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDXX","AAABLDVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  "AAABLDSU" varchar(1) NOT NULL default ' ',
  "AAABLDSV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDWD","AAABLDWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDSZ" (
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW","AAABLDWD","AAABLDWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDTN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDTT" varchar(30) NOT NULL default ' ',
  "AAABLDTU" varchar(80) NOT NULL default ' ',
  "AAABLDTX" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDTT","AAABLDTU"),
  UNIQUE KEY "AAABLDTV" ("AAABLUXQ","AAABLDTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDTY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDUB" varchar(1) NOT NULL default ' ',
  "AAABLDXQ" varchar(1) NOT NULL default ' ',
  "AAABLDUD" varchar(1) NOT NULL default ' ',
  "AAABLDYF" varchar(1) NOT NULL default ' ',
  "AAABLDUF" varchar(1) NOT NULL default ' ',
  "AAABLDUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDUJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDUQ" varchar(20) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX","AAABLDUQ","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDUS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDUY" varchar(1) NOT NULL default ' ',
  "AAABLDUZ" varchar(20) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDUY","AAABLDUZ","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDVB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDVG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDVO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDVV" varchar(20) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXX","AAABLDVV","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDVX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDWD" varchar(1) NOT NULL default ' ',
  "AAABLDWE" varchar(20) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDWD","AAABLDWE","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDWG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLNRB" varchar(30) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLNRB","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDWN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXG" varchar(20) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXG","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDWU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDXG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDWZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDXG" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLDXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDXH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLDXN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABLDXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDXO" (
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLDXQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDXS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDXX" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLDXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDXY" (
  "AAABLDYM" varchar(10) NOT NULL default ' ',
  "AAABLDYA" varchar(10) NOT NULL default ' ',
  "AAABLDYB" varchar(7) NOT NULL default ' ',
  "AAABLDYC" varchar(1) NOT NULL default ' ',
  "AAABLDYD" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABLDYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDYH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDYM" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLDYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDYN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDZG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDZG" varchar(10) NOT NULL default ' ',
  "AAABLDZK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDZG","AAABLDZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDYZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDZG" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDZH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEAY" VARBINARY(255) NOT NULL,
  "AAABLDZK" varchar(10) NOT NULL default ' ',
  "AAABLDZP" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAABLEAY"(255)),
  KEY "AAABLDZO" ("AAABLDZP"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLDZQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLEAD" VARBINARY(255),
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLDZW" varchar(32) NOT NULL default ' ',
  "AAABLDZX" varchar(70) NOT NULL default ' ',
  "AAABLDZY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTVR"(255)),
  KEY "AAABLEAB" ("AAABLUXQ","AAABLEAD"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEAE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABLEAH" longtext,
  PRIMARY KEY ("AAABLUXQ","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEAK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEAY" VARBINARY(255) NOT NULL,
  "AAABLEAN" decimal(31,14) NOT NULL default '0.00000000000000',
  "AAABLEAO" varchar(3) NOT NULL default ' ',
  "AAABLEAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEAY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEAS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLEAY" VARBINARY(255) NOT NULL,
  "AAABMMJT" decimal(15,5) NOT NULL default '0.00000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLEAY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEAZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLEBC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEBF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLEBI" varchar(30) NOT NULL default ' ',
  "AAABLEBJ" varchar(30) NOT NULL default ' ',
  "AAABLEBK" decimal(15,0) NOT NULL default '0',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABLEBM" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEBQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEBY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABLECB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLECE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECT" varchar(22) NOT NULL default ' ',
  "AAABLECU" varchar(22) NOT NULL default ' ',
  "AAABLECV" varchar(22) NOT NULL default ' ',
  "AAABLECJ" longtext NOT NULL,
  "AAABMRGO" longtext NOT NULL,
  "AAABMRCK" longtext NOT NULL,
  "AAABLTQA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(6) NOT NULL default ' ',
  "AAABLECO" varchar(22) NOT NULL default ' ',
  "AAABLECP" varchar(22) NOT NULL default ' ',
  "AAABLECQ" varchar(100) NOT NULL default ' ',
  "AAABLECR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECT","AAABLECU","AAABLECV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLECW" (
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAABLENL" varchar(8) NOT NULL default '00000000',
  "AAABLENM" varchar(2) NOT NULL default ' ',
  "AAABLENN" varchar(6) NOT NULL default ' ',
  "AAABLENO" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABLENR" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEDH" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABLEDV" varchar(10) NOT NULL default ' ',
  "AAABLEDW" varchar(10) NOT NULL default ' ',
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAABLEDY" varchar(12) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABLTJH","AAABLMNC","AAABLEDV","AAABLEDW","AAABLLNR","AAABLEDY","AAABLOSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEEA" (
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
  PRIMARY KEY ("AAABLEEU","AAABMKFH","AAABLEEW","AAABLEEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEEY" (
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAABLEFE" varchar(30) NOT NULL default ' ',
  "AAABLEFF" varchar(64) NOT NULL default ' ',
  "AAABLEFG" varchar(64) NOT NULL default ' ',
  "AAABLEFH" varchar(64) NOT NULL default ' ',
  "AAABLEFI" smallint(6) NOT NULL default '0',
  "AAABLEFJ" smallint(6) NOT NULL default '0',
  "AAABLEFK" varchar(10) NOT NULL default ' ',
  "AAABLEFL" smallint(6) NOT NULL default '0',
  "AAABLEFM" int(11) NOT NULL default '0',
  "AAABLEFN" int(11) NOT NULL default '0',
  "AAABLEFO" smallint(6) NOT NULL default '0',
  "AAABLEFP" varchar(25) NOT NULL default ' ',
  "AAABLEFQ" smallint(6) NOT NULL default '0',
  "AAABLEFR" int(11) NOT NULL default '0',
  "AAABLEFS" int(11) NOT NULL default '0',
  "AAABLEFT" smallint(6) NOT NULL default '0',
  "AAABLEFU" smallint(6) NOT NULL default '0',
  "AAABLEFV" int(11) NOT NULL default '0',
  "AAABLEFW" varchar(25) NOT NULL default ' ',
  "AAABLEFX" varchar(255) NOT NULL default ' ',
  "AAABLEFY" varchar(10) NOT NULL default ' ',
  "AAABLEFZ" int(11) NOT NULL default '0',
  "AAABLEGA" smallint(6) NOT NULL default '0',
  "AAABLEGB" smallint(6) NOT NULL default '0',
  "AAABLEGC" smallint(6) NOT NULL default '0',
  "AAABLEGD" int(11) NOT NULL default '0',
  "AAABLEGE" int(11) NOT NULL default '0',
  "AAABLEGF" varchar(25) NOT NULL default ' ',
  "AAABLEGG" varchar(25) NOT NULL default ' ',
  "AAABLEGH" smallint(6) NOT NULL default '0',
  "AAABLEGI" smallint(6) NOT NULL default '0',
  "AAABLEGJ" smallint(6) NOT NULL default '0',
  "AAABLEGK" int(11) NOT NULL default '0',
  "AAABLEGL" smallint(6) NOT NULL default '0',
  "AAABLEGM" varchar(10) NOT NULL default ' ',
  "AAABLEGN" smallint(6) NOT NULL default '0',
  "AAABLEGO" smallint(6) NOT NULL default '0',
  "AAABLEGP" smallint(6) NOT NULL default '0',
  "AAABLEGQ" smallint(6) NOT NULL default '0',
  "AAABLEGR" smallint(6) NOT NULL default '0',
  "AAABLEGS" varchar(255) NOT NULL default ' ',
  "AAABLEGT" int(11) NOT NULL default '0',
  "AAABLEGU" smallint(6) NOT NULL default '0',
  "AAABLEGV" smallint(6) NOT NULL default '0',
  "AAABLEGW" smallint(6) NOT NULL default '0',
  "AAABLEGX" smallint(6) NOT NULL default '0',
  "AAABLEGY" smallint(6) NOT NULL default '0',
  "AAABLEGZ" smallint(6) NOT NULL default '0',
  "AAABLEHA" varchar(25) NOT NULL default ' ',
  "AAABLEHB" smallint(6) NOT NULL default '0',
  "AAABLEHC" int(11) NOT NULL default '0',
  "AAABLEHD" int(11) NOT NULL default '0',
  "AAABLEHE" smallint(6) NOT NULL default '0',
  "AAABLEHF" varchar(10) NOT NULL default ' ',
  "AAABLEHG" int(11) NOT NULL default '0',
  "AAABLEHH" varchar(25) NOT NULL default ' ',
  "AAABLEHI" smallint(6) NOT NULL default '0',
  "AAABLEHJ" varchar(17) NOT NULL default ' ',
  "AAABLEHK" smallint(6) NOT NULL default '0',
  "AAABLEHL" varchar(25) NOT NULL default ' ',
  "AAABLEHM" smallint(6) NOT NULL default '0',
  "AAABLEHN" varchar(25) NOT NULL default ' ',
  "AAABLEHO" varchar(12) NOT NULL default ' ',
  "AAABLEHP" varchar(255) NOT NULL default ' ',
  "AAABLEHQ" varchar(25) NOT NULL default ' ',
  "AAABLEHR" varchar(12) NOT NULL default ' ',
  "AAABLEHS" smallint(6) NOT NULL default '0',
  "AAABLEHT" smallint(6) NOT NULL default '0',
  "AAABLEHU" varchar(25) NOT NULL default ' ',
  "AAABLEHV" varchar(25) NOT NULL default ' ',
  "AAABLEHW" varchar(33) NOT NULL default ' ',
  "AAABLEHX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJF","AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEIB" (
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEIK" varchar(1) NOT NULL default ' ',
  "AAABLEIE" varchar(14) NOT NULL default ' ',
  "AAABLEIF" varchar(20) NOT NULL default ' ',
  "AAABLEIG" varchar(255) NOT NULL default ' ',
  "AAABLEIH" int(11) NOT NULL default '0',
  "AAABLEII" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJK","AAABLEIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEIL" (
  "AAABLEIT" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(18) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABLEIP" varchar(30) NOT NULL default ' ',
  "AAABLEIQ" decimal(22,0) NOT NULL default '0',
  "AAABLEIR" decimal(22,0) NOT NULL default '0',
  "AAABLEIS" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEIT","AAABMIRX","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEIW" (
  "AAABLEIY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEIZ" (
  "AAABLEJI" varchar(5) NOT NULL default ' ',
  "AAABLRTW" varchar(14) NOT NULL default ' ',
  "AAABLEJK" int(11) NOT NULL default '0',
  "AAABLEQO" varchar(14) NOT NULL default ' ',
  "AAABLEJE" int(11) NOT NULL default '0',
  "AAABLEJF" varchar(14) NOT NULL default ' ',
  "AAABLEJG" varchar(40) NOT NULL default ' ',
  "AAABLEJH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABLRTW","AAABLEJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEJL" (
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
CREATE TABLE "AAABLEKD" (
  "AAABLEKH" varchar(40) NOT NULL default ' ',
  "AAABLEKI" varchar(10) NOT NULL default ' ',
  "AAABLEKJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKH","AAABLEKI","AAABLEKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEKK" (
  "AAABLEKN" varchar(3) NOT NULL default ' ',
  "AAABLELJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKN","AAABLELJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEKP" (
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABLEKR" varchar(3) NOT NULL default ' ',
  "AAABLEKS" varchar(30) NOT NULL default ' ',
  "AAABLEKT" VARBINARY(255),
  "AAABLEKU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEKW" (
  "AAABLELA" varchar(20) NOT NULL default ' ',
  "AAABLELB" varchar(14) NOT NULL default ' ',
  "AAABLELC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLELA","AAABLELB","AAABLELC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLELD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLELF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLELH" (
  "AAABLEML" varchar(20) NOT NULL default ' ',
  "AAABLELJ" varchar(60) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLELL" varchar(8) NOT NULL default '00000000',
  "AAABLELM" varchar(6) NOT NULL default '000000',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABLELO" varchar(8) NOT NULL default '00000000',
  "AAABLELP" varchar(6) NOT NULL default '000000',
  "AAABLELQ" varchar(4) NOT NULL default ' ',
  "AAABLELR" varchar(1) NOT NULL default ' ',
  "AAABLELS" varchar(1) NOT NULL default ' ',
  "AAABLELT" varchar(20) NOT NULL default ' ',
  "AAABLELU" varchar(4) NOT NULL default ' ',
  "AAABLELV" varchar(4) NOT NULL default ' ',
  "AAABLELW" varchar(10) NOT NULL default ' ',
  "AAABLELX" varchar(10) NOT NULL default ' ',
  "AAABLELY" varchar(3) NOT NULL default ' ',
  "AAABLELZ" varchar(20) NOT NULL default ' ',
  "AAABLEMA" varchar(1) NOT NULL default ' ',
  "AAABLEMB" varchar(1) NOT NULL default ' ',
  "AAABLEMC" varchar(1) NOT NULL default ' ',
  "AAABLEMD" varchar(10) NOT NULL default ' ',
  "AAABLEME" varchar(12) NOT NULL default ' ',
  "AAABLEMF" varchar(1) NOT NULL default ' ',
  "AAABLEMG" varchar(20) NOT NULL default ' ',
  "AAABLEMH" varchar(4) NOT NULL default ' ',
  "AAABLEMI" varchar(2) NOT NULL default ' ',
  "AAABLEMJ" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEMM" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABLENH","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEMW" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLENA" varchar(16) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  "AAABLENC" varchar(14) NOT NULL default ' ',
  "AAABLEND" varchar(16) NOT NULL default ' ',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLENI" (
  "AAABLUUU" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(8) NOT NULL default ' ',
  "AAABLENL" varchar(8) NOT NULL default '00000000',
  "AAABLENM" varchar(2) NOT NULL default ' ',
  "AAABLENN" varchar(6) NOT NULL default ' ',
  "AAABLENO" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUXO" varchar(14) NOT NULL default ' ',
  "AAABLENR" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUUU","AAABMDXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLENV" (
  "AAABLENY" varchar(20) NOT NULL default ' ',
  "AAABLENX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLENZ" (
  "AAABLTOK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEOC" (
  "AAABLEUE" varchar(50) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABLEOQ" varchar(20) NOT NULL default ' ',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLEOT" varchar(3) NOT NULL default ' ',
  "AAABLEOU" int(11) NOT NULL default '0',
  "AAABLEOK" varchar(8) NOT NULL default '00000000',
  "AAABLEOL" int(11) NOT NULL default '0',
  "AAABLEOM" int(11) NOT NULL default '0',
  "AAABLEON" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUE","AAABLUXP","AAABLEOQ","AAABLTIZ","AAABMREL","AAABLEOT","AAABLEOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEOV" (
  "AAABLEUE" varchar(50) NOT NULL default ' ',
  "AAABLUXP" varchar(13) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLEPL" varchar(20) NOT NULL default ' ',
  "AAABLEPM" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMJWN" varchar(10) NOT NULL default ' ',
  "AAABLEPD" varchar(20) NOT NULL default ' ',
  "AAABLEPE" varchar(20) NOT NULL default ' ',
  "AAABLEPF" varchar(20) NOT NULL default ' ',
  "AAABLEPG" varchar(20) NOT NULL default ' ',
  "AAABLEPH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUE","AAABLUXP","AAABMKFH","AAABLEPL","AAABLEPM","AAABMLPO","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEPP" (
  "AAABLEUE" varchar(50) NOT NULL default ' ',
  "AAABLETY" varchar(3) NOT NULL default ' ',
  "AAABLETZ" varchar(20) NOT NULL default ' ',
  "AAABLESE" int(11) NOT NULL default '0',
  "AAABLEUB" int(11) NOT NULL default '0',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABLESH" varchar(8) NOT NULL default '00000000',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLESQ" varchar(12) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLEQB" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLESU" varchar(128) NOT NULL default ' ',
  "AAABLESV" varchar(255) NOT NULL default ' ',
  "AAABLESW" varchar(32) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABLNZW" varchar(8) NOT NULL default '00000000',
  "AAABLNZX" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABLEQP" varchar(8) NOT NULL default '00000000',
  "AAABLETA" varchar(6) NOT NULL default '000000',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAABLEUF" varchar(10) NOT NULL default '0000000000',
  "AAABLETC" varchar(10) NOT NULL default '0000000000',
  "AAABLETD" varchar(1) NOT NULL default ' ',
  "AAABLETE" int(11) NOT NULL default '0',
  "AAABLETF" varchar(1) NOT NULL default ' ',
  "AAABLETG" varchar(6) NOT NULL default '000000',
  "AAABLETH" varchar(6) NOT NULL default '000000',
  "AAABLEQZ" varchar(20) NOT NULL default ' ',
  "AAABLETI" varchar(1) NOT NULL default ' ',
  "AAABLERB" int(11) NOT NULL default '0',
  "AAABLERC" int(11) NOT NULL default '0',
  "AAABLERD" int(11) NOT NULL default '0',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERG" varchar(10) NOT NULL default ' ',
  "AAABLERH" varchar(1) NOT NULL default ' ',
  "AAABLERI" varchar(3) NOT NULL default ' ',
  "AAABLERJ" varchar(1) NOT NULL default ' ',
  "AAABLERK" varchar(3) NOT NULL default ' ',
  "AAABLERL" varchar(1) NOT NULL default ' ',
  "AAABLERM" varchar(3) NOT NULL default ' ',
  "AAABLERN" varchar(1) NOT NULL default ' ',
  "AAABLERO" varchar(3) NOT NULL default ' ',
  "AAABLERP" varchar(1) NOT NULL default ' ',
  "AAABLERQ" varchar(3) NOT NULL default ' ',
  "AAABLERR" varchar(1) NOT NULL default ' ',
  "AAABLERS" varchar(3) NOT NULL default ' ',
  "AAABLERT" varchar(1) NOT NULL default ' ',
  "AAABLETT" varchar(6) NOT NULL default '000000',
  "AAABLERV" int(11) NOT NULL default '0',
  "AAABLETU" varchar(1) NOT NULL default ' ',
  "AAABLERX" int(11) NOT NULL default '0',
  "AAABLERY" int(11) NOT NULL default '0',
  "AAABLERZ" int(11) NOT NULL default '0',
  "AAABLETV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLEUE","AAABLETY","AAABLETZ","AAABLESE","AAABLEUB","AAABLEUA","AAABLESH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLESI" (
  "AAABLEUE" varchar(50) NOT NULL default ' ',
  "AAABLEUD" varchar(1) NOT NULL default ' ',
  "AAABLETY" varchar(3) NOT NULL default ' ',
  "AAABLETZ" varchar(20) NOT NULL default ' ',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABLEUB" int(11) NOT NULL default '0',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLESQ" varchar(12) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLESU" varchar(128) NOT NULL default ' ',
  "AAABLESV" varchar(255) NOT NULL default ' ',
  "AAABLESW" varchar(32) NOT NULL default ' ',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABLESY" varchar(64) NOT NULL default ' ',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABLETA" varchar(6) NOT NULL default '000000',
  "AAABLEUF" varchar(10) NOT NULL default '0000000000',
  "AAABLETC" varchar(10) NOT NULL default '0000000000',
  "AAABLETD" varchar(1) NOT NULL default ' ',
  "AAABLETE" int(11) NOT NULL default '0',
  "AAABLETF" varchar(1) NOT NULL default ' ',
  "AAABLETG" varchar(6) NOT NULL default '000000',
  "AAABLETH" varchar(6) NOT NULL default '000000',
  "AAABLETI" varchar(1) NOT NULL default ' ',
  "AAABLETJ" int(11) NOT NULL default '0',
  "AAABLETK" int(11) NOT NULL default '0',
  "AAABLETL" int(11) NOT NULL default '0',
  "AAABLETM" int(11) NOT NULL default '0',
  "AAABLETN" int(11) NOT NULL default '0',
  "AAABLETO" int(11) NOT NULL default '0',
  "AAABLETP" int(11) NOT NULL default '0',
  "AAABLETQ" int(11) NOT NULL default '0',
  "AAABLETR" varchar(1) NOT NULL default ' ',
  "AAABLETS" varchar(1) NOT NULL default ' ',
  "AAABLETT" varchar(6) NOT NULL default '000000',
  "AAABLETU" varchar(1) NOT NULL default ' ',
  "AAABLETV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLEUE","AAABLEUD","AAABLETY","AAABLETZ","AAABLEUA","AAABLEUB"),
  KEY "AAABLEUC" ("AAABLEUD","AAABLEUE","AAABLEUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEUG" (
  "AAABLEVA" varchar(18) NOT NULL default ' ',
  "AAABLEUM" varchar(53) NOT NULL default ' ',
  "AAABLEUJ" int(11) NOT NULL default '0',
  "AAABLEUK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLEVA","AAABLEUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEUN" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABLEVA" varchar(18) NOT NULL default ' ',
  "AAABLEUS" int(11) NOT NULL default '0',
  "AAABLEUT" int(11) NOT NULL default '0',
  "AAABLEUU" int(11) NOT NULL default '0',
  "AAABLEUV" int(11) NOT NULL default '0',
  "AAABLEUW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMDEP","AAABMOIA","AAABLEUZ","AAABLEVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEWO" varchar(10) NOT NULL default ' ',
  "AAABLFGJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLFGK" decimal(13,3) NOT NULL default '0.000',
  "AAABLEVG" varchar(3) NOT NULL default ' ',
  "AAABLFQH" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQI" decimal(13,3) NOT NULL default '0.000',
  "AAABLEVJ" varchar(3) NOT NULL default ' ',
  "AAABLFPY" varchar(2) NOT NULL default '00',
  "AAABLFGO" varchar(1) NOT NULL default '0',
  "AAABLEVM" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEWO" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEVT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEWO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEWO" varchar(10) NOT NULL default ' ',
  "AAABLEXI" varchar(3) NOT NULL default ' ',
  "AAABLEWU" varchar(3) NOT NULL default ' ',
  "AAABLFEV" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEW" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEX" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEY" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFA" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFB" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFE" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFF" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFD" varchar(1) NOT NULL default ' ',
  "AAABLFFG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEWO","AAABLEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZCS" varchar(10) NOT NULL default ' ',
  "AAABLEXI" varchar(3) NOT NULL default ' ',
  "AAABLEWU" varchar(3) NOT NULL default ' ',
  "AAABLFEV" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEW" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEX" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEY" decimal(13,3) NOT NULL default '0.000',
  "AAABLFEZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFA" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFB" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFE" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFF" decimal(13,3) NOT NULL default '0.000',
  "AAABLFFD" varchar(1) NOT NULL default ' ',
  "AAABLFFG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZCS","AAABLEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFFG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFFG" varchar(3) NOT NULL default ' ',
  "AAABLEXS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(10) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(10) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255),
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFCY" varchar(10) NOT NULL default ' ',
  "AAABLEYX" varchar(10) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLEZH" varchar(13) NOT NULL default '0000000000000',
  "AAABLEZI" varchar(2) NOT NULL default '00',
  "AAABLEZJ" varchar(1) NOT NULL default ' ',
  "AAABLEZK" varchar(1) NOT NULL default ' ',
  "AAABLEZL" varchar(1) NOT NULL default ' ',
  "AAABLEZM" varchar(4) NOT NULL default '0000',
  "AAABLEZN" varchar(40) NOT NULL default ' ',
  "AAABLEZO" varchar(1) NOT NULL default '0',
  "AAABLEZP" varchar(1) NOT NULL default '0',
  "AAABLEZQ" varchar(2) NOT NULL default '00',
  "AAABLEZR" varchar(10) NOT NULL default ' ',
  "AAABLEZS" varchar(1) NOT NULL default ' ',
  "AAABLEZT" varchar(18) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAABLFJC" varchar(18) NOT NULL default ' ',
  "AAABMQWT" varchar(20) NOT NULL default ' ',
  "AAABLFAI" varchar(13) NOT NULL default '0000000000000',
  "AAABLFAJ" varchar(13) NOT NULL default '0000000000000',
  "AAABLYMY" varchar(13) NOT NULL default '0000000000000',
  "AAABLFPM" varchar(13) NOT NULL default '0000000000000',
  "AAABMLEV" varchar(13) NOT NULL default '0000000000000',
  "AAABLFAN" varchar(5) NOT NULL default '00000',
  "AAABLFAO" varchar(5) NOT NULL default '00000',
  "AAABLFAP" varchar(5) NOT NULL default '00000',
  "AAABLFAQ" varchar(5) NOT NULL default '00000',
  "AAABLFAR" varchar(5) NOT NULL default '00000',
  "AAABLFAS" varchar(5) NOT NULL default '00000',
  "AAABLFAT" varchar(5) NOT NULL default '00000',
  "AAABLFAU" varchar(5) NOT NULL default '00000',
  "AAABLFAV" varchar(1) NOT NULL default ' ',
  "AAABLFAW" varchar(1) NOT NULL default ' ',
  "AAABLFAX" varchar(1) NOT NULL default ' ',
  "AAABLFAY" varchar(1) NOT NULL default ' ',
  "AAABLFAZ" varchar(3) NOT NULL default '000',
  "AAABLFBA" varchar(1) NOT NULL default ' ',
  "AAABLFBB" varchar(18) NOT NULL default ' ',
  "AAABLFIH" varchar(1) NOT NULL default '0',
  "AAABLFIJ" varchar(3) NOT NULL default ' ',
  "AAABLFPO" varchar(3) NOT NULL default ' ',
  "AAABLFPN" varchar(3) NOT NULL default ' ',
  "AAABLFBG" varchar(1) NOT NULL default ' ',
  "AAABLFII" varchar(3) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABMQIL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFOP" varchar(18) NOT NULL default ' ',
  "AAABLFBS" varchar(40) NOT NULL default ' ',
  "AAABLFBT" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBU" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBV" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBW" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBX" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBY" varchar(13) NOT NULL default '0000000000000',
  "AAABLFBZ" varchar(13) NOT NULL default '0000000000000',
  "AAABLFCA" varchar(13) NOT NULL default '0000000000000',
  "AAABLFCB" varchar(13) NOT NULL default '0000000000000',
  "AAABLFCC" varchar(13) NOT NULL default '0000000000000',
  "AAABLFCD" varchar(2) NOT NULL default '00',
  "AAABLFJC" varchar(18) NOT NULL default ' ',
  "AAABLFCF" varchar(1) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFCY" varchar(10) NOT NULL default ' ',
  "AAABLFCZ" varchar(6) NOT NULL default '000000',
  "AAABLFCQ" varchar(18) NOT NULL default ' ',
  "AAABLFCR" varchar(40) NOT NULL default ' ',
  "AAABLFCS" varchar(13) NOT NULL default '0000000000000',
  "AAABLFCT" varchar(3) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFCY","AAABLFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFDD" varchar(1) NOT NULL default ' ',
  "AAABLFZK" varchar(2) NOT NULL default ' ',
  "AAABLFJZ" varchar(2) NOT NULL default '00',
  "AAABLFKA" varchar(1) NOT NULL default ' ',
  "AAABLFKB" varchar(1) NOT NULL default ' ',
  "AAABLFKC" varchar(1) NOT NULL default ' ',
  "AAABLFKD" varchar(3) NOT NULL default '000',
  "AAABLFKE" varchar(1) NOT NULL default ' ',
  "AAABLFKF" varchar(1) NOT NULL default ' ',
  "AAABLFKG" varchar(5) NOT NULL default '00000',
  "AAABLFKH" varchar(4) NOT NULL default '0000',
  "AAABLFKI" varchar(3) NOT NULL default '000',
  "AAABLFDP" varchar(1) NOT NULL default ' ',
  "AAABLFKJ" varchar(1) NOT NULL default '0',
  "AAABLFKK" varchar(1) NOT NULL default ' ',
  "AAABLFKN" varchar(1) NOT NULL default '0',
  "AAABLFKL" varchar(3) NOT NULL default ' ',
  "AAABLFDU" varchar(8) NOT NULL default '00000000',
  "AAABLFDV" varchar(8) NOT NULL default '00000000',
  "AAABLFKM" varchar(3) NOT NULL default ' ',
  "AAABLFDX" varchar(8) NOT NULL default '00000000',
  "AAABLFDY" varchar(8) NOT NULL default '00000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFEA" varchar(8) NOT NULL default '00000000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLZMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFLO" varchar(18) NOT NULL default ' ',
  "AAABLFLP" varchar(18) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFLO","AAABLFLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZCS" varchar(18) NOT NULL default ' ',
  "AAABLFON" varchar(3) NOT NULL default ' ',
  "AAABLFEV" varchar(13) NOT NULL default '0000000000000',
  "AAABLFEW" varchar(13) NOT NULL default '0000000000000',
  "AAABLFEX" varchar(13) NOT NULL default '0000000000000',
  "AAABLFEY" varchar(13) NOT NULL default '0000000000000',
  "AAABLFEZ" varchar(13) NOT NULL default '0000000000000',
  "AAABLFFA" varchar(13) NOT NULL default '0000000000000',
  "AAABLFFB" varchar(13) NOT NULL default '0000000000000',
  "AAABLFFC" varchar(1) NOT NULL default ' ',
  "AAABLFFD" varchar(1) NOT NULL default ' ',
  "AAABLFFE" varchar(13) NOT NULL default '0000000000000',
  "AAABLFFF" varchar(13) NOT NULL default '0000000000000',
  "AAABLFFG" varchar(3) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLZCS","AAABLFON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFFQ" varchar(10) NOT NULL default ' ',
  "AAABLFFR" varchar(10) NOT NULL default ' ',
  "AAABLFLF" varchar(4) NOT NULL default ' ',
  "AAABLFFT" varchar(4) NOT NULL default ' ',
  "AAABLFFU" varchar(18) NOT NULL default ' ',
  "AAABLFFV" varchar(18) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZCS" varchar(18) NOT NULL default ' ',
  "AAABLFPY" varchar(2) NOT NULL default '00',
  "AAABLYMO" varchar(13) NOT NULL default '0000000000000',
  "AAABLYMP" varchar(13) NOT NULL default '0000000000000',
  "AAABLYMQ" varchar(13) NOT NULL default '0000000000000',
  "AAABLYMK" varchar(13) NOT NULL default '0000000000000',
  "AAABLYML" varchar(13) NOT NULL default '0000000000000',
  "AAABLFGJ" varchar(13) NOT NULL default '0000000000000',
  "AAABLFGK" varchar(13) NOT NULL default '0000000000000',
  "AAABLFQH" varchar(13) NOT NULL default '0000000000000',
  "AAABLFQI" varchar(13) NOT NULL default '0000000000000',
  "AAABLFGN" varchar(18) NOT NULL default ' ',
  "AAABLFGO" varchar(1) NOT NULL default '0',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLZCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABLFHC" varchar(20) NOT NULL default ' ',
  "AAABLOCJ" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABLFHC","AAABLOCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFHH" varchar(4) NOT NULL default '0000',
  "AAABLFHI" varchar(22) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLZMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFJC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLYLY" varchar(10) NOT NULL default ' ',
  "AAABLFOP" varchar(18) NOT NULL default ' ',
  "AAABLFHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLYLY","AAABLFOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(18) NOT NULL default ' ',
  "AAABLFIH" varchar(1) NOT NULL default '0',
  "AAABLFII" varchar(3) NOT NULL default ' ',
  "AAABLFIJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(18) NOT NULL default ' ',
  "AAABLFOP" varchar(18) NOT NULL default ' ',
  "AAABLFIS" varchar(5) NOT NULL default '00000',
  "AAABLFIT" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFJP","AAABLFOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(18) NOT NULL default ' ',
  "AAABLFJC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(18) NOT NULL default ' ',
  "AAABLFJL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLC" varchar(10) NOT NULL default ' ',
  "AAABLFJZ" varchar(2) NOT NULL default '00',
  "AAABLFKA" varchar(1) NOT NULL default ' ',
  "AAABLFKB" varchar(1) NOT NULL default ' ',
  "AAABLFKC" varchar(1) NOT NULL default ' ',
  "AAABLFKD" varchar(3) NOT NULL default '000',
  "AAABLFKE" varchar(1) NOT NULL default ' ',
  "AAABLFKF" varchar(1) NOT NULL default ' ',
  "AAABLFKG" varchar(5) NOT NULL default '00000',
  "AAABLFKH" varchar(4) NOT NULL default '0000',
  "AAABLFKI" varchar(3) NOT NULL default '000',
  "AAABLFKJ" varchar(1) NOT NULL default '0',
  "AAABLFKK" varchar(1) NOT NULL default ' ',
  "AAABLFKL" varchar(3) NOT NULL default ' ',
  "AAABLFKM" varchar(3) NOT NULL default ' ',
  "AAABLFKN" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLFLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFLC" varchar(10) NOT NULL default ' ',
  "AAABLFKU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLFLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABLFLF" varchar(4) NOT NULL default ' ',
  "AAABLFLC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABLFLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFLO" varchar(18) NOT NULL default ' ',
  "AAABLFLP" varchar(18) NOT NULL default ' ',
  "AAABMALF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFLO","AAABLFLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABLFMO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLFMO" varchar(1) NOT NULL default '0',
  "AAABLFMP" varchar(8) NOT NULL default '00000000',
  "AAABLFMQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLZMY","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFMY" varchar(22) NOT NULL default ' ',
  "AAABLFMZ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFPS" varchar(4) NOT NULL default '0000',
  "AAABLFPT" varchar(4) NOT NULL default '0000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABLFNW" varchar(10) NOT NULL default '0000000000',
  "AAABLFNX" varchar(4) NOT NULL default '0000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFPS","AAABLFPT","AAABMLFK","AAABLFNW","AAABLFNX"),
  KEY "AAABLFNT" ("AAABMRIL","AAABMLFK","AAABLFNW","AAABLFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFOW" varchar(1) NOT NULL default ' ',
  "AAABLFPI" varchar(1) NOT NULL default ' ',
  "AAABLFRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLFOW","AAABLFPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFPS" varchar(4) NOT NULL default '0000',
  "AAABLZCS" varchar(18) NOT NULL default ' ',
  "AAABLFON" varchar(3) NOT NULL default ' ',
  "AAABLFOO" varchar(1) NOT NULL default ' ',
  "AAABLFOP" varchar(18) NOT NULL default ' ',
  "AAABLFOQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQK" varchar(3) NOT NULL default ' ',
  "AAABLFOS" decimal(13,3) NOT NULL default '0.000',
  "AAABLFOT" decimal(13,3) NOT NULL default '0.000',
  "AAABLFOU" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQJ" varchar(3) NOT NULL default ' ',
  "AAABLFOW" varchar(1) NOT NULL default ' ',
  "AAABLFOX" varchar(1) NOT NULL default ' ',
  "AAABLFQM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLFPS" varchar(4) NOT NULL default '0000',
  "AAABLFPT" varchar(4) NOT NULL default '0000',
  "AAABLFPH" varchar(3) NOT NULL default '000',
  "AAABLFPI" varchar(1) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLFPM" decimal(13,3) NOT NULL default '0.000',
  "AAABLFPN" varchar(3) NOT NULL default ' ',
  "AAABLFPO" varchar(3) NOT NULL default ' ',
  "AAABLFRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLFPS","AAABLFPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZMY" varchar(10) NOT NULL default ' ',
  "AAABLZCS" varchar(18) NOT NULL default ' ',
  "AAABLFPY" varchar(2) NOT NULL default '00',
  "AAABLFPZ" varchar(3) NOT NULL default '000',
  "AAABLFQA" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQB" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQC" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQD" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQE" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQF" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQG" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQH" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQI" decimal(13,3) NOT NULL default '0.000',
  "AAABLFQJ" varchar(3) NOT NULL default ' ',
  "AAABLFQK" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFQM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZMY","AAABLZCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLFRA" varchar(1) NOT NULL default ' ',
  "AAABLFQV" varchar(8) NOT NULL default ' ',
  "AAABLFQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABLFRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLFRE" decimal(13,3) NOT NULL default '0.000',
  "AAABLFRF" varchar(3) NOT NULL default ' ',
  "AAABLFRG" varchar(1) NOT NULL default ' ',
  "AAABLFRH" varchar(18) NOT NULL default ' ',
  "AAABLFRI" varchar(2) NOT NULL default '00',
  "AAABLFRJ" varchar(1) NOT NULL default ' ',
  "AAABLFRK" varchar(1) NOT NULL default ' ',
  "AAABLFRL" varchar(1) NOT NULL default '0',
  "AAABLFRM" varchar(1) NOT NULL default '0',
  "AAABLFRN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFRV" varchar(10) NOT NULL default ' ',
  "AAABLFRT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLFRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLGN" varchar(4) NOT NULL default ' ',
  "AAABLFRZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
