# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl21.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAATMMI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATMMY" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATMMY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMMR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATMMY" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATMMY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMNA" (
  "AAAATMNI" varchar(3) NOT NULL default '000',
  "AAAATMNJ" varchar(3) NOT NULL default '000',
  "AAAATMNK" varchar(12) NOT NULL default ' ',
  "AAAATMNL" tinyint(3) unsigned NOT NULL default '0',
  "AAAATMNN" varchar(3) NOT NULL default '000',
  "AAAATMNO" varchar(3) NOT NULL default '000',
  "AAAATMNP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAATMNI","AAAATMNJ","AAAATMNK","AAAATMNL"),
  KEY "AAAATMNM" ("AAAATMNN","AAAATMNO","AAAATMNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMNQ" (
  "AAAATMOB" varchar(3) NOT NULL default '000',
  "AAAATMOC" varchar(3) NOT NULL default '000',
  "AAAATMRL" varchar(12) NOT NULL default ' ',
  "AAAATMOG" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAAATMNW" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABKERN" varchar(8) NOT NULL default '00000000',
  "AAAATMNZ" int(11) NOT NULL default '0',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAATMOB","AAAATMOC","AAAATMRL"),
  KEY "AAAATMOE" ("AAAATMRL","AAAATMOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMOH" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATMOO" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATMOO","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMOQ" (
  "AAAATMOS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATMOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMOT" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAAATMOW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMOZ" (
  "AAAATMPF" varchar(30) NOT NULL default ' ',
  "AAAATMPG" varchar(10) NOT NULL default ' ',
  "AAAATMPC" varchar(1) NOT NULL default ' ',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATMPF","AAAATMPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMPH" (
  "AAABMNHT" varchar(3) NOT NULL default '000',
  "AAAATMPT" varchar(3) NOT NULL default '000',
  "AAAATMPU" varchar(12) NOT NULL default ' ',
  "AAAATMPV" varchar(8) NOT NULL default '00000000',
  "AAAATMPW" varchar(6) NOT NULL default '000000',
  "AAAATMPX" varchar(20) NOT NULL default ' ',
  "AAAATMPY" VARBINARY(255) NOT NULL,
  "AAAATMPP" int(11) NOT NULL default '0',
  "AAAATMPQ" int(11) NOT NULL default '0',
  "AAAATMPR" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNHT","AAAATMPT","AAAATMPU","AAAATMPV","AAAATMPW","AAAATMPX","AAAATMPY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMPZ" (
  "AAAATMRJ" varchar(3) NOT NULL default '000',
  "AAAATMRK" varchar(3) NOT NULL default '000',
  "AAAATMRL" varchar(12) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAATMQH" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATMRJ","AAAATMRK","AAAATMRL","AAABLPJD","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMQO" (
  "AAAATMRJ" varchar(3) NOT NULL default '000',
  "AAAATMRK" varchar(3) NOT NULL default '000',
  "AAAATMRL" varchar(12) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABLQSF" varchar(30) NOT NULL default ' ',
  "AAABKPCZ" smallint(6) NOT NULL default '0',
  "AAAATMRQ" smallint(6) NOT NULL default '0',
  "AAAATMQX" varchar(40) NOT NULL default ' ',
  "AAABJWYW" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(6) NOT NULL default '000000',
  "AAABJPEE" varchar(4) NOT NULL default '0000',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(10) NOT NULL default ' ',
  "AAABGOLD" varchar(80) NOT NULL default ' ',
  "AAABGOLE" varchar(20) NOT NULL default ' ',
  "AAABGOLF" varchar(20) NOT NULL default ' ',
  "AAABGOLG" varchar(20) NOT NULL default ' ',
  "AAAATMRH" int(11) NOT NULL default '0',
  "AAAATMRI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATMRJ","AAAATMRK","AAAATMRL","AAABLPJD","AAABLGCM","AAABLQSF","AAABKPCZ","AAAATMRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMRR" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJSTP" varchar(20) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAATPKH" varchar(3) NOT NULL default ' ',
  "AAAATMRW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJSTP","AAABLEHK","AAAATPKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMSB" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMSE" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAAATMSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMSO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATMSV" varchar(12) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATMSV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMSX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAATMTA" varchar(12) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMTF" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABENDM" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMRHK","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMTW" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAATMUM" varchar(1) NOT NULL default ' ',
  "AAAATMUA" varchar(1) NOT NULL default ' ',
  "AAAATMUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMUD" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAATMUV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMUI" (
  "AAABLMLR" varchar(12) NOT NULL default ' ',
  "AAAATMUK" varchar(12) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAATMUM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMUO" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMUS" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAATMUV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABLFYJ" varchar(12) NOT NULL default ' ',
  "AAAATMVC" varchar(12) NOT NULL default ' ',
  "AAAATMVD" varchar(40) NOT NULL default ' ',
  "AAAATMVE" varchar(14) NOT NULL default ' ',
  "AAAATMVF" varchar(40) NOT NULL default ' ',
  "AAAATMVG" varchar(14) NOT NULL default ' ',
  "AAAATMVH" varchar(20) NOT NULL default ' ',
  "AAABDNPH" varchar(1) NOT NULL default ' ',
  "AAAATMVV" varchar(12) NOT NULL default ' ',
  "AAAATMVK" varchar(4) NOT NULL default ' ',
  "AAAATMVL" varchar(4) NOT NULL default ' ',
  "AAAATMVM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATMWE" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAATMVV" varchar(12) NOT NULL default ' ',
  "AAAAUHRK" varchar(8) NOT NULL default '00000000',
  "AAAAUHRL" varchar(6) NOT NULL default '000000',
  "AAABDNOU" varchar(4) NOT NULL default ' ',
  "AAABENDJ" varchar(8) NOT NULL default ' ',
  "AAAAVMEG" varchar(12) NOT NULL default '000000000000',
  "AAAATMWB" varchar(32) NOT NULL default ' ',
  "AAAATMWC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATMWE"),
  KEY "AAAATMWF" ("AAABMRIL","AAABLUVS","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUVS","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAAATMWW" smallint(6) NOT NULL default '0',
  "AAAATMWX" smallint(6) NOT NULL default '0',
  "AAAATMWY" varchar(1) NOT NULL default ' ',
  "AAAAUKFC" varchar(12) NOT NULL default ' ',
  "AAAATMXA" varchar(70) NOT NULL default ' ',
  "AAAAUXIU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMXK" (
  "AAABDNPH" varchar(1) NOT NULL default ' ',
  "AAABIKDN" varchar(40) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAATMXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNPH","AAABIKDN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMXT" (
  "AAABDNPH" varchar(1) NOT NULL default ' ',
  "AAABIKDN" varchar(40) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAATMYF" varchar(3) NOT NULL default '000',
  "AAAATMXZ" varchar(30) NOT NULL default ' ',
  "AAAATMYA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABDNPH","AAABIKDN","AAABMLPO","AAABMDEP","AAAATMYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMYG" (
  "AAABDOFI" varchar(22) NOT NULL default ' ',
  "AAAAVNNF" varchar(10) NOT NULL default ' ',
  "AAABIVVF" varchar(10) NOT NULL default '0000000000',
  "AAABIJNY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDOFI"),
  KEY "AAAATMYM" ("AAAAVNNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMYO" (
  "AAABDOFI" varchar(22) NOT NULL default ' ',
  "AAAAUGKU" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDOFI","AAAAUGKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMYW" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMJYX" varchar(220) NOT NULL default ' ',
  "AAAATMZD" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR"),
  UNIQUE KEY "AAAATMZB" ("AAABMJYX","AAAATMZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMZE" (
  "AAAATNIG" varchar(60) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATNIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMZI" (
  "AAABFSPQ" VARBINARY(255) NOT NULL,
  "AAAATNCV" decimal(15,0) NOT NULL default '0',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABMJYX" varchar(250) NOT NULL default ' ',
  "AAABIMRZ" varchar(40) NOT NULL default ' ',
  "AAAATNCX" varchar(60) NOT NULL default ' ',
  "AAAATNCY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFSPQ"(255),"AAAATNCV"),
  KEY "AAAATMZS" ("AAAATNCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMZU" (
  "AAABDUBG" VARBINARY(255) NOT NULL,
  "AAAATMZW" decimal(15,0) NOT NULL default '0',
  "AAAATMZX" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABDUBG"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATMZZ" (
  "AAAATNIG" varchar(60) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATNIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNAD" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMOHM" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABLEKS","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNAL" (
  "AAAATNAP" varchar(22) NOT NULL default ' ',
  "AAAATNAQ" varchar(22) NOT NULL default ' ',
  "AAAATNAO" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAAATNAP","AAAATNAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNAR" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAAATNIM" varchar(255) NOT NULL default ' ',
  "AAABLPSU" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNAX" (
  "AAABFSPQ" VARBINARY(255) NOT NULL,
  "AAABHHAD" VARBINARY(255),
  "AAABMJYX" varchar(250) NOT NULL default ' ',
  "AAABIMRZ" varchar(40) NOT NULL default ' ',
  "AAAATNCV" decimal(15,0) NOT NULL default '0',
  "AAAATNCX" varchar(60) NOT NULL default ' ',
  "AAAATNCY" varchar(60) NOT NULL default ' ',
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  "AAAATNBM" int(11) NOT NULL default '0',
  "AAAATNBN" int(11) NOT NULL default '0',
  "AAAATNBO" int(11) NOT NULL default '0',
  "AAAATNBP" int(11) NOT NULL default '0',
  "AAABMIRX" varchar(40) NOT NULL default ' ',
  "AAAATNBR" varchar(40) NOT NULL default ' ',
  "AAAATNHP" varchar(40) NOT NULL default ' ',
  "AAAATNBT" int(11) NOT NULL default '0',
  "AAAATNBU" int(11) NOT NULL default '0',
  "AAAATNBV" int(11) NOT NULL default '0',
  "AAAAVASI" varchar(8) NOT NULL default ' ',
  "AAAAVASJ" varchar(40) NOT NULL default ' ',
  "AAAAVASK" varchar(10) NOT NULL default ' ',
  "AAAAVASL" varchar(10) NOT NULL default ' ',
  "AAAAVASM" varchar(8) NOT NULL default '00000000',
  "AAAAVASN" varchar(6) NOT NULL default '000000',
  "AAAAVASO" varchar(2) NOT NULL default ' ',
  "AAAATNCD" varchar(8) NOT NULL default '00000000',
  "AAAATNCE" varchar(6) NOT NULL default '000000',
  "AAAATNCF" varchar(2) NOT NULL default ' ',
  "AAAATNCG" int(11) NOT NULL default '0',
  "AAAATNCH" int(11) NOT NULL default '0',
  "AAAATNCI" varchar(8) NOT NULL default ' ',
  "AAAATNCJ" varchar(40) NOT NULL default ' ',
  "AAAATNCK" varchar(10) NOT NULL default ' ',
  "AAAATNCL" varchar(10) NOT NULL default ' ',
  "AAAATNCM" varchar(8) NOT NULL default '00000000',
  "AAAATNCN" varchar(6) NOT NULL default '000000',
  "AAAATNCO" varchar(2) NOT NULL default ' ',
  "AAAATNCP" int(11) NOT NULL default '0',
  "AAAATNCQ" int(11) NOT NULL default '0',
  "AAAATNCR" int(11) NOT NULL default '0',
  "AAAATNCS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABFSPQ"(255)),
  KEY "AAAATNCU" ("AAAATNCV"),
  KEY "AAAATNCW" ("AAAATNCX","AAAATNCY"),
  KEY "AAAATNCZ" ("AAABMJYX"),
  KEY "AAAATNDB" ("AAABHHAD"(255)),
  KEY "AAAATNDD" ("AAAAVARC","AAAAVASQ","AAAAVARE","AAAAVARF","AAAAVARG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNDJ" (
  "AAABFSPQ" VARBINARY(255) NOT NULL,
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  "AAAATNHM" int(11) NOT NULL default '0',
  "AAAATNDT" int(11) NOT NULL default '0',
  "AAAATNDU" int(11) NOT NULL default '0',
  "AAAATNDV" int(11) NOT NULL default '0',
  "AAAATNDW" int(11) NOT NULL default '0',
  "AAAATNDX" int(11) NOT NULL default '0',
  "AAAATNDY" int(11) NOT NULL default '0',
  "AAAATNDZ" int(11) NOT NULL default '0',
  "AAAATNHP" varchar(40) NOT NULL default ' ',
  "AAAATNEB" varchar(50) NOT NULL default ' ',
  "AAAATNEC" varchar(8) NOT NULL default '00000000',
  "AAAATNED" varchar(6) NOT NULL default '000000',
  "AAAATNEE" varchar(2) NOT NULL default ' ',
  "AAAATNEF" int(11) NOT NULL default '0',
  "AAAATNEG" int(11) NOT NULL default '0',
  "AAAATNEH" int(11) NOT NULL default '0',
  "AAAATNEI" int(11) NOT NULL default '0',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAAVASF" varchar(255) NOT NULL default ' ',
  "AAABHZBX" varchar(16) NOT NULL default ' ',
  "AAABMLDW" varchar(30) NOT NULL default ' ',
  "AAABHZBC" varchar(128) NOT NULL default ' ',
  "AAABHZBD" varchar(1) NOT NULL default ' ',
  "AAABHZBE" varchar(128) NOT NULL default ' ',
  "AAABHZBF" varchar(1) NOT NULL default ' ',
  "AAABHZBG" varchar(128) NOT NULL default ' ',
  "AAABHZBH" varchar(1) NOT NULL default ' ',
  "AAABHZBI" varchar(128) NOT NULL default ' ',
  "AAABHZBJ" varchar(1) NOT NULL default ' ',
  "AAABJVWR" varchar(128) NOT NULL default ' ',
  "AAABLENB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABFSPQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNEZ" (
  "AAABFSPQ" VARBINARY(255) NOT NULL,
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  "AAAATNHM" int(11) NOT NULL default '0',
  "AAAATNFJ" int(11) NOT NULL default '0',
  "AAAATNFK" int(11) NOT NULL default '0',
  "AAAATNFL" int(11) NOT NULL default '0',
  "AAAATNFM" int(11) NOT NULL default '0',
  "AAAATNFN" int(11) NOT NULL default '0',
  "AAAATNFO" int(11) NOT NULL default '0',
  "AAAATNFP" int(11) NOT NULL default '0',
  "AAAATNFQ" int(11) NOT NULL default '0',
  "AAAATNFR" int(11) NOT NULL default '0',
  "AAAATNFS" int(11) NOT NULL default '0',
  "AAAATNFT" int(11) NOT NULL default '0',
  "AAABHZBX" varchar(16) NOT NULL default ' ',
  "AAABMLDW" varchar(30) NOT NULL default ' ',
  "AAAATNHP" varchar(40) NOT NULL default ' ',
  "AAAATNFX" varchar(4) NOT NULL default ' ',
  "AAABMNMM" int(11) NOT NULL default '0',
  "AAAATNFZ" int(11) NOT NULL default '0',
  "AAAATNGA" int(11) NOT NULL default '0',
  "AAAATNGB" int(11) NOT NULL default '0',
  "AAAATNGC" int(11) NOT NULL default '0',
  "AAAATNGD" int(11) NOT NULL default '0',
  "AAAATNGE" varchar(8) NOT NULL default '00000000',
  "AAAATNGF" varchar(6) NOT NULL default '000000',
  "AAAATNGG" varchar(2) NOT NULL default ' ',
  "AAAATNGH" int(11) NOT NULL default '0',
  "AAAATNGI" int(11) NOT NULL default '0',
  "AAAATNGJ" int(11) NOT NULL default '0',
  "AAAATNGK" int(11) NOT NULL default '0',
  "AAAATNGL" int(11) NOT NULL default '0',
  "AAAATNGM" int(11) NOT NULL default '0',
  "AAAATNGN" int(11) NOT NULL default '0',
  "AAAATNGO" int(11) NOT NULL default '0',
  "AAAATNGP" int(11) NOT NULL default '0',
  "AAAATNGQ" int(11) NOT NULL default '0',
  "AAAATNGR" int(11) NOT NULL default '0',
  "AAAATNGS" int(11) NOT NULL default '0',
  "AAAATNGT" int(11) NOT NULL default '0',
  "AAAATNGU" varchar(8) NOT NULL default '00000000',
  "AAAATNGV" varchar(6) NOT NULL default '000000',
  "AAAATNGW" varchar(2) NOT NULL default ' ',
  "AAAATNGX" int(11) NOT NULL default '0',
  "AAAATNGY" varchar(8) NOT NULL default '00000000',
  "AAAATNGZ" varchar(6) NOT NULL default '000000',
  "AAAATNHA" varchar(2) NOT NULL default ' ',
  "AAABLENB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABFSPQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNHD" (
  "AAABFSPQ" VARBINARY(255) NOT NULL,
  "AAAAVARC" varchar(8) NOT NULL default ' ',
  "AAAAVASQ" varchar(40) NOT NULL default ' ',
  "AAAAVARE" varchar(3) NOT NULL default ' ',
  "AAAAVARF" varchar(10) NOT NULL default ' ',
  "AAAAVARG" varchar(3) NOT NULL default ' ',
  "AAAAVARH" varchar(10) NOT NULL default ' ',
  "AAAAVARI" varchar(10) NOT NULL default ' ',
  "AAAATNHM" int(11) NOT NULL default '0',
  "AAAATNHN" int(11) NOT NULL default '0',
  "AAAATNHO" int(11) NOT NULL default '0',
  "AAAATNHP" varchar(40) NOT NULL default ' ',
  "AAAATNHQ" int(11) NOT NULL default '0',
  "AAAATNHR" varchar(8) NOT NULL default '00000000',
  "AAAATNHS" varchar(6) NOT NULL default '000000',
  "AAAATNHT" varchar(2) NOT NULL default ' ',
  "AAAAVASF" varchar(255) NOT NULL default ' ',
  "AAABLENB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABFSPQ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNHX" (
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNIB" (
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAAATNID" varchar(8) NOT NULL default ' ',
  "AAABGHVZ" varchar(8) NOT NULL default ' ',
  "AAABGHXN" varchar(10) NOT NULL default ' ',
  "AAAATNIG" varchar(60) NOT NULL default ' ',
  "AAABCFJB" varchar(32) NOT NULL default ' ',
  "AAAATNII" varchar(64) NOT NULL default ' ',
  "AAAATNIJ" varchar(64) NOT NULL default ' ',
  "AAAATNIK" varchar(2) NOT NULL default '00',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAATNIM" varchar(255) NOT NULL default ' ',
  "AAAATNIN" decimal(15,0) NOT NULL default '0',
  "AAABCKRR" varchar(12) NOT NULL default ' ',
  "AAABLTXI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNIR" (
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNIV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATNTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNJB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABIFGN" varchar(2) NOT NULL default '00',
  "AAAAUYBS" varchar(3) NOT NULL default '000',
  "AAAAUYBT" varchar(3) NOT NULL default '000',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABGLWW" varchar(1) NOT NULL default ' ',
  "AAAATNJL" varchar(1) NOT NULL default ' ',
  "AAAATNJM" varchar(1) NOT NULL default ' ',
  "AAAATNJN" varchar(1) NOT NULL default ' ',
  "AAAATNJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNJS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABIFGN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNJZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABIFGN" varchar(2) NOT NULL default '00',
  "AAABLPSU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNKJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCTT","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJCTT","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATNOP" varchar(8) NOT NULL default ' ',
  "AAAATOJX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATNOP","AAAATOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCTT","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJCTT","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATOJX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOJX" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAATOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNNG" (
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNNJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNNP" (
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNNS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATOKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOKA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAATOKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNOL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAAATNOO" varchar(42) NOT NULL default ' ',
  "AAAATNOP" varchar(8) NOT NULL default ' ',
  "AAAATNOQ" varchar(8) NOT NULL default ' ',
  "AAAATNOR" varchar(8) NOT NULL default ' ',
  "AAAATNOS" varchar(8) NOT NULL default ' ',
  "AAAATNOT" varchar(8) NOT NULL default ' ',
  "AAAAUXCY" varchar(32) NOT NULL default ' ',
  "AAAATNOV" varchar(32) NOT NULL default ' ',
  "AAAATNOW" varchar(32) NOT NULL default ' ',
  "AAAATNOX" varchar(32) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAATNOZ" varchar(2) NOT NULL default ' ',
  "AAAATNPA" varchar(1) NOT NULL default ' ',
  "AAAATNPB" varchar(1) NOT NULL default ' ',
  "AAAATNPC" varchar(1) NOT NULL default ' ',
  "AAAATNPD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABJCTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNPX" (
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
CREATE TABLE "AAAATNQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNQW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNRB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABMNLW" varchar(30) NOT NULL default ' ',
  "AAABIHNW" varchar(2) NOT NULL default '00',
  "AAABLOKS" varchar(1) NOT NULL default ' ',
  "AAAATNRH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNRL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNRT" (
  "AAABMNLW" varchar(30) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAATNRX" varchar(10) NOT NULL default ' ',
  "AAAATNRY" varchar(1) NOT NULL default ' ',
  "AAAATNRZ" varchar(1) NOT NULL default ' ',
  "AAAATNSA" varchar(1) NOT NULL default ' ',
  "AAAATNSB" varchar(1) NOT NULL default ' ',
  "AAAATNSC" varchar(1) NOT NULL default ' ',
  "AAAATNSD" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNSG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(30) NOT NULL default ' ',
  "AAABEZGI" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNSN" (
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
  KEY "AAAATNTD" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATNUD" varchar(8) NOT NULL default ' ',
  "AAAAUYBS" varchar(3) NOT NULL default '000',
  "AAAAUYBT" varchar(3) NOT NULL default '000',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAAATNTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATNUD","AAAAUYBS","AAAAUYBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATNUD" varchar(8) NOT NULL default ' ',
  "AAAATNTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATNUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATNUD" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAATNUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATNUZ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNVD" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAATNVS" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATNVV" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNVX" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAATNWM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATNWP" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNWR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(8) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNWX" (
  "AAAAUXIL" varchar(32) NOT NULL default ' ',
  "AAABGEXH" varchar(30) NOT NULL default ' ',
  "AAAAUXDW" varchar(32) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXIL"),
  KEY "AAAATNXE" ("AAAAUXEG"),
  KEY "AAAATNXG" ("AAAAUXDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNXI" (
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
CREATE TABLE "AAAATNXW" (
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
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAATNYM" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATNYP" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAATNZH" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATNZK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATNZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATNZY" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOAC" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAATOAR" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATOAU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOAW" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAATOBL" ("AAABMRIL","AAABLLNL"),
  KEY "AAAATOBO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOBQ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOCA" (
  "AAAAUXEC" varchar(32) NOT NULL default ' ',
  "AAAAUXED" varchar(32) NOT NULL default ' ',
  "AAAAUXEE" varchar(2) NOT NULL default ' ',
  "AAAAUXEG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAUXEC","AAAAUXED","AAAAUXEE"),
  KEY "AAAATOCI" ("AAAAUXEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOCK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOCP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOCX" (
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
CREATE TABLE "AAAATODS" (
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
  KEY "AAAATOEU" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOEW" (
  "AAAATOKU" varchar(3) NOT NULL default ' ',
  "AAAATOEY" varchar(1) NOT NULL default ' ',
  "AAAATOEZ" varchar(1) NOT NULL default ' ',
  "AAAATOFA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATOKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOFC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOFF" varchar(3) NOT NULL default ' ',
  "AAAATOFG" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOFJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOKC" varchar(2) NOT NULL default '00',
  "AAAATOKU" varchar(3) NOT NULL default ' ',
  "AAAATOFO" varchar(2) NOT NULL default '00',
  "AAAATOFP" varchar(2) NOT NULL default '00',
  "AAAAUXIU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAAATOKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOFU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOKC" varchar(2) NOT NULL default '00',
  "AAAATOFZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON","AAAATOKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOGE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOGI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOGM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOKU" varchar(3) NOT NULL default ' ',
  "AAAATOGP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATOKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOGS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOGX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOIB" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(73) NOT NULL default ' ',
  "AAAATOHZ" varchar(73) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON","AAAATOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOHK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOHS" (
  "AAAATOIB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOHV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOIB" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(73) NOT NULL default ' ',
  "AAAATOHZ" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOIC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAAATOIG" varchar(3) NOT NULL default '000',
  "AAAATOIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOIL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOIR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABLDON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATOKJ" varchar(32) NOT NULL default ' ',
  "AAAATOJC" varchar(32) NOT NULL default ' ',
  "AAAATOJD" varchar(32) NOT NULL default ' ',
  "AAAATOJE" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAAATOKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATOKJ" varchar(32) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAAATOJM" varchar(30) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABGXSI" decimal(15,0) NOT NULL default '0',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABJQCL" decimal(15,0) NOT NULL default '0',
  "AAAATOJR" varchar(12) NOT NULL default ' ',
  "AAAATOJS" decimal(15,0) NOT NULL default '0',
  "AAAATOJT" varchar(8) NOT NULL default '00000000',
  "AAABEDIF" varchar(12) NOT NULL default ' ',
  "AAABKIKF" varchar(12) NOT NULL default ' ',
  "AAAATOJW" varchar(80) NOT NULL default ' ',
  "AAAATOJX" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAAATOKA" varchar(1) NOT NULL default ' ',
  "AAABLDON" varchar(8) NOT NULL default ' ',
  "AAAATOKC" varchar(2) NOT NULL default '00',
  "AAAATOKU" varchar(3) NOT NULL default ' ',
  "AAAATOKE" varchar(32) NOT NULL default ' ',
  "AAAATOKF" varchar(40) NOT NULL default ' ',
  "AAAATOKG" varchar(40) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATOKJ"),
  KEY "AAAATOKK" ("AAABMRIL","AAABHZRB"),
  KEY "AAAATOKN" ("AAABMRIL","AAABEDIF","AAAATOKU"),
  KEY "AAAATOKR" ("AAABMRIL","AAABKIKF","AAAATOKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOKV" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAABGEUB" varchar(70) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOLE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUVJ","AAABLEIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOLM" (
  "AAAATOMM" varchar(64) NOT NULL default ' ',
  "AAAATOLQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATOMM","AAAATOLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOLR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOMM" varchar(64) NOT NULL default ' ',
  "AAAATOMN" varchar(5) NOT NULL default '00000',
  "AAAATOMO" varchar(5) NOT NULL default '00000',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATOMM","AAAATOMN","AAAATOMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOMB" (
  "AAAATOMM" varchar(64) NOT NULL default ' ',
  "AAAATOMN" varchar(5) NOT NULL default '00000',
  "AAAATOMO" varchar(5) NOT NULL default '00000',
  "AAAATOMF" varchar(70) NOT NULL default ' ',
  "AAAATOMG" varchar(64) NOT NULL default ' ',
  "AAAATOMH" varchar(5) NOT NULL default '00000',
  "AAAATOMI" varchar(5) NOT NULL default '00000',
  "AAAATOMJ" varchar(70) NOT NULL default ' ',
  "AAAATOMK" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATOMM","AAAATOMN","AAAATOMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOMP" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOMT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATONA" (
  "AAAATONJ" varchar(18) NOT NULL default ' ',
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATONJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATONE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATONJ" varchar(18) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATONJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATONK" (
  "AAAATONP" varchar(10) NOT NULL default ' ',
  "AAAATONQ" varchar(10) NOT NULL default ' ',
  "AAAATONR" varchar(5) NOT NULL default '00000',
  "AAAATONO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATONP","AAAATONQ","AAAATONR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATONS" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAAATOOS" varchar(128) NOT NULL default ' ',
  "AAAATOOT" varchar(3) NOT NULL default '000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOOC" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAABIFGN" varchar(5) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAAATOOS" varchar(128) NOT NULL default ' ',
  "AAAATOOT" varchar(3) NOT NULL default '000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOOO" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAAATOOS" varchar(128) NOT NULL default ' ',
  "AAAATOOT" varchar(3) NOT NULL default '000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOOY" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOPG" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAABIFGN" varchar(5) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOPQ" (
  "AAABLUVJ" varchar(64) NOT NULL default ' ',
  "AAABLEIH" varchar(5) NOT NULL default '00000',
  "AAAATOPX" varchar(5) NOT NULL default '00000',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABLEIH","AAAATOPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOPY" (
  "AAAAUJWL" varchar(30) NOT NULL default ' ',
  "AAAATOQA" varchar(1) NOT NULL default ' ',
  "AAAATOQB" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAAATPDY" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATPEB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAUJWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOQJ" (
  "AAAAUJWL" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUJWL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOQP" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAAATOQR" varchar(1) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAAATOQT" varchar(30) NOT NULL default ' ',
  "AAAATOQU" varchar(30) NOT NULL default ' ',
  "AAAATOQV" varchar(30) NOT NULL default ' ',
  "AAAATPDT" varchar(30) NOT NULL default ' ',
  "AAAATPEE" varchar(30) NOT NULL default ' ',
  "AAAATPDV" varchar(20) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAAATPDY" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATPEB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATPGG"),
  KEY "AAAATORG" ("AAAATPEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATORI" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPGG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATORO" (
  "AAAATPEJ" varchar(30) NOT NULL default ' ',
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATPEJ","AAAATPGG"),
  KEY "AAAATORU" ("AAAATPGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATORW" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAAAUJWL" varchar(30) NOT NULL default ' ',
  "AAAATPFV" varchar(30) NOT NULL default ' ',
  "AAAATOZI" varchar(30) NOT NULL default ' ',
  "AAAATPFX" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATPGG","AAAAUJWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOSF" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAAATPDA" varchar(30) NOT NULL default ' ',
  "AAAATPDB" varchar(30) NOT NULL default ' ',
  "AAAATPDC" varchar(1) NOT NULL default ' ',
  "AAAATPDD" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAAATPDY" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATPEB" varchar(6) NOT NULL default '000000',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATPGG","AAABLMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOSV" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPGG","AAABMLQA","AAABLMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOTD" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAAATOTI" varchar(30) NOT NULL default ' ',
  "AAAATOTJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPGG","AAAATOTI","AAAATOTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOTK" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOTT" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAAAVPJH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAAAVPJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOUA" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVPJH" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAABMLQA","AAAAVPJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOUK" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAAATOVN" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAAATOVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOUS" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAAATOVN" varchar(30) NOT NULL default ' ',
  "AAAATOVD" varchar(30) NOT NULL default ' ',
  "AAAATOUX" varchar(2) NOT NULL default '00',
  "AAAAUYTB" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAAATOVN","AAAATOVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOVE" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATOVN" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAABMLQA","AAAATOVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOVO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAVNUK" varchar(8) NOT NULL default '00000000',
  "AAAATOVZ" varchar(10) NOT NULL default '0000000000',
  "AAAAVDBE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABJTDU","AAABJTDV","AAAAVNUK","AAAATOVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOWA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUORP" varchar(10) NOT NULL default ' ',
  "AAABLTQJ" longtext,
  "AAAATOWE" longtext,
  "AAAATOWF" varchar(1) NOT NULL default ' ',
  "AAAATOWG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAUORP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOWJ" (
  "AAABDEYE" varchar(32) NOT NULL default ' ',
  "AAAATOXB" int(11) NOT NULL default '0',
  "AAAATOXC" int(11) NOT NULL default '0',
  "AAAATOWN" int(11) NOT NULL default '0',
  "AAAATOWO" int(11) NOT NULL default '0',
  "AAAATOWP" int(11) NOT NULL default '0',
  "AAAATOWQ" int(11) NOT NULL default '0',
  "AAAATOWR" int(11) NOT NULL default '0',
  "AAAATOWS" int(11) NOT NULL default '0',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAATOWU" varchar(61) NOT NULL default ' ',
  "AAAATOWV" int(11) NOT NULL default '0',
  "AAAATOWW" int(11) NOT NULL default '0',
  "AAAATOWX" int(11) NOT NULL default '0',
  "AAAATOWY" int(11) NOT NULL default '0',
  "AAAATOWZ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABDEYE","AAAATOXB","AAAATOXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOXD" (
  "AAABDEYE" varchar(32) NOT NULL default ' ',
  "AAAATOXF" varchar(3) NOT NULL default '000',
  "AAAATOXG" varchar(12) NOT NULL default ' ',
  "AAAATOXM" varchar(32) NOT NULL default ' ',
  "AAAATOXI" varchar(8) NOT NULL default '00000000',
  "AAAATOXJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABDEYE"),
  KEY "AAAATOXL" ("AAAATOXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOXN" (
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAAAVPJH" varchar(30) NOT NULL default ' ',
  "AAAATPGX" varchar(2) NOT NULL default '00',
  "AAAATPGY" varchar(30) NOT NULL default ' ',
  "AAAATOYE" varchar(2) NOT NULL default '00',
  "AAAATOYF" varchar(30) NOT NULL default ' ',
  "AAABLNHB" varchar(30) NOT NULL default ' ',
  "AAABLNGE" varchar(50) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABBZOV","AAABLNGH","AAAAVPJH","AAAATPGX","AAAATPGY","AAAATOYE","AAAATOYF","AAABLNHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOYH" (
  "AAABLNHB" varchar(30) NOT NULL default ' ',
  "AAAATOYJ" varchar(30) NOT NULL default ' ',
  "AAAATOYK" varchar(1) NOT NULL default ' ',
  "AAAATOYL" varchar(1) NOT NULL default ' ',
  "AAAATOYM" varchar(1) NOT NULL default ' ',
  "AAAATOYN" varchar(1) NOT NULL default ' ',
  "AAAATOYO" varchar(1) NOT NULL default ' ',
  "AAAATOYP" varchar(1) NOT NULL default ' ',
  "AAAATOYQ" varchar(1) NOT NULL default ' ',
  "AAAATOYR" varchar(1) NOT NULL default ' ',
  "AAAATOYS" varchar(1) NOT NULL default ' ',
  "AAAATOYT" varchar(1) NOT NULL default ' ',
  "AAAATOYU" varchar(1) NOT NULL default ' ',
  "AAABLNGO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOYX" (
  "AAABLNHB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNHB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOZD" (
  "AAAATPEJ" varchar(30) NOT NULL default ' ',
  "AAABBYHH" varchar(30) NOT NULL default ' ',
  "AAAATOZG" varchar(30) NOT NULL default ' ',
  "AAAATPFV" varchar(30) NOT NULL default ' ',
  "AAAATOZI" varchar(30) NOT NULL default ' ',
  "AAAATPFX" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATPEJ","AAABBYHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOZN" (
  "AAAATPEJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBYHH" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPEJ","AAABMLQA","AAABBYHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATOZV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(10) NOT NULL default '0000000000',
  "AAABLTYF" varchar(10) NOT NULL default '0000000000',
  "AAABLTYG" varchar(10) NOT NULL default '0000000000',
  "AAAAYGXV" varchar(1) NOT NULL default ' ',
  "AAAAYGXW" varchar(8) NOT NULL default '00000000',
  "AAAATPAC" varchar(10) NOT NULL default ' ',
  "AAAATPAD" int(11) NOT NULL default '0',
  "AAAATPAE" varchar(10) NOT NULL default '0000000000',
  "AAAATPAF" varchar(10) NOT NULL default '0000000000',
  "AAAATPAG" varchar(10) NOT NULL default ' ',
  "AAAATPAH" varchar(30) NOT NULL default ' ',
  "AAAATPAI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTYE","AAABLTYF","AAABLTYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPAN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAATPAW" varchar(32) NOT NULL default ' ',
  "AAABFLHW" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABHJYH" varchar(12) NOT NULL default ' ',
  "AAAATPAT" varchar(8) NOT NULL default '00000000',
  "AAAATPAU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAATPAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPAX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(10) NOT NULL default '0000000000',
  "AAABLTYF" varchar(10) NOT NULL default '0000000000',
  "AAABLTYG" varchar(10) NOT NULL default '0000000000',
  "AAAATPBQ" varchar(10) NOT NULL default '0000000000',
  "AAAATPBD" varchar(1) NOT NULL default ' ',
  "AAAATPBE" varchar(8) NOT NULL default '00000000',
  "AAAATPBF" varchar(10) NOT NULL default ' ',
  "AAAATPBG" int(11) NOT NULL default '0',
  "AAAATPBH" varchar(10) NOT NULL default '0000000000',
  "AAAATPBI" varchar(10) NOT NULL default '0000000000',
  "AAAATPBJ" varchar(10) NOT NULL default ' ',
  "AAAATPBK" varchar(30) NOT NULL default ' ',
  "AAAATPBL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTYE","AAABLTYF","AAABLTYG","AAAATPBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPBR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(10) NOT NULL default '0000000000',
  "AAABLTYF" varchar(10) NOT NULL default '0000000000',
  "AAABLTYG" varchar(10) NOT NULL default '0000000000',
  "AAAATPCW" varchar(10) NOT NULL default '0000000000',
  "AAAATPBX" varchar(1) NOT NULL default ' ',
  "AAAATPBY" varchar(8) NOT NULL default '00000000',
  "AAAATPBZ" varchar(10) NOT NULL default ' ',
  "AAAATPCA" int(11) NOT NULL default '0',
  "AAAATPCB" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABLTYE","AAABLTYF","AAABLTYG","AAAATPCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPCH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(10) NOT NULL default '0000000000',
  "AAABLTYF" varchar(10) NOT NULL default '0000000000',
  "AAABLTYG" varchar(10) NOT NULL default '0000000000',
  "AAAATPCW" varchar(10) NOT NULL default '0000000000',
  "AAAATPCN" varchar(10) NOT NULL default '0000000000',
  "AAAATPCO" varchar(8) NOT NULL default '00000000',
  "AAAATPCP" varchar(10) NOT NULL default ' ',
  "AAAATPCQ" int(11) NOT NULL default '0',
  "AAAATPCR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTYE","AAABLTYF","AAABLTYG","AAAATPCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPCX" (
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAAATPCZ" varchar(30) NOT NULL default ' ',
  "AAAATPDA" varchar(30) NOT NULL default ' ',
  "AAAATPDB" varchar(30) NOT NULL default ' ',
  "AAAATPDC" varchar(1) NOT NULL default ' ',
  "AAAATPDD" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAAATPDY" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATPEB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLMYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPDL" (
  "AAABLMYI" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMYI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPDR" (
  "AAAATPEJ" varchar(30) NOT NULL default ' ',
  "AAAATPDT" varchar(30) NOT NULL default ' ',
  "AAAATPEE" varchar(30) NOT NULL default ' ',
  "AAAATPDV" varchar(20) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAAATPDY" varchar(6) NOT NULL default '000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATPEB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATPEJ"),
  KEY "AAAATPED" ("AAAATPEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPEF" (
  "AAAATPEJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPEJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPEL" (
  "AAAATPFA" varchar(3) NOT NULL default '000',
  "AAABHZBP" varchar(12) NOT NULL default ' ',
  "AAABGBXR" varchar(32) NOT NULL default ' ',
  "AAAATPEP" varchar(1) NOT NULL default ' ',
  "AAAATPEQ" varchar(1) NOT NULL default ' ',
  "AAAATPER" varchar(1) NOT NULL default ' ',
  "AAAATPES" varchar(32) NOT NULL default ' ',
  "AAAATPET" varchar(30) NOT NULL default ' ',
  "AAAATPEU" varchar(61) NOT NULL default ' ',
  "AAAATPEV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPFA","AAABHZBP","AAABGBXR"),
  KEY "AAAATPEZ" ("AAAATPFA","AAABGBXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPFC" (
  "AAAATPFQ" varchar(22) NOT NULL default ' ',
  "AAABCUXD" varchar(1) NOT NULL default ' ',
  "AAAATPFO" varchar(30) NOT NULL default ' ',
  "AAABHGFL" varchar(8) NOT NULL default '00000000',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  "AAAATPFI" VARBINARY(255),
  "AAAATPFJ" longtext,
  "AAAATPFK" longtext,
  "AAAATPFL" longtext,
  PRIMARY KEY ("AAAATPFQ","AAABCUXD","AAAATPFO"),
  KEY "AAAATPFP" ("AAAATPFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPFR" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAAATPGI" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAATPFV" varchar(30) NOT NULL default ' ',
  "AAAATPFW" varchar(30) NOT NULL default ' ',
  "AAAATPFX" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATPGG","AAAATPGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPGB" (
  "AAAATPGG" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATPGI" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPGG","AAABMLQA","AAAATPGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPGJ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAABLNGH" varchar(30) NOT NULL default ' ',
  "AAAATPGX" varchar(2) NOT NULL default '00',
  "AAAATPGY" varchar(30) NOT NULL default ' ',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAAATPHA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABBZOV","AAABLNGH","AAAATPGX","AAAATPGY","AAABLEKS","AAAATPHA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPHC" (
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAABILKA" varchar(50) NOT NULL default ' ',
  "AAAATPHX" varchar(30) NOT NULL default ' ',
  "AAAATPHV" varchar(30) NOT NULL default ' ',
  "AAAATPHI" varchar(90) NOT NULL default ' ',
  "AAAATPHJ" varchar(90) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAAATPHN" varchar(1) NOT NULL default ' ',
  "AAAATPHO" varchar(1) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABJCBR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABMLPV","AAABILKA","AAAATPHX","AAAATPHV"),
  KEY "AAAATPHW" ("AAAATPHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPHY" (
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKXW","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPID" (
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKXW","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPII" (
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKXW","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPIN" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABKWHD" longtext,
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPIX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPJD" (
  "AAABIAET" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIAET","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPJI" (
  "AAAATPKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPJL" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABIAET" varchar(30) NOT NULL default ' ',
  "AAABDTAQ" varchar(80) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABIAET","AAABDTAQ","AAABMMJT"),
  KEY "AAAATPJU" ("AAABIAET","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPJX" (
  "AAAATPKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPKA" (
  "AAAATPKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAATPKK" varchar(8) NOT NULL default '00000000',
  "AAAATPKH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTDU","AAAATPKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPKL" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATPKT" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPKV" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATPLD" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPLF" (
  "AAABLITM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPLI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAAATPLM" varchar(2) NOT NULL default ' ',
  "AAAATPLN" varchar(2) NOT NULL default ' ',
  "AAAATPLO" varchar(1) NOT NULL default ' ',
  "AAAAWRZR" varchar(6) NOT NULL default '000000',
  "AAAAWRZS" varchar(6) NOT NULL default '000000',
  "AAAATPLR" varchar(3) NOT NULL default '000',
  "AAAATPLS" varchar(1) NOT NULL default '0',
  "AAAATPLT" varchar(1) NOT NULL default '0',
  "AAABFEGO" varchar(12) NOT NULL default ' ',
  "AAABGXUV" varchar(1) NOT NULL default ' ',
  "AAAATPLW" varchar(6) NOT NULL default '000000',
  "AAAATPLX" varchar(1) NOT NULL default ' ',
  "AAAATPLY" varchar(1) NOT NULL default ' ',
  "AAAATPLZ" varchar(1) NOT NULL default ' ',
  "AAAATPMA" varchar(1) NOT NULL default ' ',
  "AAAATPMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNGX"),
  KEY "AAAATPME" ("AAABLUXQ","AAABILWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPMH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAATPMY" varchar(25) NOT NULL default ' ',
  "AAAATPMZ" varchar(12) NOT NULL default ' ',
  "AAAATPZC" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(1) NOT NULL default ' ',
  "AAAATPMR" varchar(1) NOT NULL default ' ',
  "AAAATPMS" varchar(1) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  "AAAATPMV" varchar(3) NOT NULL default ' ',
  "AAABCGWY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAAATPMY","AAAATPMZ","AAAATPZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUORP" varchar(10) NOT NULL default ' ',
  "AAAATPNE" decimal(16,4) NOT NULL default '0.0000',
  "AAAATPNF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUORP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPNI" (
  "AAABIZCB" varchar(32) NOT NULL default ' ',
  "AAAATPPJ" varchar(10) NOT NULL default ' ',
  "AAAATPWY" varchar(32) NOT NULL default ' ',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAAAUFWZ" varchar(50) NOT NULL default ' ',
  "AAAAUFXA" varchar(50) NOT NULL default ' ',
  "AAAAUFXB" varchar(50) NOT NULL default ' ',
  "AAAAUFXI" varchar(50) NOT NULL default ' ',
  "AAAATPNV" varchar(1) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZCB","AAAATPPJ"),
  KEY "AAAATPNZ" ("AAAATPWY"),
  KEY "AAAATPOB" ("AAABLFYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPOD" (
  "AAABIZCB" varchar(32) NOT NULL default ' ',
  "AAAATPQU" varchar(8) NOT NULL default '00000000',
  "AAAATPQV" varchar(6) NOT NULL default '000000',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAAATPOI" decimal(15,0) NOT NULL default '0',
  "AAAATPOJ" decimal(15,0) NOT NULL default '0',
  "AAAAUUZZ" int(11) NOT NULL default '0',
  "AAAATPOL" varchar(1) NOT NULL default ' ',
  "AAAATPOM" varchar(20) NOT NULL default ' ',
  "AAAATPON" varchar(20) NOT NULL default ' ',
  "AAAATPOO" varchar(40) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAAATPOQ" varchar(8) NOT NULL default ' ',
  "AAAATPOR" varchar(3) NOT NULL default '000',
  "AAAATPOS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZCB"),
  KEY "AAAATPOU" ("AAABINIL"),
  KEY "AAAATPOW" ("AAAATPQU"),
  KEY "AAAATPOY" ("AAAATPQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPPA" (
  "AAABIZCB" varchar(32) NOT NULL default ' ',
  "AAAATPPJ" varchar(10) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATPPF" varchar(10) NOT NULL default ' ',
  "AAAAXFPL" varchar(1) NOT NULL default ' ',
  "AAAATQAQ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZCB","AAAATPPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPPK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(32) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABJELB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNSU","AAABINIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPPU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATPRM" varchar(10) NOT NULL default ' ',
  "AAABLTOL" varchar(255) NOT NULL default ' ',
  "AAABKNSU" varchar(32) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAAATPQE" varchar(30) NOT NULL default ' ',
  "AAABLJJO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNMH","AAAATPRM"),
  KEY "AAAATPQJ" ("AAABKNSU","AAABLEER","AAABINIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPQN" (
  "AAAATPQW" varchar(8) NOT NULL default ' ',
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAAATPUQ" varchar(32) NOT NULL default ' ',
  "AAAATPQR" varchar(1) NOT NULL default ' ',
  "AAAATPQS" varchar(32) NOT NULL default ' ',
  "AAAAUOLT" varchar(12) NOT NULL default ' ',
  "AAAATPQU" varchar(8) NOT NULL default '00000000',
  "AAAATPQV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATPQW","AAABMONG","AAAATPUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPQZ" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAAATPRD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAAATPRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPRE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATPRM" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKCWA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNMH","AAAATPRM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPRO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(32) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAAATQTX" varchar(130) NOT NULL default ' ',
  "AAABMMJT" varchar(130) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKNSU","AAABLEER","AAABINIK","AAABFGSE","AAAATQTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPSC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAATPUQ" varchar(32) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(130) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAATPUQ","AAABLEER","AAABINIK","AAABMNMH","AAAATQDD","AAABMREF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPSU" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABJSLL" varchar(4) NOT NULL default ' ',
  "AAABFPRE" varchar(40) NOT NULL default ' ',
  "AAAAVBJH" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAAATPTF" varchar(10) NOT NULL default ' ',
  "AAABKNAB" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAATPTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPTL" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHHCQ" varchar(10) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFOUW" varchar(130) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABFOUX" varchar(130) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHHCQ","AAABMNMH","AAABMREF","AAABFOUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPTY" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMIRX","AAABMIRY","AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPUI" (
  "AAABHHCQ" varchar(10) NOT NULL default ' ',
  "AAAATPUQ" varchar(32) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATQTX" varchar(130) NOT NULL default ' ',
  "AAABMMJT" varchar(130) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHCQ","AAAATPUQ","AAABFGSE","AAABMLQA","AAAATQTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPUU" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABHHTI" varchar(40) NOT NULL default ' ',
  "AAABKCWA" varchar(3) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMLMY" smallint(6) NOT NULL default '0',
  "AAABINER" smallint(6) NOT NULL default '0',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPVK" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABHHTI" varchar(40) NOT NULL default ' ',
  "AAABKCWA" varchar(3) NOT NULL default ' ',
  "AAABJFIP" varchar(1) NOT NULL default ' ',
  "AAABMLMY" smallint(6) NOT NULL default '0',
  "AAABINER" smallint(6) NOT NULL default '0',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPVW" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLRQO" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPWA" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPWD" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABLSAX" varchar(10) NOT NULL default ' ',
  "AAAATPWS" varchar(4) NOT NULL default '0000',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABLEPD" varchar(30) NOT NULL default ' ',
  "AAABLEPE" varchar(30) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABMIRY","AAABLSAX","AAAATPWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPWT" (
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAAATPWY" varchar(32) NOT NULL default ' ',
  "AAAATPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEUZ","AAAATPWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPWZ" (
  "AAABLSAX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPXC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLSAX" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLSAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPXI" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPXM" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(130) NOT NULL default ' ',
  "AAAATPXS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAABMLQA","AAABFGSE","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPXY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPYD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGRX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMNRG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPYL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIKDK" varchar(12) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAATPZC" varchar(1) NOT NULL default ' ',
  "AAABGYNX" varchar(2) NOT NULL default ' ',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAABMKLH" varchar(8) NOT NULL default '00000000',
  "AAAAWRZR" varchar(6) NOT NULL default '000000',
  "AAAAWRZS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABIKDK","AAABKNGX","AAAATPZC"),
  KEY "AAAATPYZ" ("AAABLUXQ","AAABKNGX","AAAATPZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPZD" (
  "AAAATPZG" varchar(12) NOT NULL default ' ',
  "AAAATPZF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATPZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPZH" (
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAAATPZJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPZL" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATPZS" varchar(32) NOT NULL default ' ',
  "AAAATPZP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAAATPZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATPZT" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(60) NOT NULL default ' ',
  "AAAATQAB" varchar(30) NOT NULL default ' ',
  "AAAATQAC" varchar(1) NOT NULL default ' ',
  "AAAATQAD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAABMNMH","AAAATQDD"),
  KEY "AAAATQAI" ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQAK" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQAN" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAAATQAQ" varchar(255) NOT NULL default ' ',
  "AAAAXFPL" varchar(1) NOT NULL default ' ',
  "AAABAJUS" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAAAUFWZ" varchar(50) NOT NULL default ' ',
  "AAAAUFXA" varchar(50) NOT NULL default ' ',
  "AAAAUFXB" varchar(50) NOT NULL default ' ',
  "AAAAUFXI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH","AAAATQDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQBC" (
  "AAAATQBH" varchar(32) NOT NULL default ' ',
  "AAABIZHF" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLSBY" longblob NOT NULL,
  PRIMARY KEY ("AAAATQBH","AAABIZHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQBJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQBR" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQBV" (
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABFOUW" varchar(130) NOT NULL default ' ',
  "AAABFOUX" varchar(130) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXIY","AAABFGSE","AAABFOUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQCD" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKCWA" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAABMNMH","AAAATQDD","AAABMREF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQCS" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAAATQDD" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKCWA" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLTXI","AAABMNMH","AAAATQDD","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQDF" (
  "AAABIYCS" varchar(60) NOT NULL default ' ',
  "AAAATQDK" varchar(30) NOT NULL default ' ',
  "AAAATQDI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS","AAAATQDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQDL" (
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQDO" (
  "AAAATQFL" varchar(32) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAAAUQTM" varchar(32) NOT NULL default ' ',
  "AAAATQDT" varchar(2) NOT NULL default '00',
  "AAAATQDU" varchar(1) NOT NULL default ' ',
  "AAAATQDV" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAAAUQUY" decimal(15,0) NOT NULL default '0',
  "AAAAUQUZ" varchar(4) NOT NULL default ' ',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAAAUQVB" decimal(15,0) NOT NULL default '0',
  "AAAAUQVC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQEF" (
  "AAAATQFL" varchar(32) NOT NULL default ' ',
  "AAAAUQTG" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAAATQEJ" varchar(12) NOT NULL default ' ',
  "AAAATQEK" varchar(1) NOT NULL default ' ',
  "AAABLRQO" varchar(32) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATQFL","AAAAUQTG","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQEQ" (
  "AAAATQFL" varchar(32) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABLMLR" varchar(6) NOT NULL default '000000',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABLFYJ" varchar(32) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAAATQEX" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQFL","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQFC" (
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAAATQFL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHK","AAAATQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQFH" (
  "AAAATQFL" varchar(32) NOT NULL default ' ',
  "AAABLZLD" varchar(61) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATQFL","AAABLZLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQGQ" varchar(40) NOT NULL default ' ',
  "AAAATQGR" int(11) NOT NULL default '0',
  "AAAATQFT" varchar(4) NOT NULL default ' ',
  "AAAATQGJ" varchar(10) NOT NULL default '0000000000',
  "AAABHEDX" varchar(8) NOT NULL default '00000000',
  "AAAATQGL" varchar(10) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAATQHO","AAAATQHP","AAAATQGQ","AAAATQGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQGQ" varchar(40) NOT NULL default ' ',
  "AAAATQGR" int(11) NOT NULL default '0',
  "AAAATQGJ" varchar(8) NOT NULL default ' ',
  "AAABHEDX" varchar(5) NOT NULL default '00000',
  "AAAATQGL" varchar(10) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAATQHO","AAAATQHP","AAAATQGQ","AAAATQGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATQHO" varchar(1) NOT NULL default ' ',
  "AAAATQHP" varchar(4) NOT NULL default '0000',
  "AAAATQHJ" varchar(8) NOT NULL default ' ',
  "AAABHMSE" varchar(4) NOT NULL default ' ',
  "AAAATQHL" varchar(8) NOT NULL default ' ',
  "AAABHMQO" varchar(1) NOT NULL default ' ',
  "AAABHMSB" varchar(80) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABLUUW" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAATQHO","AAAATQHP","AAAATQHJ","AAABHMSE","AAAATQHL","AAABHMQO"),
  KEY "AAAATQHN" ("AAAATQHO","AAAATQHP","AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQHT" (
  "AAAAUZBG" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(8) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAATQHX" varchar(8) NOT NULL default '00000000',
  "AAAATQHY" varchar(30) NOT NULL default ' ',
  "AAAAXKVU" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(1) NOT NULL default ' ',
  "AAAATQIB" varchar(1) NOT NULL default ' ',
  "AAAATQIC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQIE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTRT" varchar(25) NOT NULL default ' ',
  "AAAATQIL" varchar(8) NOT NULL default '00000000',
  "AAAATQII" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABFTRT","AAAATQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQIM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTRT" varchar(25) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABKWFM" varchar(8) NOT NULL default '00000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABLELR" varchar(1) NOT NULL default ' ',
  "AAAATQIT" varchar(2) NOT NULL default ' ',
  "AAAATQIU" varchar(2) NOT NULL default ' ',
  "AAABFUAM" varchar(200) NOT NULL default ' ',
  "AAAATQIW" varchar(1) NOT NULL default ' ',
  "AAAATQJC" varchar(20) NOT NULL default ' ',
  "AAAATQIY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABFTRT"),
  KEY "AAAATQJB" ("AAAATQJC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQJD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTRT" varchar(25) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAAATQJO" varchar(12) NOT NULL default ' ',
  "AAAATQJP" varchar(32) NOT NULL default ' ',
  "AAAATQJQ" varchar(32) NOT NULL default ' ',
  "AAAATQJK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABFTRT","AAABIRUN","AAAATQJO","AAAATQJP","AAAATQJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABKYSP" varchar(5) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXJP","AAABKYSP","AAABKYSQ","AAABIZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQKF" (
  "AAABJXED" varchar(5) NOT NULL default ' ',
  "AAAATQKH" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABJXED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQKJ" (
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
  PRIMARY KEY ("AAABMRIL","AAABLUSV"),
  KEY "AAAATQLC" ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQLF" (
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAAATQLI" varchar(20) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQLM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOHM","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQLU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAABKNFD" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABEUWM","AAAATQMS","AAAATQMT","AAABKNFD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQMJ" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(120) NOT NULL default ' ',
  "AAAATQMZ" varchar(8) NOT NULL default '00000000',
  "AAAATQNA" varchar(6) NOT NULL default '000000',
  "AAAATQMP" varchar(20) NOT NULL default ' ',
  "AAAATQMQ" varchar(8) NOT NULL default ' ',
  "AAABEUWM" varchar(10) NOT NULL default ' ',
  "AAAATQMS" varchar(10) NOT NULL default ' ',
  "AAAATQMT" varchar(10) NOT NULL default '0000000000',
  "AAAATQMU" varchar(5) NOT NULL default '00000',
  "AAAATQMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABLPJD","AAABLGCM","AAAATQMZ","AAAATQNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQNB" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLPV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQNK" (
  "AAAATQNW" varchar(12) NOT NULL default ' ',
  "AAAATQNM" varchar(10) NOT NULL default ' ',
  "AAAATQNN" varchar(30) NOT NULL default ' ',
  "AAAATQNO" varchar(18) NOT NULL default ' ',
  "AAABLUYZ" varchar(10) NOT NULL default ' ',
  "AAAATQNQ" varchar(6) NOT NULL default '000000',
  "AAABLVAN" varchar(10) NOT NULL default ' ',
  "AAAATQNS" varchar(6) NOT NULL default '000000',
  "AAAATQNT" varchar(4) NOT NULL default ' ',
  "AAAATQNU" varchar(6) NOT NULL default ' ',
  "AAAATQNV" decimal(13,0) NOT NULL default '0',
  PRIMARY KEY ("AAAATQNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQNX" (
  "AAABESXO" varchar(5) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAATQOC" varchar(11) NOT NULL default ' ',
  "AAABILUK" varchar(40) NOT NULL default ' ',
  "AAABLUVJ" varchar(30) NOT NULL default ' ',
  "AAAATQOF" varchar(5) NOT NULL default ' ',
  "AAABHYMV" varchar(3) NOT NULL default ' ',
  "AAABIMEG" varchar(30) NOT NULL default ' ',
  "AAAATQOI" varchar(128) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAABFLGW" varchar(5) NOT NULL default ' ',
  "AAAATQOL" varchar(1) NOT NULL default ' ',
  "AAAATQOM" varchar(5) NOT NULL default ' ',
  "AAAATQON" varchar(8) NOT NULL default ' ',
  "AAAATQOO" varchar(4) NOT NULL default ' ',
  "AAAATQOP" varchar(4) NOT NULL default ' ',
  "AAAATQOQ" varchar(10) NOT NULL default ' ',
  "AAAATQOR" varchar(10) NOT NULL default ' ',
  "AAABDQKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABESXO","AAABLOSZ","AAABHMQN","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQOX" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABLENH","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQPH" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(10) NOT NULL default '0000000000',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQPP" (
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
CREATE TABLE "AAAATQQB" (
  "AAABLENF" varchar(14) NOT NULL default ' ',
  "AAABLENG" varchar(3) NOT NULL default ' ',
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABLENA" varchar(16) NOT NULL default ' ',
  "AAABLENB" varchar(4) NOT NULL default '0000',
  "AAABLENC" varchar(14) NOT NULL default ' ',
  "AAABMMRH" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABLENF","AAABLENG","AAABLEKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQQM" (
  "AAAATQRK" varchar(1) NOT NULL default '0',
  "AAAATQRL" varchar(6) NOT NULL default '000000',
  "AAAATQRM" varchar(14) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMCXR" varchar(5) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAAATQQT" varchar(14) NOT NULL default ' ',
  "AAAATQQU" varchar(14) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATQQW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQRK","AAAATQRL","AAAATQRM","AAABLENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQRB" (
  "AAAATQRK" varchar(1) NOT NULL default '0',
  "AAAATQRL" varchar(6) NOT NULL default '000000',
  "AAAATQRM" varchar(14) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAAATQRH" varchar(1) NOT NULL default ' ',
  "AAABHCWH" varchar(50) NOT NULL default ' ',
  "AAAATQRJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQRK","AAAATQRL","AAAATQRM","AAABLENH","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQRP" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATQRY" varchar(70) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAATQRT" varchar(10) NOT NULL default ' ',
  "AAAATQRU" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATQRY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQSA" (
  "AAABLLLP" varchar(100) NOT NULL default ' ',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQSE" (
  "AAAATQSP" varchar(10) NOT NULL default ' ',
  "AAAATQSS" varchar(64) NOT NULL default ' ',
  "AAABFVMJ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQSJ" (
  "AAAATQSP" varchar(10) NOT NULL default ' ',
  "AAAATQSQ" varchar(32) NOT NULL default ' ',
  "AAABGGGL" varchar(2) NOT NULL default ' ',
  "AAAATQSS" varchar(64) NOT NULL default ' ',
  "AAABFVMJ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAATQSP","AAAATQSQ","AAABGGGL","AAAATQSS","AAABFVMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQSU" (
  "AAABLRXL" varchar(80) NOT NULL default ' ',
  "AAABLUFA" varchar(80) NOT NULL default ' ',
  "AAABCATV" varchar(80) NOT NULL default ' ',
  "AAABGXFD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL","AAABLUFA","AAABCATV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQTC" (
  "AAABLRXL" varchar(64) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default ' ',
  "AAABMMJT" decimal(17,0) NOT NULL default '0',
  "AAAAUMNK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQTI" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQTN" (
  "AAABJECS" varchar(64) NOT NULL default ' ',
  "AAAATQTZ" int(11) NOT NULL default '0',
  "AAAATQUA" varchar(64) NOT NULL default ' ',
  "AAABFVMJ" varchar(18) NOT NULL default ' ',
  "AAAATQTS" varchar(8) NOT NULL default ' ',
  "AAAATQTT" varchar(8) NOT NULL default ' ',
  "AAAATQTU" varchar(1) NOT NULL default ' ',
  "AAAATQTV" varchar(8) NOT NULL default ' ',
  "AAABHDKR" varchar(64) NOT NULL default ' ',
  "AAAATQTX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABJECS","AAAATQTZ","AAAATQUA","AAABFVMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQUC" (
  "AAABLEKV" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(7) NOT NULL default ' ',
  "AAABLLLP" varchar(80) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABJLOQ" varchar(8) NOT NULL default '00000000',
  "AAABIJJL" varchar(6) NOT NULL default '000000',
  "AAAATQUJ" varchar(1) NOT NULL default ' ',
  "AAAATQUK" varchar(1) NOT NULL default ' ',
  "AAAATQUL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQUN" (
  "AAABLECT" varchar(3) NOT NULL default '000',
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
  "AAABLNRX" varchar(2) NOT NULL default '00',
  "AAAATQVC" varchar(15) NOT NULL default ' ',
  "AAAATQVD" varchar(15) NOT NULL default ' ',
  "AAABGGKN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECT","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQVH" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLECT" varchar(3) NOT NULL default '000',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABGGKN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABLECT","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQVP" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAATQVS" varchar(1) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAAATQVU" varchar(1) NOT NULL default ' ',
  "AAAATQVV" varchar(200) NOT NULL default ' ',
  "AAAATQVW" int(11) NOT NULL default '0',
  "AAAATQVX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQWA" (
  "AAAAVCDW" varchar(32) NOT NULL default ' ',
  "AAAATTUX" varchar(1) NOT NULL default ' ',
  "AAAATQWD" varchar(255) NOT NULL default ' ',
  "AAAATTVA" varchar(1) NOT NULL default ' ',
  "AAABLEKS" varchar(40) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQWI" (
  "AAAAVCDW" varchar(32) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLEKT" varchar(40) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVCDW","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQWP" (
  "AAABGXFD" varchar(32) NOT NULL default ' ',
  "AAAATTVV" varchar(32) NOT NULL default ' ',
  "AAAATQWS" varchar(1) NOT NULL default ' ',
  "AAAATQWT" varchar(1) NOT NULL default ' ',
  "AAAATQWU" varchar(1) NOT NULL default ' ',
  "AAAATQWV" varchar(1) NOT NULL default ' ',
  "AAAATQWW" varchar(1) NOT NULL default ' ',
  "AAAATQWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGXFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQWZ" (
  "AAABLZLD" varchar(60) NOT NULL default ' ',
  "AAAAVQAE" varchar(60) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAVQAF" varchar(120) NOT NULL default ' ',
  "AAABEDJP" varchar(60) NOT NULL default ' ',
  "AAAAVQAC" varchar(120) NOT NULL default ' ',
  "AAAAVQAB" varchar(60) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABLHAH" longtext,
  PRIMARY KEY ("AAABLZLD","AAAAVQAE","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQXN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVCGL" varchar(8) NOT NULL default '00000000',
  "AAAAVCGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAVCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQXV" (
  "AAAATTVV" varchar(32) NOT NULL default ' ',
  "AAAATQYA" varchar(10) NOT NULL default '0000000000',
  "AAABJAAI" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTVV","AAAATQYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQYG" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATRBU" varchar(4) NOT NULL default '0000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAATQYP" varchar(30) NOT NULL default ' ',
  "AAAATQYQ" varchar(1) NOT NULL default ' ',
  "AAAATQYR" varchar(30) NOT NULL default ' ',
  "AAAATQYS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQYV" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATQZO" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAABEIZY" varchar(1) NOT NULL default ' ',
  "AAAATRAD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAAATQZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQZF" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATQZO" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAAATQZQ" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAAATQZO","AAABGFET","AAAATQZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATQZR" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAABIGMV" varchar(1) NOT NULL default ' ',
  "AAAATQZX" varchar(1) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAAAXABN" varchar(30) NOT NULL default ' ',
  "AAAATRBF" varchar(30) NOT NULL default ' ',
  "AAABEIZY" varchar(1) NOT NULL default ' ',
  "AAAATRAD" varchar(30) NOT NULL default ' ',
  "AAAATRAE" varchar(30) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRAJ" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABFYNM" varchar(20) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAAATRAW" varchar(1) NOT NULL default ' ',
  "AAABFZAX" varchar(30) NOT NULL default ' ',
  "AAAATRAQ" varchar(30) NOT NULL default ' ',
  "AAAATRAR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAABFYNM","AAABGFET","AAAATRAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRAX" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATRBF" varchar(30) NOT NULL default ' ',
  "AAAATRBG" varchar(30) NOT NULL default ' ',
  "AAAATRBC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAAATRBF","AAAATRBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRBH" (
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRBK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRBQ" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAAVQDY" varchar(4) NOT NULL default ' ',
  "AAABIJQE" varchar(40) NOT NULL default ' ',
  "AAAATRBU" varchar(4) NOT NULL default '0000',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATRNJ","AAAAVQDY","AAABIJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRCC" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVQDY" varchar(4) NOT NULL default ' ',
  "AAABIJQE" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAABMLQA","AAAAVQDY","AAABIJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRCM" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAAATRGX" varchar(30) NOT NULL default ' ',
  "AAAATRCQ" varchar(20) NOT NULL default ' ',
  "AAABHEIG" varchar(40) NOT NULL default ' ',
  "AAAATRCS" varchar(20) NOT NULL default ' ',
  "AAAATRCT" varchar(40) NOT NULL default ' ',
  "AAAATRCU" varchar(30) NOT NULL default ' ',
  "AAAATRCV" varchar(30) NOT NULL default ' ',
  "AAABEDQH" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAAATRCY" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAAATRDA" varchar(30) NOT NULL default ' ',
  "AAAATRDB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAAATRGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRDF" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAABERKM" varchar(30) NOT NULL default ' ',
  "AAAATREJ" varchar(2) NOT NULL default '00',
  "AAAATREK" varchar(2) NOT NULL default '00',
  "AAAATRDL" varchar(4) NOT NULL default ' ',
  "AAAATRME" varchar(20) NOT NULL default ' ',
  "AAABLMPW" varchar(4) NOT NULL default ' ',
  "AAABLMSB" varchar(40) NOT NULL default ' ',
  "AAABKGBL" varchar(30) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAABERKM","AAAATREJ","AAAATREK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRDX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAABERKM" varchar(30) NOT NULL default ' ',
  "AAAATREJ" varchar(2) NOT NULL default '00',
  "AAAATREK" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATRNJ","AAAATRGW","AAABERKM","AAAATREJ","AAAATREK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATREL" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAABERKM" varchar(30) NOT NULL default ' ',
  "AAAATREP" varchar(20) NOT NULL default ' ',
  "AAAATREQ" varchar(30) NOT NULL default ' ',
  "AAAATRER" varchar(2) NOT NULL default '00',
  "AAAATRES" varchar(2) NOT NULL default '00',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAAATREV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAABERKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATREZ" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAABERKM" varchar(30) NOT NULL default ' ',
  "AAAATRFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRFG" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAAATRGX" varchar(30) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABGICD" varchar(30) NOT NULL default ' ',
  "AAAATRFO" varchar(30) NOT NULL default ' ',
  "AAAATRFP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAABFDFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRFT" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAAATRGX" varchar(30) NOT NULL default ' ',
  "AAAATRGD" varchar(4) NOT NULL default '0000',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAAATRGX","AAAATRGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRGE" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAAATRGX" varchar(30) NOT NULL default ' ',
  "AAAATRGN" varchar(1) NOT NULL default ' ',
  "AAAATRGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRGW","AAAATRGX","AAAATRGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRGO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRGW" varchar(40) NOT NULL default ' ',
  "AAAATRGX" varchar(30) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATRNJ","AAAATRGW","AAAATRGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRGY" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABJLPG" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAAATRHD" varchar(1) NOT NULL default ' ',
  "AAAAUPUZ" varchar(132) NOT NULL default ' ',
  "AAAATRHF" varchar(1) NOT NULL default ' ',
  "AAAATRLG" varchar(1) NOT NULL default ' ',
  "AAAATRLH" varchar(1) NOT NULL default ' ',
  "AAABIMHJ" varchar(8) NOT NULL default ' ',
  "AAAATRLI" varchar(4) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAAATRLK" varchar(1) NOT NULL default ' ',
  "AAAATRHM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATRNJ","AAABJLPG","AAABLGCM","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRHR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABJLPG" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATRNJ","AAABJLPG","AAABLGCM","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRID" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJLPG" varchar(40) NOT NULL default ' ',
  "AAABIMHJ" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATRNJ","AAABMLQA","AAABJLPG","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRIO" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABJLPG" varchar(40) NOT NULL default ' ',
  "AAAATRIR" varchar(4) NOT NULL default ' ',
  "AAAATRME" varchar(20) NOT NULL default ' ',
  "AAAATRMF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRIW" (
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABMNYT" varchar(30) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRJB" (
  "AAAATRME" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRME","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRJH" (
  "AAABLROG" varchar(20) NOT NULL default ' ',
  "AAAATRJJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRJL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRJR" (
  "AAABLROG" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLROG","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRJW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLROG","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRKE" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABJGOK" varchar(40) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABHNSS" varchar(3) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(38) NOT NULL default ' ',
  "AAABHNSV" varchar(3) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAABJGOK","AAABFDFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRKS" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABJGOK" varchar(40) NOT NULL default ' ',
  "AAAATRME" varchar(20) NOT NULL default ' ',
  "AAAATRMF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAABJGOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRKZ" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRMH" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLFTX" varchar(4) NOT NULL default '0000',
  "AAAATRLG" varchar(1) NOT NULL default ' ',
  "AAAATRLH" varchar(1) NOT NULL default ' ',
  "AAAATRLI" varchar(4) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAAATRLK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRMH","AAABLGCM","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRLP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRMH" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATRNJ","AAAATRMH","AAABLGCM","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRMB" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAAATRMH" varchar(40) NOT NULL default ' ',
  "AAAATRME" varchar(20) NOT NULL default ' ',
  "AAAATRMF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAAATRMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRMI" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABHEIG" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGFET" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATRNJ","AAABHEIG","AAABLGCM","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRMU" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABHEIG" varchar(40) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRNJ","AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRNA" (
  "AAAATRNJ" varchar(20) NOT NULL default ' ',
  "AAABHEIG" varchar(40) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(30) NOT NULL default ' ',
  "AAAATRNG" varchar(30) NOT NULL default ' ',
  "AAABEDQH" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAATRNJ","AAABHEIG","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRNM" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAATROL" varchar(10) NOT NULL default '0000000000',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAAATROJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAAATROL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRNU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAATROL" varchar(10) NOT NULL default '0000000000',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPTB","AAAATROL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATROC" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAAATROL" varchar(10) NOT NULL default '0000000000',
  "AAAATROM" varchar(10) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAAATROH" decimal(16,4) NOT NULL default '0.0000',
  "AAAATROI" decimal(16,4) NOT NULL default '0.0000',
  "AAAATROJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAAATROL","AAAATROM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRON" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATROU" (
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
CREATE TABLE "AAAATRPF" (
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  "AAAATRPM" varchar(1) NOT NULL default ' ',
  "AAAATSJF" varchar(1) NOT NULL default ' ',
  "AAAATRPO" varchar(2) NOT NULL default '00',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAAATRPQ" varchar(1) NOT NULL default ' ',
  "AAABIYDX" varchar(3) NOT NULL default ' ',
  "AAABDVWP" varchar(4) NOT NULL default ' ',
  "AAABLLLA" varchar(15) NOT NULL default ' ',
  "AAABGFIS" varchar(1) NOT NULL default ' ',
  "AAAATRPV" varchar(1) NOT NULL default ' ',
  "AAAAVEGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRPZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRQJ" (
  "AAAATRRN" varchar(10) NOT NULL default ' ',
  "AAAATRRO" varchar(1) NOT NULL default ' ',
  "AAAATRRP" varchar(34) NOT NULL default ' ',
  "AAABDSSV" VARBINARY(255) NOT NULL,
  "AAAATRRF" varchar(2) NOT NULL default ' ',
  "AAAATRRG" VARBINARY(255),
  "AAAATRRH" varchar(72) NOT NULL default ' ',
  "AAAATRRI" VARBINARY(255),
  "AAAATRRJ" varchar(4) NOT NULL default ' ',
  "AAAATRRK" VARBINARY(255),
  "AAAATRRL" varchar(10) NOT NULL default ' ',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRRN","AAAATRRO","AAAATRRP","AAABDSSV"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRRA" (
  "AAAATRRN" varchar(10) NOT NULL default ' ',
  "AAAATRRO" varchar(1) NOT NULL default ' ',
  "AAAATRRP" varchar(34) NOT NULL default ' ',
  "AAABDSSV" VARBINARY(255) NOT NULL,
  "AAAATRRF" varchar(2) NOT NULL default ' ',
  "AAAATRRG" VARBINARY(255),
  "AAAATRRH" varchar(72) NOT NULL default ' ',
  "AAAATRRI" VARBINARY(255),
  "AAAATRRJ" varchar(4) NOT NULL default ' ',
  "AAAATRRK" VARBINARY(255),
  "AAAATRRL" varchar(10) NOT NULL default ' ',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRRN","AAAATRRO","AAAATRRP","AAABDSSV"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  "AAABLLNH" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLLNF","AAABHZDJ","AAABHZJH"),
  KEY "AAAATRSG" ("AAABHZDJ"),
  KEY "AAAATRSI" ("AAABMRIL","AAABLLNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRSL" (
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  "AAAATRSQ" varchar(10) NOT NULL default ' ',
  "AAAATRSO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKIV","AAAATRSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRSR" (
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAATRSU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRSW" (
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAAAVMFR" varchar(100) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default '00000',
  "AAAATTMJ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABGFJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRTC" (
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAAATRTF" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATRTK" varchar(30) NOT NULL default ' ',
  "AAAATRVW" varchar(30) NOT NULL default ' ',
  "AAAATRVX" varchar(30) NOT NULL default ' ',
  "AAAATRTN" varchar(30) NOT NULL default ' ',
  "AAAATRTO" varchar(30) NOT NULL default ' ',
  "AAAATRTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRTR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGFJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRTX" (
  "AAABMOHM" varchar(100) NOT NULL default ' ',
  "AAABJAAI" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(100) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUH" (
  "AAABMOHM" varchar(100) NOT NULL default ' ',
  "AAABJAAI" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUL" (
  "AAAATRUT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATRUT" varchar(4) NOT NULL default ' ',
  "AAAATRUR" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATRUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUU" (
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSTI","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRUZ" (
  "AAABLLNF" decimal(21,7) NOT NULL default '0.0000000',
  "AAABHZDJ" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAABHZDC" varchar(10) NOT NULL default '0000000000',
  "AAABHZDD" varchar(4) NOT NULL default '0000',
  "AAABLLNH" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABLLNF","AAABHZDJ","AAABHZJH"),
  KEY "AAAATRVM" ("AAABHZDJ"),
  KEY "AAAATRVO" ("AAABLLNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRVQ" (
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATRVW" varchar(30) NOT NULL default ' ',
  "AAAATRVX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRVZ" (
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATRWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRWH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATSTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRWN" (
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATRWT" varchar(1) NOT NULL default ' ',
  "AAAAVEGN" varchar(1) NOT NULL default ' ',
  "AAAAURYG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRWX" (
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
  KEY "AAAATRXL" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRXP" (
  "AAABMBXO" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXO"),
  KEY "AAAATRXT" ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRXV" (
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAAATRZZ" varchar(10) NOT NULL default ' ',
  "AAAATRXY" varchar(30) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAAATRYA" varchar(30) NOT NULL default ' ',
  "AAAATRYB" varchar(30) NOT NULL default ' ',
  "AAAATRYC" varchar(30) NOT NULL default ' ',
  "AAAATRYD" varchar(30) NOT NULL default ' ',
  "AAAATRYE" varchar(30) NOT NULL default ' ',
  "AAAATRYF" varchar(30) NOT NULL default ' ',
  "AAAATRYG" varchar(30) NOT NULL default ' ',
  "AAAATRYH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRYJ" (
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLLMF","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRZF" (
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLNL" varchar(32) NOT NULL default ' ',
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLMF","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRZR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATRZX" (
  "AAAATRZZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATRZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSAA" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSAK" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSAO" (
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSAR" (
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATSAY" varchar(30) NOT NULL default ' ',
  "AAAATSAZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSBB" (
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAAVMFR" varchar(100) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default '00000',
  "AAAATTMJ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABLLLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSBL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLKIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSBR" (
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
CREATE TABLE "AAAATSCK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSCR" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSCZ" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSDJ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSDN" (
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
  UNIQUE KEY "AAAATSEA" ("AAABLLNL"),
  KEY "AAAATSEC" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSEE" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSER" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSEV" (
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
  UNIQUE KEY "AAAATSFI" ("AAABLLNL"),
  KEY "AAAATSFK" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSFM" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSFZ" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSGD" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTCA" varchar(30) NOT NULL default ' ',
  "AAAATTCB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSGI" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTLO" varchar(30) NOT NULL default ' ',
  "AAAATTLP" varchar(30) NOT NULL default ' ',
  "AAAATTCJ" varchar(30) NOT NULL default ' ',
  "AAAATTCK" varchar(30) NOT NULL default ' ',
  "AAAATTHS" varchar(30) NOT NULL default ' ',
  "AAAATTHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSGR" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATSGU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSGX" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUZGD" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATSHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAAAUZGD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSHF" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUZGD" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAAATSHM" varchar(1) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAAAUZGD","AAABLTXI","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSHW" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUZGD" varchar(30) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(2) NOT NULL default '00',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAAAUZGD","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSIG" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSIM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAUZGD" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM","AAAAUZGD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSIW" (
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  "AAABLECD" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(3) NOT NULL default ' ',
  "AAAATSJA" varchar(4) NOT NULL default ' ',
  "AAAATSJB" varchar(40) NOT NULL default ' ',
  "AAAATSJC" varchar(4) NOT NULL default ' ',
  "AAAATSJD" varchar(40) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAATSJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSJH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSJN" (
  "AAABLLMF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAAATSKE" varchar(1) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLTXI","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSKA" (
  "AAABLLMF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAAATSKE" varchar(1) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLTXI","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSKN" (
  "AAABLLMF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLTXI","AAABLEEW","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSKZ" (
  "AAABLLMF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLTXI","AAABLEEW","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSLL" (
  "AAABJAAI" varchar(64) NOT NULL default ' ',
  "AAABMOHM" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSLP" (
  "AAABJAAI" varchar(64) NOT NULL default ' ',
  "AAABMOHM" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSLT" (
  "AAABLLMF" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSLZ" (
  "AAAATSNI" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATSNK" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSNI","AAABLTXI","AAAATSNK","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSMI" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAAATYRB" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSMU" (
  "AAAATSNI" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATSNK" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSNI","AAABLTXI","AAAATSNK","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSND" (
  "AAAATSNI" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAAATSNK" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSNI","AAABLTXI","AAAATSNK","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSNM" (
  "AAAATSOH" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSOH","AAABLTXI","AAABMOHM","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSNZ" (
  "AAAATSOH" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAATSOH","AAABLTXI","AAABMOHM","AAABLEEW","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSON" (
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
  KEY "AAAATSQD" ("AAABJAAI"),
  KEY "AAAATSQF" ("AAABMRHK","AAABHZRB"),
  KEY "AAAATSQI" ("AAABHZRB","AAABLFYJ"),
  KEY "AAAATSQL" ("AAABHZOJ"),
  KEY "AAAATSQN" ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSQP" (
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
  KEY "AAAATSRL" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSRO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAATSSC" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSSF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAAATTCY" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABMLQA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSSS" (
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATSSY" varchar(1) NOT NULL default '0',
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABHZIT" varchar(4) NOT NULL default ' ',
  "AAAATSTC" varchar(30) NOT NULL default ' ',
  "AAAATSTD" varchar(30) NOT NULL default ' ',
  "AAAATSTE" varchar(30) NOT NULL default ' ',
  "AAAATSTF" varchar(30) NOT NULL default ' ',
  "AAAATSTU" varchar(12) NOT NULL default '000000000000',
  "AAAATSTH" varchar(30) NOT NULL default ' ',
  "AAAATSTI" varchar(10) NOT NULL default ' ',
  "AAAATSTJ" varchar(1) NOT NULL default ' ',
  "AAAATSTK" varchar(1) NOT NULL default ' ',
  "AAAATSTL" varchar(1) NOT NULL default ' ',
  "AAAATSTM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSTO" (
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATSTU" varchar(12) NOT NULL default '000000000000',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSTX" (
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
CREATE TABLE "AAAATSUJ" (
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
CREATE TABLE "AAAATSUX" (
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
  UNIQUE KEY "AAAATSVL" ("AAABLLNL"),
  KEY "AAAATSVN" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSVP" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSWC" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSWG" (
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
  KEY "AAAATSXG" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSXI" (
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
  UNIQUE KEY "AAAATSXW" ("AAABLLNL"),
  KEY "AAAATSXY" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSYA" (
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
  KEY "AAAATSYP" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSYT" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATSZD" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSZH" (
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
  UNIQUE KEY "AAAATSZU" ("AAABLLNL"),
  KEY "AAAATSZW" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATSZY" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATTAL" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTAP" (
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
  UNIQUE KEY "AAAATTBC" ("AAABLLNL"),
  KEY "AAAATTBE" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTBG" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATTBT" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTBX" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTBZ" varchar(30) NOT NULL default ' ',
  "AAAATTCA" varchar(30) NOT NULL default ' ',
  "AAAATTCB" varchar(30) NOT NULL default ' ',
  "AAAATTCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTCE" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTCG" varchar(30) NOT NULL default ' ',
  "AAAATTLO" varchar(30) NOT NULL default ' ',
  "AAAATTCI" varchar(30) NOT NULL default ' ',
  "AAAATTCJ" varchar(30) NOT NULL default ' ',
  "AAAATTCK" varchar(30) NOT NULL default ' ',
  "AAAATTCL" varchar(30) NOT NULL default ' ',
  "AAAATTCM" varchar(30) NOT NULL default ' ',
  "AAAATTCN" varchar(30) NOT NULL default ' ',
  "AAAATTHS" varchar(30) NOT NULL default ' ',
  "AAAATTCP" varchar(30) NOT NULL default ' ',
  "AAAATTHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTCS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAAATTDD" varchar(4) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAAATTCY" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAAATTDD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTDF" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZCR" varchar(80) NOT NULL default ' ',
  "AAABMMJT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMDEN","AAABMRIL","AAABLZCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTDP" (
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTDR" varchar(2) NOT NULL default '00',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTDV" (
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  "AAAATTDZ" varchar(1) NOT NULL default ' ',
  "AAAATTEA" varchar(1) NOT NULL default ' ',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMF","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTEE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTEK" (
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTEX" varchar(32) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLMD","AAAATTEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTEQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTEX" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLMD","AAAATTEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTEY" (
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
  KEY "AAAATTFK" ("AAABLLNN"),
  KEY "AAAATTFM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTFO" (
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABMLRB" varchar(2) NOT NULL default '00',
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTFX" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATTGP" varchar(32) NOT NULL default ' ',
  "AAAATTGQ" varchar(10) NOT NULL default '0000000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAAATTGI" varchar(10) NOT NULL default ' ',
  "AAAATTGJ" varchar(30) NOT NULL default ' ',
  "AAAATTGK" varchar(1) NOT NULL default ' ',
  "AAAATTGL" varchar(10) NOT NULL default '0000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAAATTGP","AAAATTGQ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTGS" (
  "AAABLLNB" varchar(10) NOT NULL default ' ',
  "AAAATTGZ" varchar(10) NOT NULL default ' ',
  "AAAATTHA" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLLNB","AAAATTGZ","AAAATTHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTHB" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATTHL" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTHP" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTLO" varchar(30) NOT NULL default ' ',
  "AAAATTHS" varchar(30) NOT NULL default ' ',
  "AAAATTHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTHV" (
  "AAAATTLL" varchar(32) NOT NULL default ' ',
  "AAAATTLI" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAATTLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTID" (
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAAATTLE" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABHZIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTIL" (
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABHZIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTIS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHZIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTIY" (
  "AAABHZIR" varchar(10) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTJC" (
  "AAAATTLI" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTJK" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATTJU" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTJY" (
  "AAAATTLL" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAAATTLI" varchar(10) NOT NULL default ' ',
  "AAAATTKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLL","AAABLLMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTKF" (
  "AAAATTLE" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATTKL" int(11) NOT NULL default '0',
  "AAAATTKM" int(11) NOT NULL default '0',
  "AAAATTKN" int(11) NOT NULL default '0',
  "AAAATTKO" varchar(2) NOT NULL default ' ',
  "AAAATTKP" varchar(30) NOT NULL default ' ',
  "AAABLITM" varchar(100) NOT NULL default ' ',
  "AAAATTKR" varchar(255) NOT NULL default ' ',
  "AAABDXKV" varchar(12) NOT NULL default ' ',
  "AAAATTKT" varchar(20) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default ' ',
  "AAAATTKV" varchar(32) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABHGII" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTKZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATTLE" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATTLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTLF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATTLL" varchar(32) NOT NULL default ' ',
  "AAAATTLI" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATTLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTLM" (
  "AAAATTLR" varchar(30) NOT NULL default ' ',
  "AAAATTLO" varchar(30) NOT NULL default ' ',
  "AAAATTLP" varchar(30) NOT NULL default ' ',
  "AAAATTLQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTLS" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTLX" (
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAAAVMFR" varchar(64) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default '00000',
  "AAAATTMJ" varchar(5) NOT NULL default '00000',
  "AAABHATP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFJD","AAAAVMFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTMF" (
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAAAVMFR" varchar(64) NOT NULL default ' ',
  "AAAATTMI" varchar(5) NOT NULL default '00000',
  "AAAATTMJ" varchar(5) NOT NULL default '00000',
  "AAABHATP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLO","AAAAVMFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTMN" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZCK" varchar(12) NOT NULL default ' ',
  "AAABHZCL" varchar(14) NOT NULL default '00000000000000',
  "AAABHZCM" varchar(1) NOT NULL default ' ',
  "AAABHZCN" varchar(1) NOT NULL default ' ',
  "AAABLECQ" varchar(15) NOT NULL default ' ',
  "AAABHZCP" varchar(255) NOT NULL default ' ',
  "AAABHZCQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTNA" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLQA","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTNM" (
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
  KEY "AAAATTOA" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAATTOE" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTOG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABHZIS" varchar(4) NOT NULL default ' ',
  "AAABHZGP" varchar(10) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABICGK" varchar(12) NOT NULL default '000000000000',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  "AAABHZGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTOU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAATTPK" ("AAABLLNL"),
  KEY "AAAATTPM" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTPO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTQP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAATTRF" ("AAABLLNL"),
  KEY "AAAATTRH" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTRJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAATTRV" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTRZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAATTSO" ("AAABLLNL"),
  KEY "AAAATTSQ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTSS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAATTTH" ("AAABLLNL"),
  KEY "AAAATTTJ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTTL" (
  "AAABKMQH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTTO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKMQH" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKMQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTTU" (
  "AAAATTUD" varchar(5) NOT NULL default ' ',
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTTY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATTUD" varchar(5) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAATTUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTUE" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAATTUJ" varchar(10) NOT NULL default ' ',
  "AAAATTUK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAAATTUJ","AAAATTUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTUL" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTUO" (
  "AAABLZLD" varchar(60) NOT NULL default ' ',
  "AAAAVQAE" varchar(60) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAAAVQAF" varchar(120) NOT NULL default ' ',
  "AAABEDJP" varchar(60) NOT NULL default ' ',
  "AAAAVQAC" varchar(120) NOT NULL default ' ',
  "AAAAVQAB" varchar(60) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255),
  "AAAATTUX" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAATTUZ" varchar(40) NOT NULL default ' ',
  "AAAATTVA" varchar(1) NOT NULL default ' ',
  "AAAATTVB" varchar(255) NOT NULL default ' ',
  "AAAATTVC" varchar(255) NOT NULL default ' ',
  "AAABEDLS" varchar(1) NOT NULL default ' ',
  "AAAATTVE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZLD","AAAAVQAE","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTVI" (
  "AAABLRXL" varchar(80) NOT NULL default ' ',
  "AAABLUFA" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRXL","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTVN" (
  "AAAATTVV" varchar(32) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAAATTVQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTVS" (
  "AAAATTVV" varchar(32) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABIFRO" varchar(12) NOT NULL default ' ',
  "AAABENSM" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABIFRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAAATTWI" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABJSDF" smallint(6) NOT NULL default '0',
  "AAABMMJT" text,
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAAATTWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAATTXE" varchar(1) NOT NULL default ' ',
  "AAABLTVR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMKIQ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAABMKXW","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTXY" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAATTYD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTYG" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAVNUK" varchar(8) NOT NULL default '00000000',
  "AAAATTYY" varchar(8) NOT NULL default '00000000',
  "AAAATTYZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTDU","AAABJTDV","AAAAVNUK","AAAATTYY","AAAATTYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAATTZU" varchar(4) NOT NULL default ' ',
  "AAAATTZE" varchar(4) NOT NULL default ' ',
  "AAAATTZF" varchar(3) NOT NULL default ' ',
  "AAABCFKC" varchar(3) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAATTZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAATTZU" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAAATTZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATTZV" (
  "AAAATTZY" varchar(30) NOT NULL default ' ',
  "AAAATUAA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAATTZY"),
  KEY "AAAATTZZ" ("AAAATUAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUAB" (
  "AAAATUAK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAATUAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUAE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATUAK" varchar(3) NOT NULL default ' ',
  "AAABJGOK" varchar(12) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAATUAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATUDY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATUDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATUDY" varchar(5) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATUDY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATUDY" varchar(5) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAATUBC" varchar(1) NOT NULL default ' ',
  "AAAATUBD" varchar(1) NOT NULL default ' ',
  "AAAATUBE" varchar(1) NOT NULL default ' ',
  "AAAATUBF" varchar(1) NOT NULL default ' ',
  "AAAATUBG" varchar(1) NOT NULL default ' ',
  "AAAATUBH" varchar(1) NOT NULL default ' ',
  "AAAATUBI" varchar(1) NOT NULL default ' ',
  "AAAATUBJ" varchar(1) NOT NULL default ' ',
  "AAAATUBK" varchar(1) NOT NULL default ' ',
  "AAAATUBL" varchar(1) NOT NULL default ' ',
  "AAAATUBM" varchar(1) NOT NULL default ' ',
  "AAAATUBN" varchar(1) NOT NULL default ' ',
  "AAAATUBO" varchar(1) NOT NULL default ' ',
  "AAAATUBP" varchar(1) NOT NULL default ' ',
  "AAAATUBQ" varchar(1) NOT NULL default ' ',
  "AAAATUBR" varchar(1) NOT NULL default ' ',
  "AAAATUBS" varchar(1) NOT NULL default ' ',
  "AAAATUBT" varchar(1) NOT NULL default ' ',
  "AAAATUBU" varchar(1) NOT NULL default ' ',
  "AAAATUBV" varchar(1) NOT NULL default ' ',
  "AAAATUBW" varchar(1) NOT NULL default ' ',
  "AAAATUBX" varchar(1) NOT NULL default ' ',
  "AAAATUBY" varchar(1) NOT NULL default ' ',
  "AAAATUBZ" varchar(1) NOT NULL default ' ',
  "AAAATUCA" varchar(1) NOT NULL default ' ',
  "AAAATUCB" varchar(1) NOT NULL default ' ',
  "AAAATUCC" varchar(1) NOT NULL default ' ',
  "AAAATUCD" varchar(1) NOT NULL default ' ',
  "AAAATUCE" varchar(1) NOT NULL default ' ',
  "AAAATUCF" varchar(1) NOT NULL default ' ',
  "AAAATUCG" varchar(1) NOT NULL default ' ',
  "AAAATUCH" varchar(1) NOT NULL default ' ',
  "AAAATUCI" varchar(1) NOT NULL default ' ',
  "AAAATUCJ" varchar(1) NOT NULL default ' ',
  "AAAATUCK" varchar(1) NOT NULL default ' ',
  "AAAATUCL" varchar(1) NOT NULL default ' ',
  "AAAATUCM" varchar(1) NOT NULL default ' ',
  "AAAATUCN" varchar(1) NOT NULL default ' ',
  "AAAATUCO" varchar(1) NOT NULL default ' ',
  "AAAATUCP" varchar(1) NOT NULL default ' ',
  "AAAATUCQ" varchar(1) NOT NULL default ' ',
  "AAAATUCR" varchar(1) NOT NULL default ' ',
  "AAAATUCS" varchar(1) NOT NULL default ' ',
  "AAAATUCT" varchar(1) NOT NULL default ' ',
  "AAAATUCU" varchar(1) NOT NULL default ' ',
  "AAAATUCV" varchar(1) NOT NULL default ' ',
  "AAAATUCW" varchar(1) NOT NULL default ' ',
  "AAAATUCX" varchar(1) NOT NULL default ' ',
  "AAAATUCY" varchar(1) NOT NULL default ' ',
  "AAAATUCZ" varchar(1) NOT NULL default ' ',
  "AAAATUDA" varchar(1) NOT NULL default ' ',
  "AAAATUDB" varchar(1) NOT NULL default ' ',
  "AAAATUDC" varchar(1) NOT NULL default ' ',
  "AAAATUDD" varchar(1) NOT NULL default ' ',
  "AAAATUDE" varchar(1) NOT NULL default ' ',
  "AAAATUDF" varchar(1) NOT NULL default ' ',
  "AAAATUDG" varchar(1) NOT NULL default ' ',
  "AAAATUDH" varchar(1) NOT NULL default ' ',
  "AAAATUDI" varchar(1) NOT NULL default ' ',
  "AAAATUDJ" varchar(1) NOT NULL default ' ',
  "AAAATUDK" varchar(1) NOT NULL default ' ',
  "AAAATUDL" varchar(1) NOT NULL default ' ',
  "AAAATUDM" varchar(1) NOT NULL default ' ',
  "AAAATUDN" varchar(1) NOT NULL default ' ',
  "AAAAWJLT" varchar(1) NOT NULL default ' ',
  "AAAATUDP" varchar(1) NOT NULL default ' ',
  "AAAATUDQ" varchar(1) NOT NULL default ' ',
  "AAAATUDR" varchar(1) NOT NULL default ' ',
  "AAAATUDS" varchar(1) NOT NULL default ' ',
  "AAAATUDT" varchar(1) NOT NULL default ' ',
  "AAAATUDU" varchar(12) NOT NULL default ' ',
  "AAAATUDV" varchar(12) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATUDY","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUEA" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUEH" varchar(1) NOT NULL default '0',
  "AAABLROU" VARBINARY(255),
  "AAABHION" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA"),
  KEY "AAAATUEG" ("AAAATUEH"),
  KEY "AAAATUEI" ("AAABLROU"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUEK" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAABLRXL" varchar(2) NOT NULL default '00',
  "AAAATUSP" varchar(1) NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAABILHK" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATUEV" varchar(12) NOT NULL default ' ',
  "AAAATUEW" varchar(8) NOT NULL default '00000000',
  "AAAATUEX" varchar(1) NOT NULL default ' ',
  "AAAATUEY" varchar(1) NOT NULL default ' ',
  "AAAATUEZ" varchar(1) NOT NULL default ' ',
  "AAAATUFA" varchar(1) NOT NULL default ' ',
  "AAAATUFB" varchar(61) NOT NULL default ' ',
  "AAAATUFC" varchar(61) NOT NULL default ' ',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAAATUFE" varchar(1) NOT NULL default ' ',
  "AAAATUFF" varchar(1) NOT NULL default ' ',
  "AAAATUFG" varchar(1) NOT NULL default ' ',
  "AAAAZMTC" varchar(1) NOT NULL default ' ',
  "AAAATUFI" varchar(30) NOT NULL default ' ',
  "AAAATUHB" varchar(10) NOT NULL default ' ',
  "AAAATUFK" varchar(1) NOT NULL default ' ',
  "AAAATUFL" varchar(2) NOT NULL default '00',
  "AAAATUFM" varchar(1) NOT NULL default ' ',
  "AAAATUFN" varchar(30) NOT NULL default ' ',
  "AAABIYZP" varchar(20) NOT NULL default ' ',
  "AAAATUFP" varchar(1) NOT NULL default ' ',
  "AAAATUFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUFT" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABGGLM" VARBINARY(255),
  "AAABLHMS" int(11) NOT NULL default '0',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAAATURS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUGA" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUGG" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLMPW" varchar(1) NOT NULL default '0',
  "AAAATUPZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABEYPA","AAABLMSB"),
  KEY "AAAATUGN" ("AAABEYPA","AAABLMPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUGQ" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAABEBCL" varchar(1) NOT NULL default ' ',
  "AAAATUGV" varchar(1) NOT NULL default ' ',
  "AAAATUSP" varchar(1) NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAABFDFC" int(11) NOT NULL default '0',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAAATURR" int(11) NOT NULL default '0',
  "AAAATUHB" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATUHG" varchar(1) NOT NULL default '0',
  "AAAATUMQ" varchar(1) NOT NULL default ' ',
  "AAAATUMR" varchar(1) NOT NULL default ' ',
  "AAAATUHJ" varchar(1) NOT NULL default ' ',
  "AAAATUHK" varchar(1) NOT NULL default ' ',
  "AAAATUHL" varchar(1) NOT NULL default ' ',
  "AAAATUHM" varchar(1) NOT NULL default ' ',
  "AAAATUHN" varchar(1) NOT NULL default '0',
  "AAAATUHO" varchar(1) NOT NULL default ' ',
  "AAAATUHP" varchar(30) NOT NULL default ' ',
  "AAAATUHQ" varchar(30) NOT NULL default ' ',
  "AAAATUHR" varchar(1) NOT NULL default ' ',
  "AAAATUMS" varchar(132) NOT NULL default ' ',
  "AAAATUHT" varchar(1) NOT NULL default ' ',
  "AAAATUHU" varchar(1) NOT NULL default ' ',
  "AAAATUHV" varchar(1) NOT NULL default ' ',
  "AAAATUHW" varchar(1) NOT NULL default '0',
  "AAAATUHX" varchar(1) NOT NULL default '0',
  "AAAATUHY" varchar(1) NOT NULL default ' ',
  "AAAATUQW" varchar(1) NOT NULL default '0',
  "AAABMOHM" varchar(132) NOT NULL default ' ',
  "AAAATUQY" varchar(1) NOT NULL default ' ',
  "AAAATUQZ" int(11) NOT NULL default '0',
  "AAAATURA" smallint(6) NOT NULL default '0',
  "AAAATURB" int(11) NOT NULL default '0',
  "AAAATURC" smallint(6) NOT NULL default '0',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAAATUIH" varchar(30) NOT NULL default ' ',
  "AAAATUNE" varchar(30) NOT NULL default ' ',
  "AAAATUIJ" varchar(1) NOT NULL default ' ',
  "AAAATUIK" varchar(2) NOT NULL default '00',
  "AAAATUIL" varchar(1) NOT NULL default ' ',
  "AAAATUIM" varchar(1) NOT NULL default ' ',
  "AAAATUIN" varchar(1) NOT NULL default '0',
  "AAAATUIO" varchar(2) NOT NULL default '00',
  "AAAATUIP" smallint(6) NOT NULL default '0',
  "AAABJXVD" text,
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABLTXI"),
  KEY "AAAATUIU" ("AAAATUNX","AAAATUNE"),
  KEY "AAAATUIX" ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUIZ" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAAATURR" int(11) NOT NULL default '0',
  "AAAATURS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLMSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUJG" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUJO" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAABHVUJ" varchar(1) NOT NULL default '0',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABEYPA","AAAATUNX","AAABHVUJ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUKC" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAAATUKH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUKL" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAAATULP" varchar(30) NOT NULL default ' ',
  "AAAATULB" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAABEPEY" varchar(30) NOT NULL default ' ',
  "AAAATUKV" varchar(30) NOT NULL default ' ',
  "AAAATUKW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABLTXI","AAAATULP","AAAATULB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATULC" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAAATULP" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATULK" varchar(1) NOT NULL default ' ',
  "AAAATULL" varchar(1) NOT NULL default ' ',
  "AAAATULM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLTXI","AAAATULP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATULQ" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABEDQH" varchar(1) NOT NULL default '0',
  "AAAATUPI" varchar(30) NOT NULL default ' ',
  "AAAATUSP" varchar(1) NOT NULL default '0',
  "AAAATUMC" varchar(1) NOT NULL default ' ',
  "AAAATUMD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAAATUNX","AAABLTXI"),
  KEY "AAAATUMH" ("AAAATUNX"),
  KEY "AAAATUMJ" ("AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUML" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAAATUMW" varchar(61) NOT NULL default ' ',
  "AAAATUMQ" varchar(1) NOT NULL default ' ',
  "AAAATUMR" varchar(1) NOT NULL default ' ',
  "AAAATUMS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAAATUNX","AAABLTXI","AAAATUMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUMX" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUNY" varchar(5) NOT NULL default '00000',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAAATUNB" varchar(61) NOT NULL default ' ',
  "AAABLMPW" varchar(1) NOT NULL default '0',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAAATUNE" varchar(30) NOT NULL default ' ',
  "AAAATUNF" varchar(61) NOT NULL default ' ',
  "AAAATUNG" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(3) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  "AAABIAOH" varchar(60) NOT NULL default ' ',
  "AAAATUNO" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAAATUNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUNR" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUNX" varchar(30) NOT NULL default ' ',
  "AAAATUNY" varchar(5) NOT NULL default '00000',
  "AAABLNXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAAATUNX","AAAATUNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUNZ" (
  "AAAATUPI" varchar(30) NOT NULL default ' ',
  "AAABCHRR" varchar(1) NOT NULL default '0',
  "AAAATUSP" varchar(1) NOT NULL default '0',
  "AAABLVCU" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATUPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUOJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATUPI" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATUPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUOP" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUPI" varchar(30) NOT NULL default ' ',
  "AAAATUOS" varchar(1) NOT NULL default ' ',
  "AAAATUOT" varchar(10) NOT NULL default ' ',
  "AAAATUOU" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABEYPA","AAAATUPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUPB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUPI" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEYPA","AAAATUPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUPJ" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAAUZEN" varchar(2) NOT NULL default '00',
  "AAAATUPT" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABEYPA","AAAAUZEN","AAAATUPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUPU" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAABLMPW" varchar(1) NOT NULL default '0',
  "AAAATUPZ" varchar(1) NOT NULL default '0',
  "AAAATUQH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABEYNJ"),
  KEY "AAAATUQE" ("AAABEYPA","AAABLMSB","AAAATUQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUQI" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAABFDFC" int(11) NOT NULL default '0',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAAATURR" int(11) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATUQU" varchar(1) NOT NULL default '0',
  "AAAATUQV" varchar(1) NOT NULL default '0',
  "AAAATUQW" varchar(1) NOT NULL default '0',
  "AAABMOHM" varchar(132) NOT NULL default ' ',
  "AAAATUQY" varchar(1) NOT NULL default ' ',
  "AAAATUQZ" int(11) NOT NULL default '0',
  "AAAATURA" smallint(6) NOT NULL default '0',
  "AAAATURB" int(11) NOT NULL default '0',
  "AAAATURC" smallint(6) NOT NULL default '0',
  "AAABHDKR" varchar(132) NOT NULL default ' ',
  "AAAATURE" varchar(1) NOT NULL default ' ',
  "AAAATURF" varchar(1) NOT NULL default ' ',
  "AAAATURG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABEYNJ","AAABLTXI"),
  KEY "AAAATURL" ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATURN" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAAATURR" int(11) NOT NULL default '0',
  "AAAATURS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABEYNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATURW" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAABLMSB","AAABEYNJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUSG" (
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAAATUST" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default '0',
  "AAAATUSK" varchar(1) NOT NULL default '0',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAAATUSP" varchar(1) NOT NULL default '0',
  "AAAAUZTB" varchar(1) NOT NULL default ' ',
  "AAAATUSR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEYPA","AAAATUST","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABKNQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAAATUTS" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAATUUY" varchar(4) NOT NULL default '0000',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  KEY "AAAATUTQ" ("AAABMRIL","AAAATUTS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAAATUUL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAATUUY" varchar(4) NOT NULL default '0000',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAAAWTBN" varchar(4) NOT NULL default ' ',
  "AAAAWTBO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  KEY "AAAATUUJ" ("AAABMRIL","AAAATUUL","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABMCIM" varchar(10) NOT NULL default ' ',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAATUUY" varchar(4) NOT NULL default '0000',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  "AAAAWTAT" varchar(10) NOT NULL default ' ',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAAATUXX" varchar(4) NOT NULL default ' ',
  "AAAATVAC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  KEY "AAAATUVI" ("AAABMRIL","AAABLVIN"),
  KEY "AAAATUVL" ("AAABMRIL","AAAAWTAT"),
  KEY "AAAATUVO" ("AAABMRIL","AAABMCIM","AAABMCME","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABKNTE" varchar(12) NOT NULL default '000000000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  KEY "AAAATUWD" ("AAABMRIL","AAABKNTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAAAWTBF" varchar(12) NOT NULL default ' ',
  "AAAAWTBG" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  "AAAAWTBH" varchar(2) NOT NULL default '00',
  "AAAAWTBI" varchar(4) NOT NULL default ' ',
  "AAAATUWR" varchar(4) NOT NULL default ' ',
  "AAAAWTBK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  UNIQUE KEY "AAAATUWV" ("AAABMRIL","AAAAWTBF","AAAAWTBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABLFQM" varchar(10) NOT NULL default ' ',
  "AAABJSIW" varchar(6) NOT NULL default '000000',
  "AAABJSCN" varchar(20) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMQYE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  UNIQUE KEY "AAAATUXL" ("AAABMRIL","AAABLFQM","AAABJSIW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAAAWKZV" varchar(3) NOT NULL default '000',
  "AAABMCME" varchar(4) NOT NULL default '0000',
  "AAAATUYJ" varchar(1) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAAATUXX" varchar(4) NOT NULL default ' ',
  "AAAATVAC" varchar(10) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  UNIQUE KEY "AAAATUYE" ("AAABMRIL","AAABLSCM","AAAAWKZV","AAABMCME","AAAATUYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAAATUYT" varchar(1) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABLXTS" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKNQA"),
  UNIQUE KEY "AAAATUYZ" ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABMLEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNQA" int(11) NOT NULL default '0',
  "AAAATUZP" varchar(4) NOT NULL default '0000',
  "AAABLXTF" varchar(8) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAATUZM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNQA","AAAATUZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATUZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXTF" varchar(18) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAAAZUKK" varchar(4) NOT NULL default ' ',
  "AAAATVAC" varchar(10) NOT NULL default ' ',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAAATVAE" varchar(10) NOT NULL default ' ',
  "AAAATVAF" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAAATVAP" varchar(12) NOT NULL default ' ',
  "AAAATVAQ" varchar(8) NOT NULL default '00000000',
  "AAAATVAR" varchar(12) NOT NULL default ' ',
  "AAAATVAS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABMRHK" varchar(6) NOT NULL default '000000',
  "AAAATVBA" varchar(1) NOT NULL default ' ',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIOAE","AAABLGEY","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAAATVDH" varchar(4) NOT NULL default ' ',
  "AAAATVBK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIOAE","AAAATVDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABCKOQ" varchar(178) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABCKOQ","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABFQPC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEY","AAABMLQA","AAABMLPV"),
  KEY "AAAATVCJ" ("AAABLGEY","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEY" varchar(16) NOT NULL default ' ',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAABGRSI" varchar(6) NOT NULL default '000000',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABMRGO" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(32) NOT NULL default ' ',
  "AAAAVAJL" varchar(8) NOT NULL default ' ',
  "AAAATVCZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABDSAH" varchar(4) NOT NULL default ' ',
  "AAAATVDH" varchar(4) NOT NULL default ' ',
  "AAAAUKDK" varchar(12) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIOAE","AAABDSAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVDP" (
  "AAABEDVF" varchar(32) NOT NULL default ' ',
  "AAAATVDR" varchar(5) NOT NULL default ' ',
  "AAAATVDS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVDU" (
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAAATVDW" varchar(2) NOT NULL default '00',
  "AAAATVDX" varchar(30) NOT NULL default ' ',
  "AAAATVDY" varchar(30) NOT NULL default ' ',
  "AAAATVDZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVEB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  "AAAATVEF" varchar(40) NOT NULL default ' ',
  "AAAATVEG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFIQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAAATVEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(28) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVFC" (
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAATVFC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVFE" (
  "AAABMLPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVFH" (
  "AAABMLPV" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAAATVFS" varchar(1) NOT NULL default ' ',
  "AAAATVFT" varchar(1) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAAATVFV" varchar(1) NOT NULL default ' ',
  "AAABBJJX" varchar(24) NOT NULL default ' ',
  "AAABJAEN" varchar(12) NOT NULL default ' ',
  "AAABJAEL" varchar(8) NOT NULL default '00000000',
  "AAABLOCX" varchar(6) NOT NULL default '000000',
  "AAABBJJR" varchar(12) NOT NULL default ' ',
  "AAABBJJS" varchar(8) NOT NULL default '00000000',
  "AAAATVGC" varchar(6) NOT NULL default '000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAAATVGH" varchar(1) NOT NULL default ' ',
  "AAAATVGI" varchar(1) NOT NULL default ' ',
  "AAAATVGJ" varchar(1) NOT NULL default ' ',
  "AAAATVGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM"),
  KEY "AAAATVGP" ("AAABMRIL","AAABHUGE"),
  KEY "AAAATVGS" ("AAABMRIL","AAABJVQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABKVKI" varchar(8) NOT NULL default '00000000',
  "AAABGHGT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAABGBBM" varchar(10) NOT NULL default '0000000000',
  "AAAATVHZ" varchar(1) NOT NULL default ' ',
  "AAAATVIA" varchar(2) NOT NULL default ' ',
  "AAAATVIB" varchar(24) NOT NULL default ' ',
  "AAABDHON" varchar(24) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGBBM" varchar(10) NOT NULL default '0000000000',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM","AAABMLQA","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABJVQM" varchar(24) NOT NULL default ' ',
  "AAABGBBM" varchar(10) NOT NULL default '0000000000',
  "AAAATVJO" varchar(4) NOT NULL default ' ',
  "AAAATVJP" varchar(12) NOT NULL default ' ',
  "AAAATVJQ" varchar(24) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABIHQW","AAABJVQM","AAABGBBM"),
  KEY "AAAATVJM" ("AAABMRIL","AAAATVJO","AAAATVJP","AAAATVJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBMUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" tinyint(3) unsigned NOT NULL default '0',
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
CREATE TABLE "AAAATVKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIAEU","AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABMMVU","AAABLQVJ","AAABIAEU"),
  KEY "AAAATVLN" ("AAABMRIL","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVLQ" (
  "AAAATVNH" varchar(10) NOT NULL default ' ',
  "AAAATVMU" varchar(4) NOT NULL default ' ',
  "AAAATVML" varchar(40) NOT NULL default ' ',
  "AAAATVMV" varchar(4) NOT NULL default ' ',
  "AAAATVMN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVNH","AAAATVMU","AAAATVML","AAAATVMV","AAAATVMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVMB" (
  "AAAATVNH" varchar(10) NOT NULL default ' ',
  "AAAATVMU" varchar(4) NOT NULL default ' ',
  "AAAATVML" varchar(40) NOT NULL default ' ',
  "AAAATVMV" varchar(4) NOT NULL default ' ',
  "AAAATVMN" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVNH","AAAATVMU","AAAATVML","AAAATVMV","AAAATVMN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVMP" (
  "AAAATVNH" varchar(10) NOT NULL default ' ',
  "AAAATVMU" varchar(4) NOT NULL default ' ',
  "AAAATVMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVNH","AAAATVMU","AAAATVMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVMW" (
  "AAAATVNH" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVNH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVNC" (
  "AAAATVNH" varchar(10) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAAATVNF" varchar(1) NOT NULL default ' ',
  "AAAATVNG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVNI" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVNL" (
  "AAABGSIS" varchar(8) NOT NULL default ' ',
  "AAABGSIT" varchar(4) NOT NULL default ' ',
  "AAABGSIU" varchar(8) NOT NULL default ' ',
  "AAABGSIV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGSIS","AAABGSIT","AAABGSIU","AAABGSIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVNU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABJAQI" varchar(6) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABHFAP" varchar(2) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHDEA" varchar(2) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABJAQI","AAABLUVJ","AAABHFAP","AAABLEHI","AAABMRFQ","AAABMRFR","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVOO" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAAATVOS" varchar(4) NOT NULL default ' ',
  "AAAATVOT" varchar(4) NOT NULL default ' ',
  "AAAATVOU" varchar(20) NOT NULL default ' ',
  "AAAATVOV" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMNDU" varchar(8) NOT NULL default ' ',
  "AAAATVOY" varchar(14) NOT NULL default ' ',
  "AAAATVOZ" varchar(30) NOT NULL default ' ',
  "AAAATVPA" varchar(14) NOT NULL default ' ',
  "AAAATVPB" varchar(6) NOT NULL default ' ',
  "AAAATVPC" varchar(1) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  "AAABEMNK" varchar(1) NOT NULL default ' ',
  "AAABLMOZ" varchar(1) NOT NULL default ' ',
  "AAABFYIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVPJ" (
  "AAAATVPS" varchar(30) NOT NULL default ' ',
  "AAAATVPT" varchar(6) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABLUVE" smallint(6) NOT NULL default '0',
  "AAAAUPPD" varchar(1) NOT NULL default ' ',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAABMNMM" int(11) NOT NULL default '0',
  "AAABMMJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVPS","AAAATVPT","AAABMJWN","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATVQB" varchar(30) NOT NULL default ' ',
  "AAABCQHT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATVQB","AAABCQHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABJAAI" varchar(25) NOT NULL default ' ',
  "AAAATVSY" varchar(32) NOT NULL default ' ',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAABMMYV","AAABLUEO","AAABJAAI"),
  UNIQUE KEY "AAAATVQS" ("AAABMRIL","AAAATVSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATVSY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABKPLY" varchar(1) NOT NULL default ' ',
  "AAABGFMQ" varchar(30) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAAATVRE" varchar(1) NOT NULL default ' ',
  "AAABMLDZ" varchar(61) NOT NULL default ' ',
  "AAAATVRG" varchar(1) NOT NULL default ' ',
  "AAABMLEA" varchar(61) NOT NULL default ' ',
  "AAAATVRI" varchar(1) NOT NULL default ' ',
  "AAABMLEB" varchar(61) NOT NULL default ' ',
  "AAAATVRK" varchar(1) NOT NULL default ' ',
  "AAABMLEC" varchar(61) NOT NULL default ' ',
  "AAAATVRM" varchar(32) NOT NULL default ' ',
  "AAAATVRN" int(11) NOT NULL default '0',
  "AAAATVRO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATVSY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATVSY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLFWZ" varchar(32) NOT NULL default ' ',
  "AAABLECU" varchar(12) NOT NULL default ' ',
  "AAABLECV" varchar(12) NOT NULL default ' ',
  "AAABIVVC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATVSY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATVSY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATVSY","AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATVSY" varchar(32) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABGFMQ" varchar(61) NOT NULL default ' ',
  "AAAATVST" varchar(3) NOT NULL default '000',
  "AAAATVSU" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATVSY","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVTA" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVTH" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" text,
  PRIMARY KEY ("AAABLUNI","AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW","AAAATVUS","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVTY" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUS" varchar(4) NOT NULL default '0000',
  "AAAATVUE" varchar(1) NOT NULL default ' ',
  "AAAATVUF" varchar(60) NOT NULL default ' ',
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
CREATE TABLE "AAAATVUT" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAAATVUY" varchar(1) NOT NULL default ' ',
  "AAAATVUZ" varchar(4) NOT NULL default '0000',
  "AAAATVVA" varchar(4) NOT NULL default '0000',
  "AAAATVVL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAAATVWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVVG" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAAATVVL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVVP" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAAATVVZ" varchar(1) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVVZ","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVWB" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABGGIZ" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAABLPJD","AAABGGIZ","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVWM" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(60) NOT NULL default ' ',
  "AAAATVWW" varchar(1) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAAATVWW","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVWY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATVXI" varchar(40) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVXE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATVXI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVXJ" (
  "AAAAUYDF" varchar(3) NOT NULL default ' ',
  "AAAAVCGO" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAAAUYDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVXN" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATVXV" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVXX" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATVYF" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVYH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUYDF" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUYDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVYN" (
  "AAAATVYY" varchar(4) NOT NULL default ' ',
  "AAAATVYP" varchar(8) NOT NULL default '00000000',
  "AAAATVYQ" varchar(4) NOT NULL default ' ',
  "AAAATVYR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAATVYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVYT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATVYY" varchar(4) NOT NULL default ' ',
  "AAAATVYW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATVYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAABBQOP" varchar(8) NOT NULL default '00000000',
  "AAAAVDBF" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAWAWX" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAABBQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATVZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAABBQOP" varchar(8) NOT NULL default '00000000',
  "AAABICEF" varchar(4) NOT NULL default '0000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAVNUK" varchar(8) NOT NULL default '00000000',
  "AAABIYEN" varchar(10) NOT NULL default ' ',
  "AAAAVDNK" varchar(15) NOT NULL default ' ',
  "AAAATWGQ" varchar(15) NOT NULL default ' ',
  "AAAATWGR" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAVDBE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAABBQOP","AAABICEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAAATWBF" varchar(4) NOT NULL default '0000',
  "AAAAUOGB" varchar(3) NOT NULL default ' ',
  "AAABJTDJ" varchar(20) NOT NULL default ' ',
  "AAABJTDK" varchar(3) NOT NULL default ' ',
  "AAAAUOGE" varchar(3) NOT NULL default ' ',
  "AAABJTDL" varchar(20) NOT NULL default ' ',
  "AAABJTDM" varchar(3) NOT NULL default ' ',
  "AAABJTDN" int(11) NOT NULL default '0',
  "AAABJTDO" varchar(6) NOT NULL default '000000',
  "AAABJTDP" varchar(6) NOT NULL default '000000',
  "AAABJTDQ" decimal(9,4) NOT NULL default '0.0000',
  "AAABJTDR" varchar(3) NOT NULL default ' ',
  "AAABLUTZ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAAATWBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAAATWBF" varchar(4) NOT NULL default '0000',
  "AAAATWBG" varchar(1) NOT NULL default '0',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAATWBC" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAAATWBF","AAAATWBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWBH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" varchar(70) NOT NULL default ' ',
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAATWBZ" ("AAAAUYGY","AAAAUYHA","AAAAUYHC"),
  KEY "AAAATWCD" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAATWCH" ("AAAAUYGY","AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH"),
  KEY "AAAATWCP" ("AAABLUXQ","AAABDODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  "AAAAVNUK" varchar(8) NOT NULL default '00000000',
  "AAABFYUL" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAUORP" varchar(10) NOT NULL default ' ',
  "AAAATWDA" int(11) NOT NULL default '0',
  "AAAATWDB" int(11) NOT NULL default '0',
  "AAAATWDC" decimal(17,2) NOT NULL default '0.00',
  "AAAATWDD" int(11) NOT NULL default '0',
  "AAAATWDE" int(11) NOT NULL default '0',
  "AAAATWDF" int(11) NOT NULL default '0',
  "AAAATWDG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJTDU","AAABJTDV","AAAAVNUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWDL" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVCGL" varchar(8) NOT NULL default '00000000',
  "AAABJTDV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAVCGL","AAABJTDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWDU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" varchar(32) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABJTDK" varchar(3) NOT NULL default ' ',
  "AAABJTDM" varchar(3) NOT NULL default ' ',
  "AAAATWEA" varchar(3) NOT NULL default ' ',
  "AAABJTDJ" varchar(20) NOT NULL default ' ',
  "AAABJTDL" varchar(20) NOT NULL default ' ',
  "AAAATWED" varchar(20) NOT NULL default ' ',
  "AAABHDAD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"),
  UNIQUE KEY "AAAATWEH" ("AAABLUXQ","AAABLTVR"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWEK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAAATWGU" varchar(8) NOT NULL default '00000000',
  "AAAAVDBF" varchar(8) NOT NULL default '00000000',
  "AAAATWFJ" varchar(4) NOT NULL default '0000',
  "AAAATWFK" varchar(8) NOT NULL default '00000000',
  "AAAATWFL" varchar(4) NOT NULL default '0000',
  "AAAATWFM" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAATWFO" smallint(6) NOT NULL default '0',
  "AAAATWFP" smallint(6) NOT NULL default '0',
  "AAAATWFQ" smallint(6) NOT NULL default '0',
  "AAAATWFR" decimal(15,2) NOT NULL default '0.00',
  "AAAATWFS" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAWAWX" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAAATWGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAAATWGU" varchar(8) NOT NULL default '00000000',
  "AAAATWGJ" varchar(2) NOT NULL default '00',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVDBE" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAAATWGU","AAAATWGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVEEU" varchar(8) NOT NULL default '00000000',
  "AAAATWGU" varchar(8) NOT NULL default '00000000',
  "AAAATWGV" varchar(4) NOT NULL default '0000',
  "AAAATWGP" varchar(25) NOT NULL default ' ',
  "AAAATWGQ" varchar(15) NOT NULL default ' ',
  "AAAATWGR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAVEEU","AAAATWGU","AAAATWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(61) NOT NULL default ' ',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMMRH" varchar(5) NOT NULL default '00000',
  "AAABGBKQ" varchar(15) NOT NULL default ' ',
  "AAAATWHI" varchar(61) NOT NULL default ' ',
  "AAABMNMW" varchar(2) NOT NULL default '00',
  "AAAAUNUN" varchar(5) NOT NULL default '00000',
  "AAABMOHM" varchar(61) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWHR" (
  "AAAATWIK" varchar(4) NOT NULL default ' ',
  "AAABDMTZ" varchar(60) NOT NULL default ' ',
  "AAABJTFT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAATWIK","AAABDMTZ","AAABJTFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWHY" (
  "AAAATWIK" varchar(4) NOT NULL default ' ',
  "AAABDMTZ" varchar(60) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAATWIK","AAABDMTZ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWIF" (
  "AAAATWIK" varchar(4) NOT NULL default ' ',
  "AAABDMTZ" varchar(60) NOT NULL default ' ',
  "AAAATWIM" varchar(4) NOT NULL default ' ',
  "AAAATWIN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAATWIK","AAABDMTZ","AAAATWIM","AAAATWIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWIO" (
  "AAABCPAO" varchar(100) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAAATWIR" varchar(1) NOT NULL default ' ',
  "AAAATWIS" varchar(1) NOT NULL default ' ',
  "AAAATWIT" varchar(1) NOT NULL default ' ',
  "AAAATWIU" varchar(1) NOT NULL default ' ',
  "AAAATWIV" varchar(1) NOT NULL default ' ',
  "AAABFTTR" varchar(1) NOT NULL default ' ',
  "AAAATWIX" varchar(1) NOT NULL default ' ',
  "AAAATWIY" varchar(1) NOT NULL default ' ',
  "AAAATWIZ" varchar(1) NOT NULL default ' ',
  "AAAATWJA" varchar(1) NOT NULL default ' ',
  "AAABDPCG" varchar(1) NOT NULL default ' ',
  "AAAATWJC" varchar(4) NOT NULL default ' ',
  "AAABLLNR" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(1) NOT NULL default ' ',
  "AAABLEDY" varchar(12) NOT NULL default ' ',
  "AAAATWJG" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABAGFD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABCPAO"),
  KEY "AAAATWJK" ("AAABLLNR","AAABLEDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWJN" (
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWJR" (
  "AAABCPAO" varchar(100) NOT NULL default ' ',
  "AAABLTQC" varchar(12) NOT NULL default ' ',
  "AAAATWJU" varchar(1) NOT NULL default ' ',
  "AAAATWJV" varchar(1) NOT NULL default ' ',
  "AAAATWJW" varchar(1) NOT NULL default ' ',
  "AAAATWJX" varchar(1) NOT NULL default ' ',
  "AAAATWJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCPAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWKA" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSTP" varchar(20) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJSTN" double NOT NULL default '0',
  "AAABJSTM" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJSTP","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHATI" varchar(3) NOT NULL default '000',
  "AAABDNQK" varchar(64) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABMDEP","AAABHATI","AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWLC" (
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABIYJK" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH","AAABMDXP","AAABIYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHATI" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(32) NOT NULL default ' ',
  "AAAATWLS" varchar(32) NOT NULL default ' ',
  "AAAAVQDY" varchar(10) NOT NULL default ' ',
  "AAAATWLU" varchar(32) NOT NULL default ' ',
  "AAAATWLV" varchar(32) NOT NULL default ' ',
  "AAAATWLW" varchar(32) NOT NULL default ' ',
  "AAAATWLX" varchar(32) NOT NULL default ' ',
  "AAABLEIT" varchar(40) NOT NULL default ' ',
  "AAAATWLZ" varchar(40) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABMDEP","AAABHATI","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWMJ" (
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABMOHM" tinyint(3) unsigned NOT NULL default '0',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAAATWMO" varchar(12) NOT NULL default ' ',
  "AAAATWMP" varchar(12) NOT NULL default ' ',
  "AAAATWMZ" varchar(1) NOT NULL default ' ',
  "AAAATWNA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWMT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABMRFO" tinyint(3) unsigned NOT NULL default '0',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAAATWMZ" varchar(1) NOT NULL default ' ',
  "AAAATWNA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWNO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAVJTQ" varchar(12) NOT NULL default ' ',
  "AAABJTNV" varchar(70) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAVJTQ","AAABJTNV","AAABLPJD","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWOB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAATWOF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABIDCQ" varchar(30) NOT NULL default ' ',
  "AAAATWON" varchar(60) NOT NULL default ' ',
  "AAAAUOPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABMPMH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWPC" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGGKN" varchar(80) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERIX" (
  "AAABGHVZ" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWPN" (
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWPQ" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWPV" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABLENG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWPZ" (
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGSE","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWQF" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWQK" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWQP" (
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGSE","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWQU" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWQZ" (
  "AAAATWYP" varchar(30) NOT NULL default ' ',
  "AAABLTQF" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLTQG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATWYP","AAABLTQF","AAABMOHM","AAABLTQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(35) NOT NULL default ' ',
  "AAAATXBP" int(11) NOT NULL default '0',
  "AAABEFTK" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAATXBF" varchar(1) NOT NULL default ' ',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABFQOM" varchar(1) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABLRPQ" varchar(132) NOT NULL default ' ',
  "AAAATXBL" tinyint(3) unsigned NOT NULL default '0',
  "AAAATXBM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV","AAABMREF","AAAATXBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWSH" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(132) NOT NULL default ' ',
  "AAAATXBP" int(11) NOT NULL default '0',
  "AAABEFTK" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAATXBF" varchar(1) NOT NULL default ' ',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABFQOM" varchar(1) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABLRPQ" varchar(132) NOT NULL default ' ',
  "AAAATXBL" tinyint(3) unsigned NOT NULL default '0',
  "AAAATXBM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV","AAABMREF","AAAATXBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABLOJZ" varchar(40) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAAATWVL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV","AAABLOJZ","AAABJCFN","AAAATWVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWTV" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABLOJZ" varchar(40) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAAATWVL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV","AAABLOJZ","AAABJCFN","AAAATWVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWUK" (
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABLOJZ" varchar(40) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAAATWVL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUHG","AAABLESS","AAABMFSV","AAABLOJZ","AAABJCFN","AAAATWVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABLOJZ" varchar(40) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAAATWVL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUHG","AAABLESS","AAABMFSV","AAABLOJZ","AAABJCFN","AAAATWVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATWWV" varchar(12) NOT NULL default ' ',
  "AAABMNMH" varchar(10) NOT NULL default ' ',
  "AAABEFTK" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAAATWXL" varchar(1) NOT NULL default ' ',
  "AAAATWWS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATWWV","AAABMNMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWVY" (
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAATXFU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATWWV" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABEFTK" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAAATWXL" varchar(1) NOT NULL default ' ',
  "AAAATWWS" varchar(1) NOT NULL default ' ',
  "AAABMIRY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATWWV","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAAATWXL" varchar(1) NOT NULL default ' ',
  "AAABEFTK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAAATWXL" varchar(1) NOT NULL default ' ',
  "AAABEFTK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWXP" (
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATWXW" varchar(4) NOT NULL default '0000',
  "AAABMMRH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXFU","AAABMLQA","AAAATWXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWXX" (
  "AAAATWYP" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(128) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAATWYC" int(11) NOT NULL default '0',
  "AAAATWYD" smallint(6) NOT NULL default '0',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATWYP","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWYJ" (
  "AAAATWYP" varchar(30) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMMRH" varchar(10) NOT NULL default '0000000000',
  "AAAAVDNB" int(11) NOT NULL default '0',
  "AAABLSBY" VARBINARY(255),
  PRIMARY KEY ("AAAATWYP","AAABLTVR","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWYS" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABILZU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWZF" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABILZU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWZK" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWZO" (
  "AAABJSPH" varchar(15) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWZT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJSPH" varchar(15) NOT NULL default ' ',
  "AAAATWZW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATWZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABJSPH" varchar(15) NOT NULL default ' ',
  "AAAATXAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIOAE","AAABJSPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXAN" (
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAAATXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXAZ" (
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(132) NOT NULL default ' ',
  "AAAATXBP" int(11) NOT NULL default '0',
  "AAABEFTK" varchar(132) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAATXBF" varchar(1) NOT NULL default ' ',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABFQOM" varchar(1) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAABLRPQ" varchar(132) NOT NULL default ' ',
  "AAAATXBL" tinyint(3) unsigned NOT NULL default '0',
  "AAAATXBM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAAATXFU","AAABMREF","AAAATXBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXBQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  "AAAATXBT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATXFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXCY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMDEP","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXCG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXCY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMDEP","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXCO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXCY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXCY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAAATXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXDQ" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAAATXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXEC" (
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAAATXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXEZ" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABEUHG","AAABLESS","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXFJ" (
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUHG","AAAATXFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAAATXFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUHG","AAAATXFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" int(11) NOT NULL default '0',
  "AAAATXGF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXGI" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAATXGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXGM" (
  "AAABMOHM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXGP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABMRCK" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXGV" (
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXGY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXHE" (
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAABDSRO" varchar(10) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABLITM" varchar(100) NOT NULL default ' ',
  "AAABLQHQ" varchar(64) NOT NULL default ' ',
  "AAAATXIP" varchar(1) NOT NULL default ' ',
  "AAABJRGL" varchar(10) NOT NULL default ' ',
  "AAABHATP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXHP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDSRO" varchar(10) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABLITM" varchar(100) NOT NULL default ' ',
  "AAABLQHQ" varchar(64) NOT NULL default ' ',
  "AAAATXIP" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDSRO","AAABKGBL","AAABMOHM","AAABLITM","AAABLQHQ","AAAATXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXIF" (
  "AAABJRGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABJRGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXII" (
  "AAAAUBGZ" varchar(12) NOT NULL default ' ',
  "AAABDSRO" varchar(10) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default '0',
  "AAABLITM" varchar(100) NOT NULL default ' ',
  "AAABLQHQ" varchar(64) NOT NULL default ' ',
  "AAAATXIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBGZ","AAABDSRO","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXIT" (
  "AAAATXLB" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  "AAABIJNF" varchar(30) NOT NULL default ' ',
  "AAAATXIX" varchar(1) NOT NULL default ' ',
  "AAAATXIY" varchar(1) NOT NULL default ' ',
  "AAAATXIZ" varchar(1) NOT NULL default ' ',
  "AAAATXJA" varchar(1) NOT NULL default ' ',
  "AAAATXJB" varchar(30) NOT NULL default ' ',
  "AAAATXJC" int(11) NOT NULL default '0',
  "AAAATXJD" int(11) NOT NULL default '0',
  "AAAATXJE" int(11) NOT NULL default '0',
  "AAAATXJZ" decimal(20,0) NOT NULL default '0',
  "AAAATXKA" decimal(20,0) NOT NULL default '0',
  "AAAATXKB" decimal(20,0) NOT NULL default '0',
  "AAAATXKC" int(11) NOT NULL default '0',
  "AAAATXKD" int(11) NOT NULL default '0',
  "AAAATXKE" int(11) NOT NULL default '0',
  "AAAATXKF" int(11) NOT NULL default '0',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAATXJP" varchar(12) NOT NULL default ' ',
  "AAAATXJQ" varchar(8) NOT NULL default '00000000',
  "AAAATXJR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATXLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXJT" (
  "AAAATXLB" varchar(30) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAAATXJZ" decimal(20,0) NOT NULL default '0',
  "AAAATXKA" decimal(20,0) NOT NULL default '0',
  "AAAATXKB" decimal(20,0) NOT NULL default '0',
  "AAAATXKC" int(11) NOT NULL default '0',
  "AAAATXKD" int(11) NOT NULL default '0',
  "AAAATXKE" int(11) NOT NULL default '0',
  "AAAATXKF" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAATXLB","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXKI" (
  "AAAATXLB" varchar(30) NOT NULL default ' ',
  "AAAATXLD" varchar(80) NOT NULL default ' ',
  "AAAATXKW" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATXLB","AAAATXLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXKS" (
  "AAAATXLB" varchar(30) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAAATXLD" varchar(80) NOT NULL default ' ',
  "AAAATXKW" varchar(20) NOT NULL default ' ',
  "AAABLLLP" varchar(60) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAATXLB","AAABLUXQ","AAAATXLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXLE" (
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXLL" (
  "AAABLNHV" varchar(32) NOT NULL default ' ',
  "AAAATXMP" varchar(12) NOT NULL default ' ',
  "AAAATXLO" varchar(8) NOT NULL default '00000000',
  "AAAATXLP" varchar(6) NOT NULL default '000000',
  "AAAAVNMW" VARBINARY(255),
  "AAAATXMR" varchar(1) NOT NULL default ' ',
  "AAAATXMS" varchar(30) NOT NULL default ' ',
  "AAAATXMT" varchar(12) NOT NULL default ' ',
  "AAAATXLU" varchar(30) NOT NULL default ' ',
  "AAAATXLV" varchar(1) NOT NULL default ' ',
  "AAAATXLW" varchar(1) NOT NULL default ' ',
  "AAAATXLX" varchar(1) NOT NULL default ' ',
  "AAAATXLY" varchar(1) NOT NULL default ' ',
  "AAAATXLZ" varchar(1) NOT NULL default ' ',
  "AAAATXMA" varchar(1) NOT NULL default ' ',
  "AAAATXMB" varchar(1) NOT NULL default ' ',
  "AAAATXMC" varchar(1) NOT NULL default ' ',
  "AAAATXMD" varchar(1) NOT NULL default ' ',
  "AAAATXME" varchar(1) NOT NULL default ' ',
  "AAAATXMF" varchar(1) NOT NULL default ' ',
  "AAAATXMG" varchar(1) NOT NULL default ' ',
  "AAABAFSF" varchar(1) NOT NULL default ' ',
  "AAAATXMI" varchar(1) NOT NULL default ' ',
  "AAAATXMJ" varchar(1) NOT NULL default ' ',
  "AAAATXMK" varchar(1) NOT NULL default ' ',
  "AAAATXML" varchar(1) NOT NULL default ' ',
  "AAAATXMM" varchar(1) NOT NULL default ' ',
  "AAAATXMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNHV","AAAATXMP"),
  KEY "AAAATXMQ" ("AAAATXMR","AAAATXMS","AAAATXMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMCZV" varchar(4) NOT NULL default ' ',
  "AAABJLEO" varchar(1) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABLXJY" varchar(8) NOT NULL default '00000000',
  "AAABLXJR" varchar(10) NOT NULL default ' ',
  "AAABLXJU" varchar(3) NOT NULL default ' ',
  "AAABLXJS" varchar(4) NOT NULL default ' ',
  "AAABLXJW" varchar(8) NOT NULL default '00000000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATXPU" varchar(35) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAATXPU","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATXPU" varchar(35) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATXPU","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAAATXPS" varchar(20) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAATXPU" varchar(35) NOT NULL default ' ',
  "AAABJLDA" varchar(1) NOT NULL default ' ',
  "AAABJLDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABKXWG" varchar(8) NOT NULL default '00000000',
  "AAABKXWZ" varchar(8) NOT NULL default '00000000',
  "AAABKXWE" varchar(8) NOT NULL default '00000000',
  "AAABKXWD" varchar(8) NOT NULL default '00000000',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABLXHV" varchar(2) NOT NULL default ' ',
  "AAABLWQI" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAAATXOX" varchar(5) NOT NULL default '00000',
  "AAABJLDA" varchar(1) NOT NULL default ' ',
  "AAABJLDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABKXWG" varchar(8) NOT NULL default '00000000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABJLDB" varchar(1) NOT NULL default ' ',
  "AAABJLDA" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAAATXPS" varchar(20) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAATXPU" varchar(35) NOT NULL default ' ',
  "AAABJLDA" varchar(1) NOT NULL default ' ',
  "AAABJLDB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAATXQF" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  "AAABKXWD" varchar(8) NOT NULL default '00000000',
  "AAABKXWF" varchar(8) NOT NULL default '00000000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXQR" (
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAAAVCGO" tinyint(3) unsigned NOT NULL default '0',
  "AAAATXSF" varchar(30) NOT NULL default ' ',
  "AAABEURQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXQX" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATXRF" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXRH" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(40) NOT NULL default ' ',
  "AAABMMJT" varchar(3) NOT NULL default ' ',
  "AAABHGTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ","AAABMMJT"),
  KEY "AAAATXRP" ("AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXRR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXRX" (
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAAATXSF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCCC","AAAATXSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXSC" (
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAAAVCGO" tinyint(3) unsigned NOT NULL default '0',
  "AAAATXSF" varchar(30) NOT NULL default ' ',
  "AAABEURQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXSI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXSO" (
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXSR" (
  "AAAATXUW" varchar(1) NOT NULL default ' ',
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAAATXSU" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXUW","AAAATXVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATXWL" varchar(22) NOT NULL default ' ',
  "AAAATXWM" varchar(4) NOT NULL default '0000',
  "AAAATXTB" longblob,
  "AAAATXTC" varchar(4) NOT NULL default '0000',
  "AAAATXTD" varchar(3) NOT NULL default ' ',
  "AAAATXTE" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAAATXWL","AAAATXWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATXWL" varchar(22) NOT NULL default ' ',
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAAATXTN" varchar(8) NOT NULL default ' ',
  "AAAATXTO" varchar(1) NOT NULL default ' ',
  "AAABCVOB" varchar(1) NOT NULL default ' ',
  "AAAATXTQ" varchar(1) NOT NULL default ' ',
  "AAAATXWF" decimal(15,0) NOT NULL default '0',
  "AAAATXVN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAATXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXTV" (
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAAATXVE" varchar(30) NOT NULL default ' ',
  "AAAATXVF" varchar(30) NOT NULL default ' ',
  "AAAATXVG" varchar(20) NOT NULL default ' ',
  "AAABCVOC" varchar(1) NOT NULL default ' ',
  "AAABCVOD" varchar(1) NOT NULL default ' ',
  "AAAATXVJ" varchar(1) NOT NULL default ' ',
  "AAABCVOF" varchar(1) NOT NULL default ' ',
  "AAAATXVN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAATXVL","AAABCVOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXUI" (
  "AAAATXUW" varchar(1) NOT NULL default ' ',
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAAATXUV" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXUW","AAAATXVL","AAABCVOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXUQ" (
  "AAAATXUW" varchar(1) NOT NULL default ' ',
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAAATXVN" varchar(2) NOT NULL default '00',
  "AAAATXUV" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXUW","AAAATXVL","AAABCVOH","AAAATXVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXVA" (
  "AAAATXVL" varchar(8) NOT NULL default ' ',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAAATXVN" varchar(2) NOT NULL default '00',
  "AAAATXVE" varchar(30) NOT NULL default ' ',
  "AAAATXVF" varchar(30) NOT NULL default ' ',
  "AAAATXVG" varchar(20) NOT NULL default ' ',
  "AAABCVOC" varchar(1) NOT NULL default ' ',
  "AAABCVOD" varchar(1) NOT NULL default ' ',
  "AAAATXVJ" varchar(1) NOT NULL default ' ',
  "AAABCVOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATXVL","AAABCVOH","AAAATXVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATXWL" varchar(22) NOT NULL default ' ',
  "AAAATXWM" varchar(4) NOT NULL default '0000',
  "AAAATXWN" varchar(4) NOT NULL default '0000',
  "AAAATXVT" varchar(8) NOT NULL default ' ',
  "AAAATXVU" varchar(40) NOT NULL default ' ',
  "AAAATXVV" varchar(5) NOT NULL default '00000',
  "AAAATXVW" varchar(8) NOT NULL default ' ',
  "AAAATXVX" varchar(40) NOT NULL default ' ',
  "AAAATXVY" varchar(12) NOT NULL default ' ',
  "AAAATXVZ" varchar(30) NOT NULL default ' ',
  "AAAATXWA" varchar(30) NOT NULL default ' ',
  "AAAATXWB" varchar(1) NOT NULL default ' ',
  "AAAATXWC" varchar(1) NOT NULL default ' ',
  "AAAATXWD" varchar(1) NOT NULL default ' ',
  "AAAATXWE" varchar(68) NOT NULL default ' ',
  "AAAATXWF" decimal(15,0) NOT NULL default '0',
  "AAAATXWG" varchar(6) NOT NULL default ' ',
  "AAAATXWH" longtext,
  "AAAATXWI" varchar(20) NOT NULL default ' ',
  "AAAATXWJ" longblob,
  PRIMARY KEY ("AAABMRIL","AAAATXWL","AAAATXWM","AAAATXWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABJDQY" varchar(46) NOT NULL default ' ',
  "AAABJDQZ" varchar(46) NOT NULL default ' ',
  "AAABJDQW" varchar(1) NOT NULL default ' ',
  "AAAATXWX" int(11) NOT NULL default '0',
  "AAAATXWY" int(11) NOT NULL default '0',
  "AAAATXWZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBJW","AAABLOZE","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHBEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBJW","AAABLOZE","AAABIHQW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
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
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  "AAABHBJW" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAABIHQW" varchar(12) NOT NULL default ' ',
  "AAAATXYJ" varchar(255) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAAATXYL" varchar(1) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXYQ" (
  "AAABIJSY" varchar(8) NOT NULL default '00000000',
  "AAABIJSZ" varchar(6) NOT NULL default '000000',
  "AAAATXZL" varchar(10) NOT NULL default '0000000000',
  "AAAATXZC" varchar(30) NOT NULL default ' ',
  "AAAATXZD" varchar(30) NOT NULL default ' ',
  "AAAATXZE" varchar(30) NOT NULL default ' ',
  "AAAATXYX" varchar(132) NOT NULL default ' ',
  "AAAATXYY" longblob,
  PRIMARY KEY ("AAABIJSY","AAABIJSZ","AAAATXZL","AAAATXZC","AAAATXZD","AAAATXZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXZF" (
  "AAABIJSY" varchar(8) NOT NULL default '00000000',
  "AAABIJSZ" varchar(6) NOT NULL default '000000',
  "AAAATXZL" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABIJSY","AAABIJSZ","AAAATXZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXZM" (
  "AAABKIUC" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(40) NOT NULL default ' ',
  "AAAATXZT" varchar(4) NOT NULL default ' ',
  "AAABLSBY" longblob,
  PRIMARY KEY ("AAABKIUC","AAABLUSV","AAAATXZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATXZU" (
  "AAABKIUC" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAAATXZY" varchar(10) NOT NULL default ' ',
  "AAABJSPH" varchar(4) NOT NULL default ' ',
  "AAABKJAU" varchar(1) NOT NULL default ' ',
  "AAAATYAB" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(200) NOT NULL default ' ',
  "AAABLUVS" varchar(100) NOT NULL default ' ',
  "AAABKWHD" varchar(70) NOT NULL default ' ',
  "AAAATYAF" varchar(70) NOT NULL default ' ',
  "AAAATYAG" varchar(70) NOT NULL default ' ',
  "AAAATYAH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIUC","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYAK" (
  "AAAATYAW" varchar(10) NOT NULL default ' ',
  "AAAATYAX" varchar(10) NOT NULL default ' ',
  "AAAATYAY" varchar(10) NOT NULL default ' ',
  "AAAATYAZ" varchar(30) NOT NULL default ' ',
  "AAAATYAP" varchar(1) NOT NULL default ' ',
  "AAAATYAQ" varchar(1) NOT NULL default ' ',
  "AAAATYAR" varchar(1) NOT NULL default ' ',
  "AAAATYAS" varchar(1) NOT NULL default ' ',
  "AAAATYAT" varchar(1) NOT NULL default ' ',
  "AAAATYAU" varchar(80) NOT NULL default ' ',
  "AAAATYAV" longblob,
  PRIMARY KEY ("AAAATYAW","AAAATYAX","AAAATYAY","AAAATYAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCST" varchar(1) NOT NULL default ' ',
  "AAAAVZDA" varchar(10) NOT NULL default ' ',
  "AAAAVZCH" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCSU" varchar(2) NOT NULL default ' ',
  "AAAAVZCL" varchar(4) NOT NULL default ' ',
  "AAABFXJE" varchar(10) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAVZCP" varchar(1) NOT NULL default ' ',
  "AAAAVZCQ" varchar(1) NOT NULL default ' ',
  "AAAAVZET" varchar(1) NOT NULL default ' ',
  "AAAAVZCS" varchar(1) NOT NULL default ' ',
  "AAAAWAJS" varchar(25) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMCSO"),
  KEY "AAAATYBW" ("AAABMRIL","AAAAVZDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAWZMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZBM","AAABMCSO","AAAAWZMU"),
  UNIQUE KEY "AAAATYCK" ("AAABMRIL","AAABMRHS","AAABLZBM","AAAAWZMU","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  "AAAAWAJS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZBM","AAABMCSO"),
  KEY "AAAATYDC" ("AAABMRIL","AAABMRHS","AAAAWAJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAAVZEC" varchar(3) NOT NULL default '000',
  "AAAAVZED" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAVZEG" varchar(4) NOT NULL default ' ',
  "AAAAVZEH" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAAAVZEN" varchar(1) NOT NULL default ' ',
  "AAAAVZEO" varchar(1) NOT NULL default ' ',
  "AAABMCRY" varchar(1) NOT NULL default ' ',
  "AAAAVZEQ" varchar(1) NOT NULL default ' ',
  "AAAAVZER" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAAAVZET" varchar(1) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAAAVZEY" varchar(1) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAAAVZFB" varchar(1) NOT NULL default ' ',
  "AAAAVZFC" varchar(1) NOT NULL default ' ',
  "AAAATYGM" varchar(10) NOT NULL default ' ',
  "AAAATYGN" varchar(8) NOT NULL default ' ',
  "AAABLCGB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO"),
  UNIQUE KEY "AAAATYEY" ("AAABMRIL","AAABMCSO","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFXJE" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAAVZEC" varchar(3) NOT NULL default '000',
  "AAAAVZED" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAVZEG" varchar(4) NOT NULL default ' ',
  "AAAAVZEH" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAAAVZEN" varchar(1) NOT NULL default ' ',
  "AAAAVZEO" varchar(1) NOT NULL default ' ',
  "AAABMCRY" varchar(1) NOT NULL default ' ',
  "AAAAVZER" varchar(1) NOT NULL default ' ',
  "AAABMCRZ" varchar(1) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABMCRC" varchar(10) NOT NULL default ' ',
  "AAAAVZEY" varchar(1) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAAAVZFB" varchar(1) NOT NULL default ' ',
  "AAAAVZFC" varchar(1) NOT NULL default ' ',
  "AAAATYGM" varchar(10) NOT NULL default ' ',
  "AAAATYGN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABFXJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFXJE" varchar(10) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZBM","AAABFXJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZOH" varchar(10) NOT NULL default ' ',
  "AAAAUZOI" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAATYHH" varchar(8) NOT NULL default '00000000',
  "AAAATYHI" varchar(2) NOT NULL default ' ',
  "AAAATYHJ" varchar(1) NOT NULL default ' ',
  "AAAAVZEQ" varchar(1) NOT NULL default ' ',
  "AAAATYHL" varchar(1) NOT NULL default ' ',
  "AAAATYHM" varchar(1) NOT NULL default ' ',
  "AAAATYHN" varchar(1) NOT NULL default ' ',
  "AAAATYHO" varchar(1) NOT NULL default ' ',
  "AAAATYHP" varchar(1) NOT NULL default ' ',
  "AAAATYHQ" decimal(11,2) NOT NULL default '0.00',
  "AAAATYHR" decimal(11,2) NOT NULL default '0.00',
  "AAABFZVY" decimal(13,2) NOT NULL default '0.00',
  "AAAATYHT" varchar(2) NOT NULL default '00',
  "AAAATYHU" varchar(2) NOT NULL default '00',
  "AAAATYHV" varchar(5) NOT NULL default '00000',
  "AAAATYHW" decimal(4,2) NOT NULL default '0.00',
  "AAAAUZOE" decimal(4,2) NOT NULL default '0.00',
  "AAAATYHY" decimal(4,2) NOT NULL default '0.00',
  "AAAATYHZ" varchar(8) NOT NULL default '00000000',
  "AAAATYIA" varchar(12) NOT NULL default ' ',
  "AAAATYIB" varchar(8) NOT NULL default '00000000',
  "AAAATYIC" varchar(12) NOT NULL default ' ',
  "AAAATYID" varchar(8) NOT NULL default '00000000',
  "AAAATYIE" varchar(12) NOT NULL default ' ',
  "AAAATYIF" varchar(8) NOT NULL default '00000000',
  "AAAATYIG" varchar(12) NOT NULL default ' ',
  "AAAATYIH" varchar(8) NOT NULL default '00000000',
  "AAAATYII" varchar(12) NOT NULL default ' ',
  "AAAATYIJ" varchar(8) NOT NULL default '00000000',
  "AAAATYIK" varchar(12) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  "AAAAXOPH" varchar(3) NOT NULL default '000',
  "AAAATYIN" varchar(8) NOT NULL default '00000000',
  "AAAATYIO" varchar(12) NOT NULL default ' ',
  "AAAATYIP" varchar(1) NOT NULL default ' ',
  "AAAATYIQ" varchar(1) NOT NULL default ' ',
  "AAAATYIR" varchar(1) NOT NULL default ' ',
  "AAAATYIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUZOH","AAAAUZOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYIW" (
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(70) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTOL" varchar(100) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAATYJC" varchar(60) NOT NULL default ' ',
  "AAAATYJD" varchar(60) NOT NULL default ' ',
  "AAAATYJE" varchar(60) NOT NULL default ' ',
  "AAAATYJF" varchar(60) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLPJD","AAABLGCM","AAABMREL","AAABLTOL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATZEO" varchar(32) NOT NULL default ' ',
  "AAABLEKS" varchar(32) NOT NULL default ' ',
  "AAABIYCS" varchar(32) NOT NULL default ' ',
  "AAAATYKB" varchar(32) NOT NULL default ' ',
  "AAABIFGN" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATZEO","AAABLEKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYJX" (
  "AAAATYKB" varchar(32) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  "AAAATYKA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATYKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYKC" (
  "AAABHCQH" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYKG" (
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABMRGK" varchar(30) NOT NULL default ' ',
  "AAABMNMP" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLJJL" varchar(40) NOT NULL default ' ',
  "AAABLVCU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYKO" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABFQPC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYKU" (
  "AAAATYKW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAATYKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYKX" (
  "AAABLECD" varchar(15) NOT NULL default ' ',
  "AAAATYPE" varchar(1) NOT NULL default ' ',
  "AAAATYLA" varchar(30) NOT NULL default ' ',
  "AAAATYLB" varchar(40) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYLH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(15) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYLN" (
  "AAAATYLX" varchar(6) NOT NULL default ' ',
  "AAAATYLP" varchar(1) NOT NULL default ' ',
  "AAAATYLQ" int(11) NOT NULL default '0',
  "AAAATYLR" int(11) NOT NULL default '0',
  "AAAAVQPQ" int(11) NOT NULL default '0',
  "AAAATYLT" int(11) NOT NULL default '0',
  "AAAATYLU" varchar(1) NOT NULL default ' ',
  "AAABIGLD" varchar(12) NOT NULL default ' ',
  "AAABHCQH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAATYLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATYMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATYPN" varchar(32) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATYPN","AAABLPJD","AAABLOZE","AAABLUNJ","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCRNY" varchar(32) NOT NULL default ' ',
  "AAAATYNL" varchar(5) NOT NULL default '00000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAWVSB" varchar(2) NOT NULL default ' ',
  "AAAATYWW" varchar(64) NOT NULL default ' ',
  "AAAATYWX" varchar(64) NOT NULL default ' ',
  "AAABMNIO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCRNY","AAAATYNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYNM" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABLENN" varchar(16) NOT NULL default ' ',
  "AAABLENO" varchar(32) NOT NULL default ' ',
  "AAABKNBI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR","AAABMKFU","AAAAZOEQ","AAABCZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYNY" (
  "AAAATYPN" varchar(32) NOT NULL default ' ',
  "AAAATYOA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAATYPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATYPN" varchar(32) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAATYPN","AAABLPJD","AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAAATYOW" varchar(1) NOT NULL default ' ',
  "AAAATYOX" varchar(1) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAAATYPA" varchar(32) NOT NULL default ' ',
  "AAABLECD" varchar(15) NOT NULL default ' ',
  "AAABFZAB" varchar(32) NOT NULL default ' ',
  "AAAATYWB" varchar(30) NOT NULL default ' ',
  "AAAATYPE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATYPN" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATYPN","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATYPZ" varchar(32) NOT NULL default ' ',
  "AAAATYQA" varchar(5) NOT NULL default '00000',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(64) NOT NULL default ' ',
  "AAABMNNA" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATYPZ","AAAATYQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(15) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLPJD","AAABLOZE","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYQU" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAATYQY" varchar(32) NOT NULL default ' ',
  "AAAATYQZ" int(11) NOT NULL default '0',
  "AAAATYRA" int(11) NOT NULL default '0',
  "AAAATYRB" varchar(14) NOT NULL default '00000000000000',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYRI" (
  "AAAATYSO" varchar(32) NOT NULL default ' ',
  "AAABFSPQ" varchar(32) NOT NULL default ' ',
  "AAABJHCT" varchar(20) NOT NULL default ' ',
  "AAABLPJD" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAAATYRR" varchar(64) NOT NULL default ' ',
  "AAAATYRS" varchar(1) NOT NULL default ' ',
  "AAAATYRT" varchar(10) NOT NULL default ' ',
  "AAAATYRU" varchar(32) NOT NULL default ' ',
  "AAAATYRV" varchar(64) NOT NULL default ' ',
  "AAAATYRW" varchar(64) NOT NULL default ' ',
  "AAAATYRX" varchar(64) NOT NULL default ' ',
  "AAAATYRY" varchar(1) NOT NULL default ' ',
  "AAAATYRZ" varchar(1) NOT NULL default ' ',
  "AAAATYSA" varchar(1) NOT NULL default ' ',
  "AAABFNHE" int(11) NOT NULL default '0',
  "AAAATYSC" varchar(32) NOT NULL default ' ',
  "AAAATYSD" varchar(1) NOT NULL default ' ',
  "AAAATYSE" varchar(10) NOT NULL default ' ',
  "AAAATYSF" varchar(32) NOT NULL default ' ',
  "AAAATYSG" varchar(1) NOT NULL default ' ',
  "AAAATYSH" varchar(1) NOT NULL default ' ',
  "AAAATYSI" varchar(1) NOT NULL default ' ',
  "AAAATYSJ" varchar(1) NOT NULL default ' ',
  "AAAATYSK" varchar(1) NOT NULL default ' ',
  "AAAATYSL" varchar(1) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAAATYSN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAATYSO","AAABFSPQ","AAABJHCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATYTA" varchar(55) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAATYTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAATYTL" varchar(55) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAATYTL","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYTN" (
  "AAABGFOS" varchar(30) NOT NULL default ' ',
  "AAAATYTP" varchar(30) NOT NULL default ' ',
  "AAABLEIH" varchar(4) NOT NULL default ' ',
  "AAAATYTR" varchar(30) NOT NULL default ' ',
  "AAAATYTS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYTU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUYGO" VARBINARY(255) NOT NULL,
  "AAABEDQH" varchar(10) NOT NULL default ' ',
  "AAAAUYGY" varchar(70) NOT NULL default ' ',
  "AAAAUYHA" varchar(32) NOT NULL default ' ',
  "AAAAUYHC" varchar(2) NOT NULL default ' ',
  "AAAAUYGZ" varchar(70) NOT NULL default ' ',
  "AAAAUYHB" varchar(32) NOT NULL default ' ',
  "AAAAUYHD" varchar(2) NOT NULL default ' ',
  "AAAAUYGH" varchar(10) NOT NULL default ' ',
  "AAAAUYGI" varchar(1) NOT NULL default ' ',
  "AAAAUYGJ" varchar(10) NOT NULL default ' ',
  "AAAAUYGK" varchar(1) NOT NULL default ' ',
  "AAABDODY" decimal(15,0) NOT NULL default '0',
  "AAAAUYGM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUYGO"(255)),
  KEY "AAAATYUM" ("AAAAUYGY","AAAAUYHA","AAAAUYHC"),
  KEY "AAAATYUQ" ("AAAAUYGZ","AAAAUYHB","AAAAUYHD"),
  KEY "AAAATYUU" ("AAAAUYGY","AAAAUYGZ","AAAAUYHA","AAAAUYHB","AAAAUYHC","AAAAUYHD","AAABEDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYVC" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAAATYVH" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYVN" (
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAAATYVP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYVR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYVX" (
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  "AAABCTJC" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(15) NOT NULL default ' ',
  "AAAATYWB" varchar(30) NOT NULL default ' ',
  "AAAATYXA" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYWI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAAATYWM" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYWP" (
  "AAABMLPV" varchar(15) NOT NULL default ' ',
  "AAAATYXA" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLEEW" varchar(25) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAWVSB" varchar(2) NOT NULL default ' ',
  "AAAATYWW" varchar(64) NOT NULL default ' ',
  "AAAATYWX" varchar(64) NOT NULL default ' ',
  "AAABMNIO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMLPV","AAAATYXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYXB" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(61) NOT NULL default ' ',
  "AAABCTJC" varchar(10) NOT NULL default ' ',
  "AAABAGTJ" varchar(32) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYXM" (
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHMQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYXP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAAAVJTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHMQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYXV" (
  "AAABIARM" varchar(32) NOT NULL default ' ',
  "AAABHKDJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIARM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYXZ" (
  "AAABIARM" varchar(32) NOT NULL default ' ',
  "AAAATYYB" varchar(32) NOT NULL default ' ',
  "AAAATYYC" varchar(32) NOT NULL default ' ',
  "AAAATYYD" varchar(32) NOT NULL default ' ',
  "AAAATYYE" varchar(32) NOT NULL default ' ',
  "AAAATYYF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIARM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYYH" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(15) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAABLVCU" varchar(15) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABHGTL" varchar(2) NOT NULL default '00',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAATZEA","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYYU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(15) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATZEA","AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYZC" (
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYZF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYZL" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(30) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  "AAABGCZV" varchar(14) NOT NULL default '00000000000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATYZW" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAATZEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZAD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATZEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZAJ" (
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZAM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(40) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZAS" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(15) NOT NULL default ' ',
  "AAABLUEO" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAAATZEA","AAABLVCU","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZAZ" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABLRTQ" varchar(2) NOT NULL default '00',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLUEO" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAAATZEA","AAABIFOV","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZBL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  "AAABLMOZ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATZEA","AAABIFOV","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZBW" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAAATZCH" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAATZEA","AAABIFOV","AAAATZCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZCI" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default '00',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAATZEA","AAABMKFH","AAABMOHM","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZCS" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(15) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAATZEA","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZDB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(15) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATZEA","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZDJ" (
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAAATZDM" varchar(15) NOT NULL default ' ',
  "AAAATZDN" varchar(15) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAAATZEA","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZDU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEA" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAATZEA","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATZEO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATZEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAATZEO" varchar(32) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAATZEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFGN" varchar(32) NOT NULL default ' ',
  "AAAAUYTB" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAAAVTMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTND" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTON" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCXN","AAABFOPV","AAABHNOA","AAABFSUK","AAABFUWK","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAAAVTMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTND" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTNZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTON" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTOZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTPZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTQT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCXN","AAABFOPV","AAABHNOA","AAABFSUK","AAABFUWK","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAAAVTRE" varchar(10) NOT NULL default ' ',
  "AAAAVTRF" varchar(4) NOT NULL default ' ',
  "AAAAVTRG" varchar(4) NOT NULL default ' ',
  "AAAAVTRH" varchar(4) NOT NULL default ' ',
  "AAAAVTRI" varchar(4) NOT NULL default ' ',
  "AAAAVTRJ" varchar(18) NOT NULL default ' ',
  "AAAAVTRK" varchar(4) NOT NULL default ' ',
  "AAAAVTRL" varchar(4) NOT NULL default ' ',
  "AAAAVTRM" varchar(4) NOT NULL default ' ',
  "AAAAVTRN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABIBGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABIBGO" decimal(15,2) NOT NULL default '0.00',
  "AAABIBFS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXN" decimal(15,3) NOT NULL default '0.000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAVTXW" varchar(22) NOT NULL default ' ',
  "AAABIBGG" varchar(22) NOT NULL default ' ',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABKOHU" varchar(5) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAAAVTYF" varchar(3) NOT NULL default ' ',
  "AAAAVTYG" varchar(1) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABJFXG" varchar(3) NOT NULL default '000',
  "AAAAVTYJ" varchar(3) NOT NULL default '000',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAAAVTYL" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIBFX" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABJRZB" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAZNML" varchar(2) NOT NULL default '00',
  "AAABIBGU" varchar(22) NOT NULL default ' ',
  "AAAAVTYW" varchar(22) NOT NULL default ' ',
  "AAAAVTYX" varchar(22) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCZQ" varchar(4) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABIBGS" varchar(10) NOT NULL default ' ',
  "AAABIBGL" varchar(10) NOT NULL default ' ',
  "AAABIBGJ" varchar(4) NOT NULL default ' ',
  "AAABIBGK" varchar(4) NOT NULL default ' ',
  "AAAAVTZI" varchar(2) NOT NULL default ' ',
  "AAABIBGT" varchar(10) NOT NULL default ' ',
  "AAABMCXF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABIBGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABIBGO" decimal(15,2) NOT NULL default '0.00',
  "AAABIBFS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVTXK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVTXN" decimal(15,3) NOT NULL default '0.000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAVTXW" varchar(22) NOT NULL default ' ',
  "AAABIBGG" varchar(22) NOT NULL default ' ',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABKOHU" varchar(5) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAAAVTYF" varchar(3) NOT NULL default ' ',
  "AAAAVTYG" varchar(1) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABJFXG" varchar(3) NOT NULL default '000',
  "AAAAVTYJ" varchar(3) NOT NULL default '000',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAAAVTYL" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABIBFX" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABJRZB" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAZNML" varchar(2) NOT NULL default '00',
  "AAABIBGU" varchar(22) NOT NULL default ' ',
  "AAAAVTYW" varchar(22) NOT NULL default ' ',
  "AAAAVTYX" varchar(22) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCZQ" varchar(4) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABIBGS" varchar(10) NOT NULL default ' ',
  "AAABIBGL" varchar(10) NOT NULL default ' ',
  "AAABIBGJ" varchar(4) NOT NULL default ' ',
  "AAABIBGK" varchar(4) NOT NULL default ' ',
  "AAAAVTZI" varchar(2) NOT NULL default ' ',
  "AAABIBGT" varchar(10) NOT NULL default ' ',
  "AAABMCXF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBA","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAATZTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAAAVUAF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDH" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDI" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDJ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDN" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDO" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDP" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDR" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDS" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDT" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDU" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDV" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDW" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDX" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDY" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDZ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEA" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEB" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEC" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUED" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEE" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEF" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEG" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEH" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEI" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEJ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEN" varchar(1) NOT NULL default ' ',
  "AAAAVUEO" varchar(1) NOT NULL default ' ',
  "AAAAVUEP" varchar(1) NOT NULL default ' ',
  "AAAAVUEQ" varchar(1) NOT NULL default ' ',
  "AAAAVUER" varchar(1) NOT NULL default ' ',
  "AAAAVUES" varchar(1) NOT NULL default ' ',
  "AAAAVUET" varchar(1) NOT NULL default ' ',
  "AAAAVUEU" varchar(1) NOT NULL default ' ',
  "AAAAVUEV" varchar(1) NOT NULL default ' ',
  "AAAAVUEW" varchar(1) NOT NULL default ' ',
  "AAAAVUEX" varchar(1) NOT NULL default ' ',
  "AAAAVUEY" varchar(1) NOT NULL default ' ',
  "AAAAVUEZ" varchar(1) NOT NULL default ' ',
  "AAAAVUFA" varchar(1) NOT NULL default ' ',
  "AAAAVUFB" varchar(1) NOT NULL default ' ',
  "AAAAVUFC" varchar(1) NOT NULL default ' ',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXF","AAABKRXK","AAABKOTY","AAABMCXV","AAABJGDL","AAABMCRA","AAABMCPW","AAABIBFR","AAABKRXN","AAABKRXO"),
  KEY "AAAATZZT" ("AAABJGDL","AAABMDBB","AAABKRXF","AAABKRXK","AAABKRXE","AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAAAVUAF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUAZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUBZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCH" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCI" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCK" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCL" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCM" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCN" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCP" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCR" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCS" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCT" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCU" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCV" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCW" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCX" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCY" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUCZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDA" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDB" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDC" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDD" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDE" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDF" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDG" decimal(15,2) NOT NULL default '0.00',
  "AAAAVUDH" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDI" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDJ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDN" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDO" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDP" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDR" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDS" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDT" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDU" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDV" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDW" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDX" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDY" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUDZ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEA" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEB" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEC" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUED" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEE" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEF" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEG" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEH" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEI" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEJ" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEK" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEL" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEM" decimal(15,3) NOT NULL default '0.000',
  "AAAAVUEN" varchar(1) NOT NULL default ' ',
  "AAAAVUEO" varchar(1) NOT NULL default ' ',
  "AAAAVUEP" varchar(1) NOT NULL default ' ',
  "AAAAVUEQ" varchar(1) NOT NULL default ' ',
  "AAAAVUER" varchar(1) NOT NULL default ' ',
  "AAAAVUES" varchar(1) NOT NULL default ' ',
  "AAAAVUET" varchar(1) NOT NULL default ' ',
  "AAAAVUEU" varchar(1) NOT NULL default ' ',
  "AAAAVUEV" varchar(1) NOT NULL default ' ',
  "AAAAVUEW" varchar(1) NOT NULL default ' ',
  "AAAAVUEX" varchar(1) NOT NULL default ' ',
  "AAAAVUEY" varchar(1) NOT NULL default ' ',
  "AAAAVUEZ" varchar(1) NOT NULL default ' ',
  "AAAAVUFA" varchar(1) NOT NULL default ' ',
  "AAAAVUFB" varchar(1) NOT NULL default ' ',
  "AAAAVUFC" varchar(1) NOT NULL default ' ',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXF","AAABKRXK","AAABKOTY","AAABMCXV","AAABJGDL","AAABMCRA","AAABMCPW","AAABIBFR","AAABKRXN","AAABKRXO"),
  KEY "AAAAUAGF" ("AAABJGDL","AAABMDBB","AAABKRXF","AAABKRXK","AAABKRXE","AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXIY","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAGW" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAAAUAHA" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABHYMW" varchar(100) NOT NULL default ' ',
  "AAABHYMX" varchar(32) NOT NULL default ' ',
  "AAAAUAHE" varchar(1) NOT NULL default ' ',
  "AAAAUAHF" varchar(1) NOT NULL default ' ',
  "AAAAUAHG" int(11) NOT NULL default '0',
  "AAAAUAHH" varchar(100) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAAAUAHJ" varchar(4) NOT NULL default '0000',
  "AAAAUAHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAHM" (
  "AAABLUSV" varchar(40) NOT NULL default ' ',
  "AAABLKWV" int(11) NOT NULL default '0',
  "AAABLSBY" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLKWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAAAUAHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWBG","AAABGVLV","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAIC" (
  "AAABKNGX" varchar(3) NOT NULL default '000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAAAUAIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNGX","AAABGWBG","AAABGVLV","AAABJGWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAIM" (
  "AAABJVTD" int(11) NOT NULL default '0',
  "AAABIWIW" int(11) NOT NULL default '0',
  "AAABIWPY" int(11) NOT NULL default '0',
  "AAABIWQH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABJVTD"),
  KEY "AAAAUAIS" ("AAABIWIW","AAABIWPY","AAABIWQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAAAUAJD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUAJE" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAAAUAJD","AAAAUAJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAAAUAMG" tinyint(3) unsigned NOT NULL default '0',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAAAUAMD" int(11) NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGTBZ" varchar(3) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABGWDW" varchar(10) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  "AAABGSLP" varchar(1) NOT NULL default ' ',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  "AAABKZVA" varchar(4) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGRHK" varchar(4) NOT NULL default ' ',
  "AAABGRHL" varchar(12) NOT NULL default ' ',
  "AAABGXTR" varchar(1) NOT NULL default ' ',
  "AAABGXTQ" varchar(4) NOT NULL default '0000',
  "AAABIDTM" varchar(1) NOT NULL default '0',
  "AAABGVPH" double NOT NULL default '0',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABLADA" varchar(4) NOT NULL default ' ',
  "AAABLADS" varchar(10) NOT NULL default ' ',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABLADQ" varchar(1) NOT NULL default ' ',
  "AAABLADR" varchar(1) NOT NULL default ' ',
  "AAABKWDD" varchar(1) NOT NULL default ' ',
  "AAABKWDE" varchar(1) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABKTRY" varchar(20) NOT NULL default ' ',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABGVPT" varchar(1) NOT NULL default '0',
  "AAABGZIW" varchar(8) NOT NULL default '00000000',
  "AAABGZIX" varchar(1) NOT NULL default ' ',
  "AAABGVPV" varchar(8) NOT NULL default '00000000',
  "AAABGVPW" varchar(8) NOT NULL default '00000000',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  "AAABKWDH" varchar(50) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAABLABR" varchar(16) NOT NULL default ' ',
  "AAABKXKU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGZKH","AAAAUAMG","AAABKSXJ","AAAAUAMD"),
  KEY "AAAAUAME" ("AAABMRIL","AAAAUAMG","AAABKTRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABKNGX" tinyint(3) unsigned NOT NULL default '0',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABGWBG" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default '000',
  "AAABGRNU" varchar(1) NOT NULL default ' ',
  "AAABGRMQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGXSL" VARBINARY(255),
  "AAABGXMD" varchar(2) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABGWBR" varchar(8) NOT NULL default '00000000',
  "AAABGWBS" varchar(2) NOT NULL default '00',
  "AAABGWBT" varchar(8) NOT NULL default '00000000',
  "AAABGWBU" varchar(2) NOT NULL default '00',
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAAAUANK" ("AAABMRIL","AAABJGWB","AAABLAXK"(255)),
  KEY "AAAAUANO" ("AAABMRIL","AAABJGWD","AAABJGWB","AAABGWBG","AAABGRMQ","AAABGVFO"),
  KEY "AAAAUANV" ("AAABMRIL","AAABJGWB","AAABLAXL","AAABGXSL"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAOA" (
  "AAAAUARF" int(11) NOT NULL default '0',
  "AAAAUARH" int(11) NOT NULL default '0',
  "AAAAUARJ" int(11) NOT NULL default '0',
  "AAAAUARL" int(11) NOT NULL default '0',
  "AAAAUARN" int(11) NOT NULL default '0',
  "AAAAUARP" int(11) NOT NULL default '0',
  "AAAAUARR" int(11) NOT NULL default '0',
  "AAAAUART" int(11) NOT NULL default '0',
  "AAAAUAQG" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQH" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQI" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQK" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQL" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQM" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQN" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQO" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQP" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQQ" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQR" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQS" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQT" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQU" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQV" decimal(17,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAAAUARF","AAAAUARH","AAAAUARJ","AAAAUARL","AAAAUARN","AAAAUARP","AAAAUARR","AAAAUART"),
  KEY "AAAAUAPH" ("AAAAUARF"),
  KEY "AAAAUAPJ" ("AAAAUARH"),
  KEY "AAAAUAPL" ("AAAAUARJ"),
  KEY "AAAAUAPN" ("AAAAUARL"),
  KEY "AAAAUAPP" ("AAAAUARN"),
  KEY "AAAAUAPR" ("AAAAUARP"),
  KEY "AAAAUAPT" ("AAAAUARR"),
  KEY "AAAAUAPV" ("AAAAUART")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAPX" (
  "AAAAUARF" int(11) NOT NULL default '0',
  "AAAAUARH" int(11) NOT NULL default '0',
  "AAAAUARJ" int(11) NOT NULL default '0',
  "AAAAUARL" int(11) NOT NULL default '0',
  "AAAAUARN" int(11) NOT NULL default '0',
  "AAAAUARP" int(11) NOT NULL default '0',
  "AAAAUARR" int(11) NOT NULL default '0',
  "AAAAUART" int(11) NOT NULL default '0',
  "AAAAUAQG" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQH" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQI" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQK" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQL" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQM" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQN" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQO" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQP" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQQ" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQR" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQS" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQT" decimal(17,3) NOT NULL default '0.000',
  "AAAAUAQU" decimal(17,2) NOT NULL default '0.00',
  "AAAAUAQV" decimal(17,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAAAUARF","AAAAUARH","AAAAUARJ","AAAAUARL","AAAAUARN","AAAAUARP","AAAAUARR","AAAAUART"),
  KEY "AAAAUARE" ("AAAAUARF"),
  KEY "AAAAUARG" ("AAAAUARH"),
  KEY "AAAAUARI" ("AAAAUARJ"),
  KEY "AAAAUARK" ("AAAAUARL"),
  KEY "AAAAUARM" ("AAAAUARN"),
  KEY "AAAAUARO" ("AAAAUARP"),
  KEY "AAAAUARQ" ("AAAAUARR"),
  KEY "AAAAUARS" ("AAAAUART")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUARU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUZOH" varchar(10) NOT NULL default ' ',
  "AAAAUZOI" varchar(4) NOT NULL default '0000',
  "AAAAUATA" varchar(3) NOT NULL default '000',
  "AAAAUATB" varchar(6) NOT NULL default '000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABKRSB" varchar(10) NOT NULL default ' ',
  "AAABLSDV" varchar(1) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABKPOJ" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLCFJ" varchar(1) NOT NULL default ' ',
  "AAAAUASL" varchar(1) NOT NULL default ' ',
  "AAAAUASM" varchar(1) NOT NULL default ' ',
  "AAAAUASN" varchar(1) NOT NULL default ' ',
  "AAAAUASO" varchar(4) NOT NULL default '0000',
  "AAAAUASP" varchar(2) NOT NULL default '00',
  "AAAAUZNJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAUASR" varchar(2) NOT NULL default '00',
  "AAAAUZNV" decimal(13,2) NOT NULL default '0.00',
  "AAAAUAST" varchar(2) NOT NULL default '00',
  "AAAAUZOA" decimal(13,2) NOT NULL default '0.00',
  "AAAAUZOB" decimal(13,2) NOT NULL default '0.00',
  "AAABKSOZ" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAUZOH","AAAAUZOI","AAAAUATA","AAAAUATB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUATC" (
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
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBPPF" varchar(2) NOT NULL default '00',
  "AAABIBHM" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPPL" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABBPPR" varchar(10) NOT NULL default ' ',
  "AAABBPPS" varchar(2) NOT NULL default '00',
  "AAABIBHQ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABMQDX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQED" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQER" decimal(15,2) NOT NULL default '0.00',
  "AAABMQES" decimal(15,2) NOT NULL default '0.00',
  "AAABMQET" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS"),
  KEY "AAAAUAXM" ("AAABBTSC","AAABMQHZ","AAABMCRJ","AAABMQHY","AAABMQHX"),
  KEY "AAAAUAXS" ("AAABBPPF","AAABMCRJ","AAABMQHY","AAABMQBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAXX" (
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
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBPPF" varchar(2) NOT NULL default '00',
  "AAABIBHM" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPPL" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABBPPR" varchar(10) NOT NULL default ' ',
  "AAABBPPS" varchar(2) NOT NULL default '00',
  "AAABIBHQ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUAZR" (
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
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBPPF" varchar(2) NOT NULL default '00',
  "AAABIBHM" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPPL" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABBPPR" varchar(10) NOT NULL default ' ',
  "AAABBPPS" varchar(2) NOT NULL default '00',
  "AAABIBHQ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABMQDX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQED" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQER" decimal(15,2) NOT NULL default '0.00',
  "AAABMQES" decimal(15,2) NOT NULL default '0.00',
  "AAABMQET" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBEB" (
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
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBPPF" varchar(2) NOT NULL default '00',
  "AAABIBHM" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABBPPL" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABBPPR" varchar(10) NOT NULL default ' ',
  "AAABBPPS" varchar(2) NOT NULL default '00',
  "AAABIBHQ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABMQDY" decimal(15,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(15,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(15,2) NOT NULL default '0.00',
  "AAABMQFX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' '
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBFR" (
  "AAAAUBGN" varchar(8) NOT NULL default '00000000',
  "AAABECTI" varchar(2) NOT NULL default ' ',
  "AAABLKWV" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAUBGN","AAABECTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBFX" (
  "AAAAUBGN" varchar(8) NOT NULL default '00000000',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABLKWV" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAUBGN","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBGD" (
  "AAAAUBGN" varchar(8) NOT NULL default '00000000',
  "AAAAUBGI" varchar(40) NOT NULL default ' ',
  "AAABLKWV" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAUBGN","AAAAUBGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBGJ" (
  "AAAAUBGN" varchar(8) NOT NULL default '00000000',
  "AAAAVPOV" varchar(20) NOT NULL default ' ',
  "AAABLKWV" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAAAUBGN","AAAAVPOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBGP" (
  "AAAAUBGT" varchar(6) NOT NULL default ' ',
  "AAAAUBGR" varchar(16) NOT NULL default ' ',
  "AAAAXVMB" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBGU" (
  "AAAAUBGZ" varchar(12) NOT NULL default ' ',
  "AAABDSRO" varchar(10) NOT NULL default ' ',
  "AAABKGBL" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBGZ","AAABDSRO","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBHC" (
  "AAAAWAXE" varchar(40) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABGFTC" varchar(8) NOT NULL default '00000000',
  "AAABGFTD" varchar(6) NOT NULL default '000000',
  "AAAAUBJR" varchar(12) NOT NULL default ' ',
  "AAAAUBHI" varchar(8) NOT NULL default '00000000',
  "AAAAUBHJ" varchar(6) NOT NULL default '000000',
  "AAAAUBHK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAAAWAXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBHM" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABKCWA","AAABMNYT","AAABLUMP"),
  KEY "AAAAUBHX" ("AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBHZ" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLUSV" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMMJT" longblob,
  PRIMARY KEY ("AAABMNYT","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBIG" (
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAAAUBIJ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAAAUBJF" varchar(3) NOT NULL default ' ',
  "AAAAUBIM" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBIQ" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBIV" (
  "AAAAUBJN" varchar(4) NOT NULL default '0000',
  "AAAAUBIX" varchar(22) NOT NULL default ' ',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAAAUBIZ" varchar(1) NOT NULL default ' ',
  "AAAAUBJA" varchar(1) NOT NULL default ' ',
  "AAAAUBJB" varchar(1) NOT NULL default '0',
  "AAAAUBJC" varchar(10) NOT NULL default ' ',
  "AAAAUBJD" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(76) NOT NULL default ' ',
  "AAAAUBJF" varchar(3) NOT NULL default ' ',
  "AAABLRQK" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAUBJI" varchar(1) NOT NULL default ' ',
  "AAAAUBJJ" varchar(1) NOT NULL default ' ',
  "AAAAUBJK" varchar(1) NOT NULL default ' ',
  "AAAAUBJL" varchar(1) NOT NULL default ' ',
  "AAABIGGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUBJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBJO" (
  "AAABIZAS" varchar(5) NOT NULL default '00000',
  "AAABLFZK" varchar(4) NOT NULL default ' ',
  "AAAAUBJR" varchar(12) NOT NULL default ' ',
  "AAABGFTC" varchar(8) NOT NULL default '00000000',
  "AAABGFTD" varchar(6) NOT NULL default '000000',
  "AAAAUBJU" varchar(6) NOT NULL default '000000',
  "AAAAUBJV" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBJY" (
  "AAABIZAS" varchar(5) NOT NULL default '00000',
  "AAAAUBLA" varchar(4) NOT NULL default ' ',
  "AAAAUBKQ" varchar(4) NOT NULL default ' ',
  "AAAAUBLB" varchar(32) NOT NULL default ' ',
  "AAAAUBLC" varchar(1) NOT NULL default ' ',
  "AAAAUBLD" varchar(6) NOT NULL default ' ',
  "AAAAUBKU" varchar(4) NOT NULL default ' ',
  "AAAAUBKV" varchar(2) NOT NULL default ' ',
  "AAAAUBKW" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(4) NOT NULL default ' ',
  "AAAAUBKJ" int(11) NOT NULL default '0',
  "AAABGWIT" int(11) NOT NULL default '0',
  "AAAAUBKL" int(11) NOT NULL default '0',
  "AAAAUBKM" int(11) NOT NULL default '0',
  "AAAAUBKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIZAS","AAAAUBLA","AAAAUBKQ","AAAAUBLB","AAAAUBLC","AAAAUBLD","AAAAUBKU","AAAAUBKV","AAAAUBKW","AAABIMRZ"),
  KEY "AAAAUBKY" ("AAABIZAS","AAAAUBLA","AAAAUBLB","AAAAUBLC","AAAAUBLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAAAWALM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMRHS"),
  KEY "AAAAUBLN" ("AAABMRIL","AAABMRHS","AAAAWALM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSLC" varchar(40) NOT NULL default ' ',
  "AAAAWALM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMRHS"),
  KEY "AAAAUBMA" ("AAABMRIL","AAABMRHS","AAAAWALM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAWALU" varchar(15) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABIMOU" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLZRN" varchar(18) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABLOXN" varchar(22) NOT NULL default ' ',
  "AAAAWAME" varchar(3) NOT NULL default ' ',
  "AAAAWAMF" varchar(2) NOT NULL default ' ',
  "AAAAWAMG" varchar(4) NOT NULL default ' ',
  "AAAAWAMH" varchar(6) NOT NULL default ' ',
  "AAAAWAMI" varchar(3) NOT NULL default ' ',
  "AAAAWAMJ" varchar(3) NOT NULL default '000',
  "AAAAWAMK" varchar(18) NOT NULL default ' ',
  "AAABAGBI" varchar(4) NOT NULL default ' ',
  "AAABLIAR" varchar(32) NOT NULL default ' ',
  "AAABHXVX" varchar(14) NOT NULL default ' ',
  "AAAAWAMO" varchar(18) NOT NULL default ' ',
  "AAAAWUMF" varchar(3) NOT NULL default ' ',
  "AAAAWTLU" varchar(4) NOT NULL default ' ',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABLXPY" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAAAWKIP" varchar(2) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAAAWKIC" varchar(2) NOT NULL default ' ',
  "AAABDHAN" varchar(3) NOT NULL default ' ',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  "AAAAWANB" varchar(18) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXRT" varchar(13) NOT NULL default ' ',
  "AAAAWANF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZUKE" varchar(1) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAAAZULA" varchar(2) NOT NULL default ' ',
  "AAAAZULF" varchar(2) NOT NULL default ' ',
  "AAAAWANL" varchar(1) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABLFAI" decimal(13,3) NOT NULL default '0.000',
  "AAABLFAJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMY" decimal(13,3) NOT NULL default '0.000',
  "AAABJRZA" varchar(3) NOT NULL default ' ',
  "AAABHUEF" varchar(18) NOT NULL default ' ',
  "AAAAWANT" varchar(1) NOT NULL default ' ',
  "AAAAWANU" varchar(1) NOT NULL default ' ',
  "AAAAWANV" varchar(1) NOT NULL default ' ',
  "AAAAWANW" decimal(13,3) NOT NULL default '0.000',
  "AAABLVIO" varchar(3) NOT NULL default ' ',
  "AAAAWANY" decimal(13,3) NOT NULL default '0.000',
  "AAAAWANZ" varchar(3) NOT NULL default ' ',
  "AAABJRZC" decimal(3,1) NOT NULL default '0.0',
  "AAABJRZD" decimal(3,1) NOT NULL default '0.0',
  "AAAAWAOC" varchar(1) NOT NULL default ' ',
  "AAAAWAOD" varchar(1) NOT NULL default ' ',
  "AAABKPQL" varchar(1) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAABJUPW" varchar(4) NOT NULL default ' ',
  "AAAAWAOH" decimal(3,0) NOT NULL default '0',
  "AAABLQLI" smallint(6) NOT NULL default '0',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAWAOM" varchar(8) NOT NULL default '00000000',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABHXCZ" varchar(2) NOT NULL default ' ',
  "AAAAWAOP" varchar(1) NOT NULL default ' ',
  "AAABJDFM" varchar(18) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWAOU" varchar(1) NOT NULL default ' ',
  "AAABHUEI" varchar(18) NOT NULL default ' ',
  "AAABGATM" varchar(2) NOT NULL default ' ',
  "AAABLOWH" varchar(2) NOT NULL default ' ',
  "AAABGATN" varchar(8) NOT NULL default '00000000',
  "AAABLOWI" varchar(8) NOT NULL default '00000000',
  "AAAAWAPA" varchar(1) NOT NULL default ' ',
  "AAAAWAPB" varchar(9) NOT NULL default ' ',
  "AAAAWAPC" decimal(4,0) NOT NULL default '0',
  "AAABLIPC" decimal(4,0) NOT NULL default '0',
  "AAAAWAPE" decimal(3,0) NOT NULL default '0',
  "AAAAWAPF" varchar(3) NOT NULL default ' ',
  "AAAAWAPG" decimal(13,3) NOT NULL default '0.000',
  "AAAAWAPH" decimal(5,0) NOT NULL default '0',
  "AAABLGPH" varchar(18) NOT NULL default ' ',
  "AAAAWAPJ" decimal(13,3) NOT NULL default '0.000',
  "AAABEJLV" varchar(1) NOT NULL default ' ',
  "AAAAWAPL" varchar(18) NOT NULL default '000000000000000000',
  "AAABIBUK" varchar(1) NOT NULL default ' ',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ"),
  KEY "AAAAUBQH" ("AAABMRIL","AAABLXOB"),
  KEY "AAAAUBQK" ("AAABMRIL","AAABLRZL"),
  KEY "AAAAUBQN" ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAWALU" varchar(15) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABIMOU" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABLZRN" varchar(18) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLIPB" varchar(3) NOT NULL default ' ',
  "AAABLOXN" varchar(22) NOT NULL default ' ',
  "AAAAWAME" varchar(3) NOT NULL default ' ',
  "AAAAWAMF" varchar(2) NOT NULL default ' ',
  "AAAAWAMG" varchar(4) NOT NULL default ' ',
  "AAAAWAMH" varchar(6) NOT NULL default ' ',
  "AAAAWAMI" varchar(3) NOT NULL default ' ',
  "AAAAWAMJ" varchar(3) NOT NULL default '000',
  "AAAAWAMK" varchar(18) NOT NULL default ' ',
  "AAABAGBI" varchar(4) NOT NULL default ' ',
  "AAABLIAR" varchar(32) NOT NULL default ' ',
  "AAABHXVX" varchar(14) NOT NULL default ' ',
  "AAAAWAMO" varchar(18) NOT NULL default ' ',
  "AAAAWUMF" varchar(3) NOT NULL default ' ',
  "AAAAWTLU" varchar(4) NOT NULL default ' ',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABLXPY" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAABMLEX" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAAAWKIP" varchar(2) NOT NULL default ' ',
  "AAABHVDW" varchar(2) NOT NULL default ' ',
  "AAAAWKIC" varchar(2) NOT NULL default ' ',
  "AAABDHAN" varchar(3) NOT NULL default ' ',
  "AAABKXVZ" varchar(4) NOT NULL default ' ',
  "AAAAWANB" varchar(18) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABLXRT" varchar(13) NOT NULL default ' ',
  "AAAAWANF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZUKE" varchar(1) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABLSKS" varchar(4) NOT NULL default ' ',
  "AAAAZULA" varchar(2) NOT NULL default ' ',
  "AAAAZULF" varchar(2) NOT NULL default ' ',
  "AAAAWANL" varchar(1) NOT NULL default ' ',
  "AAABLXSK" varchar(18) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABLFAI" decimal(13,3) NOT NULL default '0.000',
  "AAABLFAJ" decimal(13,3) NOT NULL default '0.000',
  "AAABLYMY" decimal(13,3) NOT NULL default '0.000',
  "AAABJRZA" varchar(3) NOT NULL default ' ',
  "AAABHUEF" varchar(18) NOT NULL default ' ',
  "AAAAWANT" varchar(1) NOT NULL default ' ',
  "AAAAWANU" varchar(1) NOT NULL default ' ',
  "AAAAWANV" varchar(1) NOT NULL default ' ',
  "AAAAWANW" decimal(13,3) NOT NULL default '0.000',
  "AAABLVIO" varchar(3) NOT NULL default ' ',
  "AAAAWANY" decimal(13,3) NOT NULL default '0.000',
  "AAAAWANZ" varchar(3) NOT NULL default ' ',
  "AAABJRZC" decimal(3,1) NOT NULL default '0.0',
  "AAABJRZD" decimal(3,1) NOT NULL default '0.0',
  "AAAAWAOC" varchar(1) NOT NULL default ' ',
  "AAAAWAOD" varchar(1) NOT NULL default ' ',
  "AAABKPQL" varchar(1) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAABJUPW" varchar(4) NOT NULL default ' ',
  "AAAAWAOH" decimal(3,0) NOT NULL default '0',
  "AAABLQLI" smallint(6) NOT NULL default '0',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAWAOM" varchar(8) NOT NULL default '00000000',
  "AAABLSKR" varchar(4) NOT NULL default ' ',
  "AAABHXCZ" varchar(2) NOT NULL default ' ',
  "AAAAWAOP" varchar(1) NOT NULL default ' ',
  "AAABJDFM" varchar(18) NOT NULL default ' ',
  "AAABLRZL" varchar(18) NOT NULL default ' ',
  "AAABLSJL" varchar(2) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWAOU" varchar(1) NOT NULL default ' ',
  "AAABHUEI" varchar(18) NOT NULL default ' ',
  "AAABGATM" varchar(2) NOT NULL default ' ',
  "AAABLOWH" varchar(2) NOT NULL default ' ',
  "AAABGATN" varchar(8) NOT NULL default '00000000',
  "AAABLOWI" varchar(8) NOT NULL default '00000000',
  "AAAAWAPA" varchar(1) NOT NULL default ' ',
  "AAAAWAPB" varchar(9) NOT NULL default ' ',
  "AAAAWAPC" decimal(4,0) NOT NULL default '0',
  "AAABLIPC" decimal(4,0) NOT NULL default '0',
  "AAAAWAPE" decimal(3,0) NOT NULL default '0',
  "AAAAWAPF" varchar(3) NOT NULL default ' ',
  "AAAAWAPG" decimal(13,3) NOT NULL default '0.000',
  "AAAAWAPH" decimal(5,0) NOT NULL default '0',
  "AAABLGPH" varchar(18) NOT NULL default ' ',
  "AAAAWAPJ" decimal(13,3) NOT NULL default '0.000',
  "AAABEJLV" varchar(1) NOT NULL default ' ',
  "AAAAWAPL" varchar(18) NOT NULL default '000000000000000000',
  "AAABIBUK" varchar(1) NOT NULL default ' ',
  "AAABLXUQ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ"),
  KEY "AAAAUBUT" ("AAABMRIL","AAABLXOB"),
  KEY "AAAAUBUW" ("AAABMRIL","AAABLRZL"),
  KEY "AAAAUBUZ" ("AAABMRIL","AAABMLCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUBVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUCBL" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUCBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUCHX" ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUCIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUCOJ" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUCOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUCUV" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUCUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUDBH" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUDHT" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLLZG" varchar(1) NOT NULL default ' ',
  "AAABLXRY" varchar(1) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default ' ',
  "AAAAXZQX" varchar(8) NOT NULL default '00000000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAAAVXQU" varchar(1) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) NOT NULL default ' ',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) NOT NULL default '0.00',
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) NOT NULL default ' ',
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVZUE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) NOT NULL default '0.00',
  "AAAAWTPV" varchar(1) NOT NULL default ' ',
  "AAAAWUXL" varchar(1) NOT NULL default ' ',
  "AAAAVXRS" varchar(1) NOT NULL default ' ',
  "AAABKRSN" varchar(1) NOT NULL default ' ',
  "AAAAXCSW" varchar(8) NOT NULL default '00000000',
  "AAAAVXRV" varchar(18) NOT NULL default ' ',
  "AAAAVXRW" varchar(1) NOT NULL default ' ',
  "AAAAVXRX" varchar(1) NOT NULL default ' ',
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) NOT NULL default ' ',
  "AAAAVXSA" varchar(1) NOT NULL default ' ',
  "AAABKRRK" varchar(1) NOT NULL default ' ',
  "AAABKNSN" varchar(3) NOT NULL default ' ',
  "AAABKOPN" decimal(5,2) NOT NULL default '0.00',
  "AAABGFZR" decimal(5,2) NOT NULL default '0.00',
  "AAABKOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXSG" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXSH" decimal(3,0) NOT NULL default '0',
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) NOT NULL default ' ',
  "AAAAVXSL" varchar(1) NOT NULL default ' ',
  "AAAAVXSM" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSN" varchar(1) NOT NULL default ' ',
  "AAAAVXSO" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSP" decimal(3,0) NOT NULL default '0',
  "AAABLXPH" varchar(1) NOT NULL default ' ',
  "AAAAVXSR" decimal(5,2) NOT NULL default '0.00',
  "AAAAWCYS" varchar(2) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAP" decimal(3,1) NOT NULL default '0.0',
  "AAAAVXSV" decimal(3,0) NOT NULL default '0',
  "AAAAVXSW" varchar(8) NOT NULL default ' ',
  "AAABGOAV" decimal(5,0) NOT NULL default '0',
  "AAAAVXSY" varchar(1) NOT NULL default ' ',
  "AAAAVXSZ" varchar(1) NOT NULL default ' ',
  "AAAAVXTA" decimal(9,0) NOT NULL default '0',
  "AAABGOBD" decimal(5,0) NOT NULL default '0',
  "AAAAVXTC" varchar(8) NOT NULL default '00000000',
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) NOT NULL default ' ',
  "AAAAVXTG" varchar(1) NOT NULL default ' ',
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAVXTO" varchar(1) NOT NULL default ' ',
  "AAAAVXTP" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXTR" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXTS" varchar(4) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) NOT NULL default ' ',
  "AAAAZRSO" varchar(17) NOT NULL default ' ',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVXTY" varchar(3) NOT NULL default ' ',
  "AAAAVXTZ" varchar(3) NOT NULL default ' ',
  "AAABHNFH" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) NOT NULL default ' ',
  "AAABGPHE" varchar(1) NOT NULL default ' ',
  "AAAAVXUF" varchar(3) NOT NULL default '000',
  "AAAAWUQB" varchar(1) NOT NULL default ' ',
  "AAAAWUQC" varchar(3) NOT NULL default '000',
  "AAAAWUQD" varchar(3) NOT NULL default '000',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKQWS" varchar(8) NOT NULL default ' ',
  "AAAAVXUN" varchar(2) NOT NULL default ' ',
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) NOT NULL default ' ',
  "AAAAVXUQ" varchar(3) NOT NULL default ' ',
  "AAAAVXUR" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  "AAABJBMG" decimal(5,2) NOT NULL default '0.00',
  "AAAAVXUU" varchar(4) NOT NULL default ' ',
  "AAAAVXUV" varchar(1) NOT NULL default ' ',
  "AAAAVXUW" decimal(3,0) NOT NULL default '0',
  "AAAAWUTA" varchar(3) NOT NULL default ' ',
  "AAAAVXUY" varchar(10) NOT NULL default ' ',
  "AAAAWKZD" varchar(1) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABKOAT" varchar(4) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVD" varchar(18) NOT NULL default ' ',
  "AAAAVXVE" varchar(4) NOT NULL default ' ',
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAVXVJ" varchar(18) NOT NULL default ' ',
  "AAAAVXVK" varchar(4) NOT NULL default ' ',
  "AAAAVXVL" varchar(8) NOT NULL default '00000000',
  "AAAAVXVM" decimal(4,2) NOT NULL default '0.00',
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) NOT NULL default ' ',
  "AAAAVXVP" varchar(1) NOT NULL default ' ',
  "AAABBHKA" varchar(8) NOT NULL default '00000000',
  "AAAAVXVR" varchar(8) NOT NULL default '00000000',
  "AAAAVXVS" varchar(1) NOT NULL default ' ',
  "AAAAVXVT" varchar(8) NOT NULL default '00000000',
  "AAAAVXVU" varchar(1) NOT NULL default ' ',
  "AAABHXUY" varchar(1) NOT NULL default ' ',
  "AAABKRSF" varchar(1) NOT NULL default ' ',
  "AAAAWWNH" varchar(2) NOT NULL default ' ',
  "AAAAVXVY" varchar(18) NOT NULL default '000000000000000000',
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) NOT NULL default ' ',
  "AAAAVXWB" varchar(1) NOT NULL default ' ',
  "AAAAWUKU" varchar(4) NOT NULL default ' ',
  "AAAAVXWD" varchar(6) NOT NULL default ' ',
  "AAAAWUQL" decimal(3,0) NOT NULL default '0',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAAAVXWG" varchar(3) NOT NULL default ' ',
  "AAAAVXWH" varchar(4) NOT NULL default ' ',
  "AAAAVXWI" decimal(2,1) NOT NULL default '0.0',
  "AAABKNVV" varchar(6) NOT NULL default ' ',
  "AAAAWUQM" varchar(1) NOT NULL default ' ',
  "AAAAWUUK" varchar(2) NOT NULL default '00',
  "AAABKQXS" varchar(2) NOT NULL default ' ',
  "AAABKRTO" varchar(1) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUDOF" ("AAABMRIL","AAABLQPZ"),
  KEY "AAAAUDOI" ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) default NULL,
  "AAABLLZG" varchar(1) default NULL,
  "AAABLXRY" varchar(1) default NULL,
  "AAAAYIKV" varchar(2) default NULL,
  "AAAAXZQX" varchar(8) default NULL,
  "AAABFXAO" varchar(1) default NULL,
  "AAAAVXQU" varchar(1) default NULL,
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAAAVXQW" varchar(3) NOT NULL default ' ',
  "AAABLQPZ" varchar(4) default NULL,
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAABLURO" varchar(3) NOT NULL default ' ',
  "AAAAVXRA" varchar(1) default NULL,
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  "AAABLLYR" decimal(3,0) NOT NULL default '0',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAAAVXRE" decimal(5,2) default NULL,
  "AAAAWUYE" varchar(2) NOT NULL default ' ',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKRYO" varchar(2) default NULL,
  "AAABLQYC" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYD" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXRK" decimal(13,3) default NULL,
  "AAAAVZUE" decimal(13,3) default NULL,
  "AAAAVXRM" decimal(13,3) default NULL,
  "AAAAVXRN" decimal(13,3) NOT NULL default '0.000',
  "AAABLQYB" decimal(13,3) NOT NULL default '0.000',
  "AAABLIQO" decimal(11,2) default NULL,
  "AAAAWTPV" varchar(1) default NULL,
  "AAAAWUXL" varchar(1) default NULL,
  "AAAAVXRS" varchar(1) default NULL,
  "AAABKRSN" varchar(1) default NULL,
  "AAAAXCSW" varchar(8) default NULL,
  "AAAAVXRV" varchar(18) default NULL,
  "AAAAVXRW" varchar(1) default NULL,
  "AAAAVXRX" varchar(1) default NULL,
  "AAABKNSO" varchar(3) NOT NULL default ' ',
  "AAAAVXRZ" varchar(1) default NULL,
  "AAAAVXSA" varchar(1) default NULL,
  "AAABKRRK" varchar(1) default NULL,
  "AAABKNSN" varchar(3) default NULL,
  "AAABKOPN" decimal(5,2) default NULL,
  "AAABGFZR" decimal(5,2) default NULL,
  "AAABKOPT" decimal(5,2) default NULL,
  "AAAAVXSG" decimal(13,3) default NULL,
  "AAAAVXSH" decimal(3,0) default NULL,
  "AAAAVXSI" decimal(5,0) NOT NULL default '0',
  "AAAAVXSJ" varchar(3) NOT NULL default ' ',
  "AAAAVXSK" varchar(1) default NULL,
  "AAAAVXSL" varchar(1) default NULL,
  "AAAAVXSM" decimal(3,1) default NULL,
  "AAAAVXSN" varchar(1) default NULL,
  "AAAAVXSO" decimal(3,1) default NULL,
  "AAAAVXSP" decimal(3,0) default NULL,
  "AAABLXPH" varchar(1) default NULL,
  "AAAAVXSR" decimal(5,2) default NULL,
  "AAAAWCYS" varchar(2) default NULL,
  "AAABLLYU" varchar(1) default NULL,
  "AAABGOAP" decimal(3,1) default NULL,
  "AAAAVXSV" decimal(3,0) default NULL,
  "AAAAVXSW" varchar(8) default NULL,
  "AAABGOAV" decimal(5,0) default NULL,
  "AAAAVXSY" varchar(1) default NULL,
  "AAAAVXSZ" varchar(1) default NULL,
  "AAAAVXTA" decimal(9,0) default NULL,
  "AAABGOBD" decimal(5,0) default NULL,
  "AAAAVXTC" varchar(8) default NULL,
  "AAAAVXTD" decimal(13,3) NOT NULL default '0.000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABLXRX" varchar(1) default NULL,
  "AAAAVXTG" varchar(1) default NULL,
  "AAABLIOW" decimal(3,1) NOT NULL default '0.0',
  "AAAAWUXX" varchar(1) default NULL,
  "AAABJFHJ" varchar(2) default NULL,
  "AAABLHPM" varchar(2) default NULL,
  "AAABLUNJ" varchar(8) default NULL,
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) default NULL,
  "AAAAVXTO" varchar(1) default NULL,
  "AAAAVXTP" decimal(5,2) default NULL,
  "AAAAVXTQ" decimal(13,3) default NULL,
  "AAAAVXTR" decimal(5,2) default NULL,
  "AAAAVXTS" varchar(4) default NULL,
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAAAVXTU" varchar(1) NOT NULL default ' ',
  "AAAAWCFR" varchar(1) default NULL,
  "AAAAZRSO" varchar(17) default NULL,
  "AAAAZQYQ" varchar(3) default NULL,
  "AAAAVXTY" varchar(3) default NULL,
  "AAAAVXTZ" varchar(3) default NULL,
  "AAABHNFH" varchar(4) default NULL,
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAVXUC" decimal(13,3) NOT NULL default '0.000',
  "AAABLURE" varchar(3) default NULL,
  "AAABGPHE" varchar(1) default NULL,
  "AAAAVXUF" varchar(3) default NULL,
  "AAAAWUQB" varchar(1) default NULL,
  "AAAAWUQC" varchar(3) default NULL,
  "AAAAWUQD" varchar(3) default NULL,
  "AAABFJAY" varchar(1) default NULL,
  "AAABKRTQ" varchar(2) default NULL,
  "AAABKQVZ" varchar(1) default NULL,
  "AAABKQWS" varchar(8) default NULL,
  "AAAAVXUN" varchar(2) default NULL,
  "AAABBHVV" decimal(13,3) NOT NULL default '0.000',
  "AAAAVXUP" varchar(2) default NULL,
  "AAAAVXUQ" varchar(3) default NULL,
  "AAAAVXUR" varchar(4) default NULL,
  "AAABIFNV" varchar(4) default NULL,
  "AAABJBMG" decimal(5,2) default NULL,
  "AAAAVXUU" varchar(4) default NULL,
  "AAAAVXUV" varchar(1) default NULL,
  "AAAAVXUW" decimal(3,0) default NULL,
  "AAAAWUTA" varchar(3) default NULL,
  "AAAAVXUY" varchar(10) default NULL,
  "AAAAWKZD" varchar(1) default NULL,
  "AAABKOUS" varchar(6) default NULL,
  "AAABKOAT" varchar(4) default NULL,
  "AAABLXTN" varchar(18) default NULL,
  "AAAAVXVD" varchar(18) default NULL,
  "AAAAVXVE" varchar(4) default NULL,
  "AAAAVXVF" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) default NULL,
  "AAABLURG" varchar(3) NOT NULL default ' ',
  "AAABLURM" varchar(4) default NULL,
  "AAAAVXVJ" varchar(18) default NULL,
  "AAAAVXVK" varchar(4) default NULL,
  "AAAAVXVL" varchar(8) default NULL,
  "AAAAVXVM" decimal(4,2) default NULL,
  "AAAAVXVN" varchar(1) NOT NULL default ' ',
  "AAABLXUF" varchar(1) default NULL,
  "AAAAVXVP" varchar(1) default NULL,
  "AAABBHKA" varchar(8) default NULL,
  "AAAAVXVR" varchar(8) default NULL,
  "AAAAVXVS" varchar(1) default NULL,
  "AAAAVXVT" varchar(8) default NULL,
  "AAAAVXVU" varchar(1) default NULL,
  "AAABHXUY" varchar(1) default NULL,
  "AAABKRSF" varchar(1) default NULL,
  "AAAAWWNH" varchar(2) default NULL,
  "AAAAVXVY" varchar(18) default NULL,
  "AAAAVXVZ" varchar(4) NOT NULL default ' ',
  "AAABKRQJ" varchar(1) default NULL,
  "AAAAVXWB" varchar(1) default NULL,
  "AAAAWUKU" varchar(4) default NULL,
  "AAAAVXWD" varchar(6) default NULL,
  "AAAAWUQL" decimal(3,0) default NULL,
  "AAABKQWU" varchar(1) default NULL,
  "AAAAVXWG" varchar(3) default NULL,
  "AAAAVXWH" varchar(4) default NULL,
  "AAAAVXWI" decimal(2,1) default NULL,
  "AAABKNVV" varchar(6) default NULL,
  "AAAAWUQM" varchar(1) default NULL,
  "AAAAWUUK" varchar(2) default NULL,
  "AAABKQXS" varchar(2) default NULL,
  "AAABKRTO" varchar(1) default NULL,
  "AAABLURK" varchar(4) default NULL,
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI"),
  KEY "AAAAUDUU" ("AAABMRIL","AAABLQPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUDYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUECL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABAHTV" varchar(15) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABLHRV" varchar(4) NOT NULL default '0000',
  "AAABLHRW" varchar(2) NOT NULL default '00',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLLOE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFK" decimal(13,3) NOT NULL default '0.000',
  "AAAAZWZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFO" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFP" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFQ" decimal(13,3) NOT NULL default '0.000',
  "AAAAWTVU" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFS" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYFU" varchar(3) NOT NULL default ' ',
  "AAAAVYFV" varchar(3) NOT NULL default ' ',
  "AAAAVYFW" varchar(3) NOT NULL default ' ',
  "AAAAVYFX" varchar(3) NOT NULL default ' ',
  "AAAAVYFY" varchar(3) NOT NULL default ' ',
  "AAAAVYFZ" varchar(3) NOT NULL default ' ',
  "AAAAVYGA" varchar(3) NOT NULL default ' ',
  "AAAAVYGB" varchar(3) NOT NULL default ' ',
  "AAAAWVQO" varchar(1) NOT NULL default ' ',
  "AAAAVYGD" varchar(2) NOT NULL default ' ',
  "AAAAVYGE" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGF" decimal(13,3) NOT NULL default '0.000',
  "AAAAZQYQ" varchar(3) NOT NULL default ' ',
  "AAAAVYGH" varchar(1) NOT NULL default ' ',
  "AAAAWPFZ" varchar(2) NOT NULL default ' ',
  "AAAAVYGJ" varchar(10) NOT NULL default ' ',
  "AAAAVYGK" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGL" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGM" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGN" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYGO" varchar(8) NOT NULL default '00000000',
  "AAAAWEHF" varchar(10) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAABMCEO"),
  KEY "AAAAUEIC" ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMLHP" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABLXRS" varchar(2) NOT NULL default ' ',
  "AAABLXRM" varchar(1) NOT NULL default ' ',
  "AAABMACA" varchar(2) NOT NULL default ' ',
  "AAAAVYSY" varchar(8) NOT NULL default '00000000',
  "AAAAVYSZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTB" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTC" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTD" varchar(3) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABKOBV" varchar(4) NOT NULL default ' ',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABLXNZ" varchar(18) NOT NULL default ' ',
  "AAABLXRP" varchar(2) NOT NULL default ' ',
  "AAABLXRQ" varchar(2) NOT NULL default ' ',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABHXWE" varchar(1) NOT NULL default ' ',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABLRZB" varchar(2) NOT NULL default ' ',
  "AAAAVYTU" varchar(1) NOT NULL default ' ',
  "AAAAVYTV" varchar(8) NOT NULL default '00000000',
  "AAAAVYTW" varchar(8) NOT NULL default '00000000',
  "AAAAVYTX" varchar(8) NOT NULL default '00000000',
  "AAAAVYTY" varchar(8) NOT NULL default '00000000',
  "AAAAVYTZ" varchar(8) NOT NULL default '00000000',
  "AAAAVYUA" varchar(8) NOT NULL default '00000000',
  "AAAAVYUB" varchar(8) NOT NULL default '00000000',
  "AAAAVYUC" varchar(8) NOT NULL default '00000000',
  "AAAAVYUD" varchar(1) NOT NULL default ' ',
  "AAAAVYUE" varchar(1) NOT NULL default ' ',
  "AAAAVYUF" varchar(1) NOT NULL default ' ',
  "AAAAVYUG" varchar(1) NOT NULL default ' ',
  "AAAAVYUH" varchar(1) NOT NULL default ' ',
  "AAAAVYUI" varchar(1) NOT NULL default ' ',
  "AAAAVYUJ" varchar(1) NOT NULL default ' ',
  "AAAAVYUK" varchar(1) NOT NULL default ' ',
  "AAAAVYUL" varchar(1) NOT NULL default ' ',
  "AAABBCTM" varchar(1) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  "AAAAVYUP" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMLHP" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(2) NOT NULL default ' ',
  "AAABLXRS" varchar(2) NOT NULL default ' ',
  "AAABLXRM" varchar(1) NOT NULL default ' ',
  "AAABMACA" varchar(2) NOT NULL default ' ',
  "AAAAVYSY" varchar(8) NOT NULL default '00000000',
  "AAAAVYSZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLXRZ" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTB" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTC" decimal(13,3) NOT NULL default '0.000',
  "AAAAVYTD" varchar(3) NOT NULL default ' ',
  "AAABLZJP" varchar(3) NOT NULL default ' ',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABKOBV" varchar(4) NOT NULL default ' ',
  "AAABLXOK" varchar(18) NOT NULL default ' ',
  "AAABLXNZ" varchar(18) NOT NULL default ' ',
  "AAABLXRP" varchar(2) NOT NULL default ' ',
  "AAABLXRQ" varchar(2) NOT NULL default ' ',
  "AAABLXSN" varchar(3) NOT NULL default ' ',
  "AAABLXSO" varchar(3) NOT NULL default ' ',
  "AAABLXSP" varchar(3) NOT NULL default ' ',
  "AAABLXSQ" varchar(3) NOT NULL default ' ',
  "AAABLXSR" varchar(3) NOT NULL default ' ',
  "AAABLRZA" varchar(2) NOT NULL default ' ',
  "AAABHXWE" varchar(1) NOT NULL default ' ',
  "AAABLRYX" varchar(2) NOT NULL default ' ',
  "AAABLRZB" varchar(2) NOT NULL default ' ',
  "AAAAVYTU" varchar(1) NOT NULL default ' ',
  "AAAAVYTV" varchar(8) NOT NULL default '00000000',
  "AAAAVYTW" varchar(8) NOT NULL default '00000000',
  "AAAAVYTX" varchar(8) NOT NULL default '00000000',
  "AAAAVYTY" varchar(8) NOT NULL default '00000000',
  "AAAAVYTZ" varchar(8) NOT NULL default '00000000',
  "AAAAVYUA" varchar(8) NOT NULL default '00000000',
  "AAAAVYUB" varchar(8) NOT NULL default '00000000',
  "AAAAVYUC" varchar(8) NOT NULL default '00000000',
  "AAAAVYUD" varchar(1) NOT NULL default ' ',
  "AAAAVYUE" varchar(1) NOT NULL default ' ',
  "AAAAVYUF" varchar(1) NOT NULL default ' ',
  "AAAAVYUG" varchar(1) NOT NULL default ' ',
  "AAAAVYUH" varchar(1) NOT NULL default ' ',
  "AAAAVYUI" varchar(1) NOT NULL default ' ',
  "AAAAVYUJ" varchar(1) NOT NULL default ' ',
  "AAAAVYUK" varchar(1) NOT NULL default ' ',
  "AAAAVYUL" varchar(1) NOT NULL default ' ',
  "AAABBCTM" varchar(1) NOT NULL default ' ',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAABLURK" varchar(4) NOT NULL default ' ',
  "AAAAVYUP" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMQII","AAABMJXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(3) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAAAUEND" varchar(1) NOT NULL default ' ',
  "AAAAUENE" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABGAJY","AAABCBUI","AAABBACP","AAABKJVG"),
  KEY "AAAAUENO" ("AAABMRIL","AAABKJVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUENR" (
  "AAAAUEOA" varchar(8) NOT NULL default ' ',
  "AAAAUENT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUEOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUENV" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAAAUEOA" varchar(8) NOT NULL default ' ',
  "AAAAUENY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAAAUEOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIWK" varchar(24) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFDS" varchar(8) NOT NULL default '00000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAAUFCY" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIWK","AAABLESX","AAAAUGAU","AAABCRWQ","AAABIJNY","AAAAUFSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCWA" varchar(30) NOT NULL default ' ',
  "AAAAUERX" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUGAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCWA","AAAAUERX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAAAUETL" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  "AAAAYBCK" varchar(2) NOT NULL default ' ',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAAAUESJ" varchar(1) NOT NULL default ' ',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABKZAW" text,
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAAAUETL","AAAAUGAU","AAABCRWQ","AAABIJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGRXB" varchar(50) NOT NULL default ' ',
  "AAABIYJL" varchar(40) NOT NULL default ' ',
  "AAABIWXH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABKXSJ","AAABLRSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAAAUESJ" varchar(1) NOT NULL default ' ',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABKZAW" text,
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABCRWQ","AAABIJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUERB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAYBCK" varchar(2) NOT NULL default ' ',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAAAUESJ" varchar(1) NOT NULL default ' ',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABKZAW" text,
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUERP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCWA" varchar(30) NOT NULL default ' ',
  "AAAAUERX" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  "AAAAUGAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCWA","AAAAUERX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUERY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAAAUETL" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(6) NOT NULL default '000000',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAAAUESJ" varchar(1) NOT NULL default ' ',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABKZAW" text,
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAAAUETL","AAAAUGAU","AAABHMSE","AAABCRWQ","AAABIJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAAAUETL" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUESY" varchar(8) NOT NULL default '00000000',
  "AAAAUESZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFOC" varchar(12) NOT NULL default ' ',
  "AAAAUETB" varchar(40) NOT NULL default ' ',
  "AAABACUG" varchar(8) NOT NULL default '00000000',
  "AAABACUI" varchar(8) NOT NULL default '00000000',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAAAUETF" varchar(12) NOT NULL default ' ',
  "AAAAUGAO" varchar(1) NOT NULL default ' ',
  "AAABILEY" varchar(1) NOT NULL default ' ',
  "AAAAUGAQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAAAUETL","AAAAUGAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUETN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFPX" varchar(3) NOT NULL default '000',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUETV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFSX" varchar(3) NOT NULL default '000',
  "AAABLFWZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAABLUSV" varchar(3) NOT NULL default '000',
  "AAAAUEUQ" varchar(1) NOT NULL default ' ',
  "AAABAWWL" varchar(2) NOT NULL default ' ',
  "AAAAUFSJ" varchar(30) NOT NULL default ' ',
  "AAAAUEUK" varchar(1) NOT NULL default ' ',
  "AAAAUEUL" varchar(1) NOT NULL default '0',
  "AAAAUFRY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAABLUSV","AAAAUEUQ","AAABAWWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFQQ" varchar(3) NOT NULL default '000',
  "AAAAUFQL" varchar(64) NOT NULL default ' ',
  "AAAAUEUX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFRP" varchar(3) NOT NULL default '000',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFRP" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(3) NOT NULL default '000',
  "AAAAUFRY" varchar(1) NOT NULL default '0',
  "AAABLFWZ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFRP","AAAAUFSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAABFYLB" varchar(3) NOT NULL default '000',
  "AAAAUFRW" varchar(1) NOT NULL default ' ',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAAAUEWB" varchar(1) NOT NULL default '0',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAABFYLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUEXW" varchar(8) NOT NULL default '00000000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUFXI" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAAAUEXS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUEXW"),
  UNIQUE KEY "AAAAUEWW" ("AAABMRIL","AAAAUGAW","AAABKXSJ","AAABLOZE","AAABLTVR","AAAAUFXI","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUEXW" varchar(8) NOT NULL default '00000000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUFXI" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAAAUEXS" varchar(25) NOT NULL default ' ',
  "AAABMECK" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUEXW"),
  UNIQUE KEY "AAAAUEXX" ("AAABMRIL","AAAAUGAW","AAABKXSJ","AAABLOZE","AAABLTVR","AAAAUFXI","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFSP" varchar(3) NOT NULL default '000',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMHK","AAAAUFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUEYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABBLCZ" varchar(10) NOT NULL default ' ',
  "AAABLTYY" varchar(32) NOT NULL default ' ',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAAAUFXI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAABJPEL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAAAUEZE" varchar(1) NOT NULL default '0',
  "AAABLECQ" varchar(64) NOT NULL default ' ',
  "AAAAUEZG" decimal(3,2) NOT NULL default '0.00',
  "AAABFIGO" decimal(3,2) NOT NULL default '0.00',
  "AAAAUEZI" decimal(3,2) NOT NULL default '0.00',
  "AAAAUEZJ" varchar(1) NOT NULL default '0',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAABMKCG" varchar(255) NOT NULL default ' ',
  "AAAAUEZM" varchar(1) NOT NULL default ' ',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIMHK"),
  UNIQUE KEY "AAAAUEZR" ("AAABMRIL","AAABLOZE","AAABLTVR","AAABLTYY","AAAAUFWZ","AAAAUFXA","AAAAUFXB","AAAAUFXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAAAUFJT" varchar(12) NOT NULL default ' ',
  "AAABAOOL" varchar(40) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(15) NOT NULL default ' ',
  "AAABIYJJ" varchar(20) NOT NULL default ' ',
  "AAAAUFAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAAAUFJT" varchar(12) NOT NULL default ' ',
  "AAABAOOL" varchar(40) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUFXI" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFJT" varchar(12) NOT NULL default ' ',
  "AAAAUFYQ" varchar(12) NOT NULL default ' ',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABKXSJ","AAAAUGAW","AAABLOZE","AAABLTVR","AAAAUFXI","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIWK" varchar(24) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUFDS" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(8) NOT NULL default '00000000',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAAUFCV" varchar(10) NOT NULL default ' ',
  "AAABMJWH" varchar(10) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAAUFCY" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIWK","AAABLESX","AAAAUGAU","AAAAUFDS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIWK" varchar(24) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUFDS" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(8) NOT NULL default '00000000',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAAAUFCV" varchar(10) NOT NULL default ' ',
  "AAABMJWH" varchar(10) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAAAUFCY" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIWK","AAABLESX","AAAAUGAU","AAAAUFDS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIWK" varchar(24) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUFDS" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(8) NOT NULL default '00000000',
  "AAABJSYW" smallint(6) NOT NULL default '0',
  "AAABLSBY" text,
  PRIMARY KEY ("AAABMRIL","AAABGIWK","AAABLESX","AAAAUGAU","AAAAUFDS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVBPJ" varchar(24) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAAAUFDY" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAAAUFEA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVBPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVBPJ" varchar(24) NOT NULL default ' ',
  "AAAAUFEP" varchar(24) NOT NULL default ' ',
  "AAABMLQA" varchar(2) NOT NULL default ' ',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABCRGY" varchar(20) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAUFEM" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVBPJ","AAAAUFEP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBCK" varchar(2) NOT NULL default ' ',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAAAUFEZ" varchar(2) NOT NULL default ' ',
  "AAAAVBJH" varchar(45) NOT NULL default ' ',
  "AAAAUFFB" varchar(12) NOT NULL default ' ',
  "AAAAYGQC" varchar(40) NOT NULL default ' ',
  "AAAAUFFD" varchar(40) NOT NULL default ' ',
  "AAAAUFFE" varchar(20) NOT NULL default ' ',
  "AAAAUFFF" varchar(20) NOT NULL default ' ',
  "AAAAUFFG" varchar(20) NOT NULL default ' ',
  "AAAAUFFH" varchar(20) NOT NULL default ' ',
  "AAAAUFFI" varchar(20) NOT NULL default ' ',
  "AAAAUFFJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYBCK","AAABILOU","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLESS" varchar(16) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABMDXM" varchar(72) NOT NULL default ' ',
  "AAAAUFFT" varchar(4) NOT NULL default ' ',
  "AAABEORQ" varchar(4) NOT NULL default ' ',
  "AAAAUFFV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAAAYBCK" varchar(2) NOT NULL default ' ',
  "AAABILOU" varchar(45) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABAMNN" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAABBKHW" varchar(2) NOT NULL default ' ',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAAAUGAO" varchar(1) NOT NULL default ' ',
  "AAAAUFIC" varchar(1) NOT NULL default ' ',
  "AAABAEST" varchar(15) NOT NULL default ' ',
  "AAAAUFGM" varchar(1) NOT NULL default ' ',
  "AAAAUFGN" varchar(25) NOT NULL default ' ',
  "AAAAUFGO" varchar(25) NOT NULL default ' ',
  "AAAAUFQX" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABGCTO" varchar(1) NOT NULL default ' ',
  "AAABEXVV" varchar(1) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABAZFT" varchar(1) NOT NULL default ' ',
  "AAAAUFGV" varchar(1) NOT NULL default ' ',
  "AAABILEY" varchar(1) NOT NULL default ' ',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG"),
  KEY "AAAAUFHB" ("AAABMRIL","AAAAYBCK","AAABILOU","AAABAMNN","AAABHMSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFOZT" varchar(2) NOT NULL default ' ',
  "AAABKAHC" varchar(45) NOT NULL default ' ',
  "AAAAUFHL" varchar(12) NOT NULL default ' ',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAAAUFHO" varchar(1) NOT NULL default ' ',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAAAUFYQ" varchar(12) NOT NULL default ' ',
  "AAAAXBOG" varchar(40) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUFMW" varchar(2) NOT NULL default ' ',
  "AAAAUFHU" varchar(12) NOT NULL default ' ',
  "AAAAUFHV" varchar(40) NOT NULL default ' ',
  "AAABACUG" varchar(8) NOT NULL default '00000000',
  "AAABACUI" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUFND" varchar(8) NOT NULL default '00000000',
  "AAAAUFNE" varchar(12) NOT NULL default ' ',
  "AAABFEIS" varchar(1) NOT NULL default ' ',
  "AAAAUFIC" varchar(1) NOT NULL default ' ',
  "AAAAUFID" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAABBKHW" varchar(2) NOT NULL default ' ',
  "AAAAUFWK" varchar(8) NOT NULL default '00000000',
  "AAAAUFII" varchar(64) NOT NULL default ' ',
  "AAAAUFWL" varchar(8) NOT NULL default '00000000',
  "AAAAUFIK" varchar(64) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default '0',
  "AAAAUFTB" varchar(3) NOT NULL default ' ',
  "AAAAUFIN" varchar(1) NOT NULL default ' ',
  "AAAAUFTA" varchar(4) NOT NULL default ' ',
  "AAAAUFSZ" varchar(8) NOT NULL default ' ',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABKGKQ" varchar(24) NOT NULL default ' ',
  "AAAAUFNS" varchar(8) NOT NULL default ' ',
  "AAAAUFNH" varchar(40) NOT NULL default ' ',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAAAUFNL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFOZT","AAABKAHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAAAUFJL" varchar(2) NOT NULL default ' ',
  "AAAAUFJM" varchar(8) NOT NULL default '00000000',
  "AAAAUFJN" varchar(6) NOT NULL default '000000',
  "AAAAUFJH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAAAUFTG","AAAAUFJL","AAAAUFJM","AAAAUFJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABKXSJ" varchar(8) NOT NULL default '00000000',
  "AAAAUFJT" varchar(12) NOT NULL default ' ',
  "AAABAOOL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABJFHJ","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAABATZR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHMSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAAAUFYQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAAAUGAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAYY" varchar(2) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFWL" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABBLCZ" varchar(10) NOT NULL default ' ',
  "AAABLTYY" varchar(32) NOT NULL default ' ',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAAAUFXI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAABJPEL" varchar(10) NOT NULL default ' ',
  "AAABLECQ" varchar(64) NOT NULL default ' ',
  "AAAAUFVE" decimal(5,2) NOT NULL default '0.00',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAAAUFLM" varchar(255) NOT NULL default ' ',
  "AAABMOHM" varchar(20) NOT NULL default ' ',
  "AAAAWDEM" varchar(20) NOT NULL default ' ',
  "AAABLFWZ" varchar(20) NOT NULL default ' ',
  "AAAAUFLQ" varchar(255) NOT NULL default ' ',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAUFWL"),
  UNIQUE KEY "AAAAUFLU" ("AAABMRIL","AAABLOZE","AAABLTVR","AAABLTYY","AAAAUFWZ","AAAAUFXA","AAAAUFXB","AAAAUFXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAAAUFWL" varchar(8) NOT NULL default '00000000',
  "AAABICEF" varchar(3) NOT NULL default '000',
  "AAAAUFVE" decimal(5,2) NOT NULL default '0.00',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAAAUFVG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHMSE","AAAAUFWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAABKAHC" varchar(45) NOT NULL default ' ',
  "AAAAUFNS" varchar(8) NOT NULL default ' ',
  "AAABIGLD" varchar(12) NOT NULL default ' ',
  "AAAAUFMV" varchar(8) NOT NULL default ' ',
  "AAAAUFMW" varchar(2) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAAAUFMZ" varchar(12) NOT NULL default ' ',
  "AAAAUFNA" varchar(8) NOT NULL default ' ',
  "AAABACUG" varchar(8) NOT NULL default '00000000',
  "AAABACUI" varchar(8) NOT NULL default '00000000',
  "AAAAUFND" varchar(8) NOT NULL default '00000000',
  "AAAAUFNE" varchar(12) NOT NULL default ' ',
  "AAABFOZT" varchar(2) NOT NULL default ' ',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAAAUFNH" varchar(40) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAAAUFNL" varchar(40) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAAAUFZA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGLE","AAABIGLF","AAABKAHC","AAAAUFNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLESX" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAABIJNY" varchar(8) NOT NULL default ' ',
  "AAABMLDX" varchar(4) NOT NULL default '0000',
  "AAAAUFOB" varchar(3) NOT NULL default '000',
  "AAAAUFOC" varchar(12) NOT NULL default ' ',
  "AAABBENX" varchar(40) NOT NULL default ' ',
  "AAAAUFOE" varchar(12) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABJVWR" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLESX","AAAAUGAU","AAABCRWQ","AAABIJNY","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKCG" varchar(10) NOT NULL default ' ',
  "AAABIVJW" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(2) NOT NULL default ' ',
  "AAABMDXM" varchar(25) NOT NULL default ' ',
  "AAAAUFOU" varchar(1) NOT NULL default ' ',
  "AAAAWXDT" varchar(1) NOT NULL default ' ',
  "AAAAUFOW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKCG","AAABIVJW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFPL" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFPH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAAAUFPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFPX" varchar(3) NOT NULL default '000',
  "AAAAUFPR" varchar(255) NOT NULL default ' ',
  "AAAAUFSJ" varchar(30) NOT NULL default ' ',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAAAUFPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHRZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABMOQQ" text,
  PRIMARY KEY ("AAABMRIL","AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFQQ" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFQL" varchar(60) NOT NULL default ' ',
  "AAAAUFRY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAAAUFQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAAAUFQX" varchar(1) NOT NULL default ' ',
  "AAAAUFTA" varchar(4) NOT NULL default '0000',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFRP" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFSJ" varchar(30) NOT NULL default ' ',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAAAUFRY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAAAUFRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAABFYLB" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFRW" varchar(1) NOT NULL default ' ',
  "AAAAUFRX" varchar(255) NOT NULL default ' ',
  "AAAAUFRY" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAABFYLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  "AAAAUFSP" varchar(3) NOT NULL default '000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAAAUFSJ" varchar(30) NOT NULL default ' ',
  "AAAAUFSK" varchar(2) NOT NULL default '00',
  "AAAAUFSL" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABIMHK","AAAAUFSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFTG" varchar(8) NOT NULL default '00000000',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAAAUFWK" varchar(8) NOT NULL default '00000000',
  "AAAAUFWL" varchar(8) NOT NULL default '00000000',
  "AAAAUFSX" varchar(8) NOT NULL default '00000000',
  "AAABLMLH" varchar(1) NOT NULL default '0',
  "AAAAUFSZ" decimal(6,3) NOT NULL default '0.000',
  "AAAAUFTA" varchar(4) NOT NULL default '0000',
  "AAAAUFTB" varchar(3) NOT NULL default ' ',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAUFTG","AAABLMKH","AAABHMSE","AAAAUFWK","AAAAUFWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAAAUFWK" varchar(8) NOT NULL default '00000000',
  "AAABMLPV" varchar(64) NOT NULL default ' ',
  "AAAAUFTQ" varchar(4) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAAAUFTS" varchar(1) NOT NULL default '0',
  "AAAAUFTT" varchar(1) NOT NULL default '0',
  "AAABICEF" varchar(3) NOT NULL default '000',
  "AAAAUFVE" decimal(5,2) NOT NULL default '0.00',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAAAUFVG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHMSE","AAAAUFWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABBLCZ" varchar(10) NOT NULL default ' ',
  "AAABLTYY" varchar(32) NOT NULL default ' ',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAAAUFXI" varchar(2) NOT NULL default ' ',
  "AAABLUAV" varchar(40) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAABFSDE" varchar(10) NOT NULL default ' ',
  "AAABLECQ" varchar(64) NOT NULL default ' ',
  "AAAAUFUQ" varchar(1) NOT NULL default ' ',
  "AAAAUFXP" varchar(1) NOT NULL default ' ',
  "AAAAWOKG" varchar(1) NOT NULL default ' ',
  "AAAAUFUT" varchar(20) NOT NULL default ' ',
  "AAAAUFUU" varchar(32) NOT NULL default ' ',
  "AAAAUFUV" varchar(10) NOT NULL default ' ',
  "AAAAUFUW" varchar(1) NOT NULL default '0',
  "AAAAUFUX" varchar(6) NOT NULL default '000000',
  "AAABICEF" varchar(3) NOT NULL default '000',
  "AAAAUFUZ" varchar(1) NOT NULL default '0',
  "AAAAUFVA" varchar(1) NOT NULL default '0',
  "AAAAUFVB" varchar(2) NOT NULL default ' ',
  "AAAAUFVC" varchar(2) NOT NULL default ' ',
  "AAABAVBO" varchar(1) NOT NULL default ' ',
  "AAAAUFVE" decimal(5,2) NOT NULL default '0.00',
  "AAABADPF" varchar(4) NOT NULL default '0000',
  "AAAAUFVG" varchar(1) NOT NULL default '0',
  "AAAAUFVH" varchar(1) NOT NULL default '0',
  "AAAAUFVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHMSE"),
  UNIQUE KEY "AAAAUFVL" ("AAABMRIL","AAABLOZE","AAABLTVR","AAABLTYY","AAAAUFWZ","AAAAUFXA","AAAAUFXB","AAAAUFXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHRZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFVX" smallint(6) NOT NULL default '0',
  "AAABMOQQ" text,
  PRIMARY KEY ("AAABMRIL","AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAAAUFWK" varchar(8) NOT NULL default '00000000',
  "AAAAUFWL" varchar(8) NOT NULL default '00000000',
  "AAABBKHW" varchar(2) NOT NULL default ' ',
  "AAABIMHK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHMSE","AAAAUFWK","AAAAUFWL","AAABBKHW","AAABIMHK"),
  KEY "AAAAUFWO" ("AAABHMSE","AAABBKHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAAUFXI" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAUFWZ" varchar(10) NOT NULL default ' ',
  "AAAAUFXA" varchar(12) NOT NULL default ' ',
  "AAAAUFXB" varchar(3) NOT NULL default ' ',
  "AAABMDXM" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGAW","AAABLOZE","AAABLTVR","AAAAUFXI","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAAAUFYQ" varchar(12) NOT NULL default ' ',
  "AAAAXBOG" varchar(40) NOT NULL default ' ',
  "AAAAUFXP" varchar(1) NOT NULL default ' ',
  "AAABFEIS" varchar(1) NOT NULL default '0',
  "AAAAUFXR" varchar(1) NOT NULL default '0',
  "AAABCOLW" varchar(2) NOT NULL default '00',
  "AAAAUFXT" decimal(5,2) NOT NULL default '0.00',
  "AAAAUFXU" decimal(5,2) NOT NULL default '0.00',
  "AAAAUFXV" decimal(5,2) NOT NULL default '0.00',
  "AAAAUFXW" decimal(5,2) NOT NULL default '0.00',
  "AAAAUFXX" varchar(1) NOT NULL default ' ',
  "AAAAUFXY" varchar(1) NOT NULL default ' ',
  "AAAAUFXZ" varchar(1) NOT NULL default ' ',
  "AAAAUFYA" varchar(1) NOT NULL default ' ',
  "AAAAUFYB" varchar(1) NOT NULL default ' ',
  "AAAAUFYC" varchar(1) NOT NULL default ' ',
  "AAABIFRP" varchar(1) NOT NULL default ' ',
  "AAAAUFYE" varchar(1) NOT NULL default ' ',
  "AAAAUFYF" varchar(1) NOT NULL default ' ',
  "AAABMKGE" varchar(1) NOT NULL default ' ',
  "AAAAUFYH" varchar(50) NOT NULL default ' ',
  "AAAAUFYI" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUFYQ" varchar(12) NOT NULL default ' ',
  "AAAAXBOG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUFYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  "AAABANBG" varchar(8) NOT NULL default '00000000',
  "AAAAUFZA" varchar(40) NOT NULL default ' ',
  "AAABLAQG" varchar(4) NOT NULL default '0000',
  "AAAAUFZC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGAW","AAABLHPM","AAABLUNJ","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABDFFC" varchar(45) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUFZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUVS" varchar(20) NOT NULL default ' ',
  "AAAAUFZV" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(8) NOT NULL default '00000000',
  "AAABHMNR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABAMNN" varchar(8) NOT NULL default '00000000',
  "AAAAUGAU" varchar(1) NOT NULL default ' ',
  "AAABHMSE" varchar(8) NOT NULL default '00000000',
  "AAAAUGAW" varchar(8) NOT NULL default '00000000',
  "AAAAUGAO" varchar(1) NOT NULL default ' ',
  "AAABILEY" varchar(1) NOT NULL default ' ',
  "AAAAUGAQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABAMNN","AAAAUGAU","AAABHMSE","AAAAUGAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGAX" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVCGL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAVCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGBE" (
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
  PRIMARY KEY ("AAABLLLR","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGBR" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLLLR","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGCD" (
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
  KEY "AAAAUGCR" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUGCV" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGCX" (
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
CREATE TABLE "AAAAUGDQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGDX" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUGEH" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGEL" (
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
  UNIQUE KEY "AAAAUGEY" ("AAABLLNL"),
  KEY "AAAAUGFA" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGFC" (
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
  UNIQUE KEY "AAAAUGFP" ("AAABLLNL"),
  KEY "AAAAUGFR" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGFT" (
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
  PRIMARY KEY ("AAABLLLR","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGGH" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUGGX" ("AAABLLNL"),
  KEY "AAAAUGGZ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGHB" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLLLR","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGIC" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLLR","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUGIO" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGIS" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUGJH" ("AAABLLNL"),
  KEY "AAAAUGJJ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGJL" (
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABLLLR","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAUGKA" ("AAABLLNL"),
  KEY "AAAAUGKC" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGKE" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAAUGKU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER","AAAAUGKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGLC" (
  "AAAAUGKU" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  "AAAAUGKM" varchar(35) NOT NULL default ' ',
  "AAABLUEL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUGKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGKP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUGKU" varchar(30) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUGKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGKV" (
  "AAAAVGEC" varchar(14) NOT NULL default ' ',
  "AAABIVVF" varchar(10) NOT NULL default '0000000000',
  "AAAAUGLC" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVGEC","AAABIVVF","AAAAUGLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGLD" (
  "AAAAUGMG" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABIASL" varchar(32) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUGMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGLS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUGMG" varchar(32) NOT NULL default ' ',
  "AAAAUGLV" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUGMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGLY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAUGMG" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAUGMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGMH" (
  "AAABGDGH" varchar(32) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAAAUXDW" varchar(24) NOT NULL default ' ',
  "AAAAWCEI" varchar(132) NOT NULL default ' ',
  "AAAAWCEJ" varchar(132) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABGDGH"),
  KEY "AAAAUGMY" ("AAAAUXDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGNA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAABGDGH" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAABGDGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGNJ" (
  "AAAAUGNM" varchar(2) NOT NULL default ' ',
  "AAABLDNI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAAUGNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGNN" (
  "AAAAUGQG" varchar(32) NOT NULL default ' ',
  "AAAAUXDW" varchar(20) NOT NULL default ' ',
  "AAAAUGNQ" varchar(1) NOT NULL default ' ',
  "AAAAUGNR" varchar(100) NOT NULL default ' ',
  "AAAAVCBQ" varchar(100) NOT NULL default ' ',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  "AAAAUGPC" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUGQG"),
  KEY "AAAAUGOH" ("AAAAUXDW"),
  KEY "AAAAUGOJ" ("AAAAUGQG","AAAAUGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGOM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAUGQG" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAUGQG"),
  KEY "AAAAUGOV" ("AAAAUQRO","AAAAUGQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGOY" (
  "AAAAUGQG" varchar(32) NOT NULL default ' ',
  "AAAAUXDW" varchar(20) NOT NULL default ' ',
  "AAABGDGH" varchar(32) NOT NULL default ' ',
  "AAAAUGPC" varchar(1) NOT NULL default ' ',
  "AAAAUGPD" int(11) NOT NULL default '0',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUGQG"),
  KEY "AAAAUGPQ" ("AAAAUXDW"),
  KEY "AAAAUGPS" ("AAAAUGQG","AAAAUGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGPV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAUGQG" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAUGQG"),
  KEY "AAAAUGQE" ("AAAAUQRO","AAAAUGQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGQH" (
  "AAAAUGRC" varchar(32) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGQU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAUGRC" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGRD" (
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAAAUGRF" varchar(30) NOT NULL default ' ',
  "AAAAUGRG" varchar(30) NOT NULL default ' ',
  "AAAAUGRH" varchar(30) NOT NULL default ' ',
  "AAAAUGRI" varchar(30) NOT NULL default ' ',
  "AAAAUGRJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGRL" (
  "AAAAUGTL" varchar(32) NOT NULL default ' ',
  "AAAAUGRN" varchar(1) NOT NULL default ' ',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(2) NOT NULL default ' ',
  "AAAAUGRQ" VARBINARY(255),
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAUGRS" varchar(1) NOT NULL default ' ',
  "AAAAUGRT" varchar(1) NOT NULL default ' ',
  "AAAAUGRU" varchar(1) NOT NULL default ' ',
  "AAAAUGRV" varchar(1) NOT NULL default ' ',
  "AAAAUGRW" varchar(1) NOT NULL default ' ',
  "AAAAUGRX" varchar(1) NOT NULL default ' ',
  "AAABIASL" varchar(32) NOT NULL default ' ',
  "AAAAUGRZ" varchar(32) NOT NULL default ' ',
  "AAAAUGSA" varchar(32) NOT NULL default ' ',
  "AAAAUGSB" varchar(32) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAAAUGSI" varchar(4) NOT NULL default ' ',
  "AAAAUGSJ" varchar(4) NOT NULL default ' ',
  "AAAAUGSP" varchar(1) NOT NULL default ' ',
  "AAABLXDT" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAUGTL"),
  KEY "AAAAUGSN" ("AAAAUGTL","AAAAUGSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGSQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUGTL" varchar(32) NOT NULL default ' ',
  "AAAAUGSX" varchar(1) NOT NULL default ' ',
  "AAAAUGSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUGTL","AAAAUGSX"),
  KEY "AAAAUGSY" ("AAAAUGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGTA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUQRO" varchar(15) NOT NULL default ' ',
  "AAAAUGTL" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLFWP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAUQRO","AAAAUGTL"),
  KEY "AAAAUGTJ" ("AAAAUQRO","AAAAUGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGTM" (
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
  KEY "AAAAUGVC" ("AAABJAAI"),
  KEY "AAAAUGVE" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAUGVH" ("AAABLFYJ","AAABHZRB"),
  KEY "AAAAUGVK" ("AAABHZOJ"),
  KEY "AAAAUGVM" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGVP" (
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
  KEY "AAAAUGWL" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGWO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAAAUGXC" ("AAABMRHK","AAABHZRB"),
  KEY "AAAAUGXF" ("AAABMRHS","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGXJ" (
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGXY" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLQA","AAABLLLR","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGYM" (
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
  KEY "AAAAUGZA" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAUGZE" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUGZG" (
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
CREATE TABLE "AAAAUGZZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHAG" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHAQ" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHAU" (
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
  UNIQUE KEY "AAAAUHBH" ("AAABLLNL"),
  KEY "AAAAUHBJ" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHBL" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHBY" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHCC" (
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
  UNIQUE KEY "AAAAUHCP" ("AAABLLNL"),
  KEY "AAAAUHCR" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHCT" (
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHDG" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHDK" (
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHEA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUHES" ("AAABLLNL"),
  KEY "AAAAUHEU" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHEW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHFN" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHFR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(4) NOT NULL default '0000',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABHZJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHGT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNI","AAABLLNN"),
  UNIQUE KEY "AAAAUHHL" ("AAABLLNL"),
  KEY "AAAAUHHN" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHHP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNI","AAABLLNN","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHII" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHIM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHJA" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHJE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABLLNL"),
  UNIQUE KEY "AAAAUHJV" ("AAABLLNL"),
  KEY "AAAAUHJX" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHJZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHKQ" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHKU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNI","AAABLLNL"),
  UNIQUE KEY "AAAAUHLL" ("AAABLLNL"),
  KEY "AAAAUHLN" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHLP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR","AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAUHMG" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLEUA" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAUHMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABFJTV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAUJXQ" varchar(3) NOT NULL default '000',
  "AAAAUJXR" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLHZO" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(1) NOT NULL default ' ',
  "AAAAUHOP" varchar(8) NOT NULL default ' ',
  "AAAAUHOQ" varchar(8) NOT NULL default '00000000',
  "AAAAUHOR" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAAAUHOW" varchar(8) NOT NULL default '00000000',
  "AAABDJMM" varchar(4) NOT NULL default ' ',
  "AAABCOIG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHPB" (
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEHK","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHPH" (
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABJZWV" varchar(4) NOT NULL default ' ',
  "AAAAVYMX" varchar(4) NOT NULL default ' ',
  "AAAAVYMY" varchar(2) NOT NULL default ' ',
  "AAAAVYMZ" varchar(2) NOT NULL default ' ',
  "AAAAVYNA" varchar(6) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAAAVYNC" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABIWLB" varchar(2) NOT NULL default ' ',
  "AAAAVYNF" varchar(10) NOT NULL default ' ',
  "AAAAVYNG" varchar(10) NOT NULL default ' ',
  "AAAAVYNH" varchar(10) NOT NULL default ' ',
  "AAAAVYNI" varchar(10) NOT NULL default ' ',
  "AAAAVYNJ" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(35) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAVYNM" varchar(10) NOT NULL default ' ',
  "AAAAVYNN" varchar(4) NOT NULL default ' ',
  "AAAAVYNO" varchar(4) NOT NULL default ' ',
  "AAAAVYNP" varchar(4) NOT NULL default ' ',
  "AAAAVYNQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFG","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHQJ" (
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAAAUHRO" varchar(3) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAUHQS" varchar(40) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVS","AAAAUHRO","AAABMRHK","AAAAUHQS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHQU" (
  "AAABHUYZ" varchar(2) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHUYZ","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHRA" (
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAABIWFD" varchar(1) NOT NULL default ' ',
  "AAABJQWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYVH","AAABIWFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHRG" (
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAAAUHRO" varchar(3) NOT NULL default ' ',
  "AAABMRHK" varchar(12) NOT NULL default ' ',
  "AAAAUHRK" varchar(8) NOT NULL default '00000000',
  "AAAAUHRL" varchar(6) NOT NULL default '000000',
  "AAAAUHRM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUVS","AAAAUHRO","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAUHRQ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABJTDU" varchar(3) NOT NULL default ' ',
  "AAAAVCGL" varchar(8) NOT NULL default '00000000',
  "AAAAUHRU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABJTDU","AAAAVCGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
