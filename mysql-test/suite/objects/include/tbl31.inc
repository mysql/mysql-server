# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl31.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABAULZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUNB" varchar(3) NOT NULL default '000',
  "AAABAUNC" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUNB","AAABAUNC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(3) NOT NULL default ' ',
  "AAABAUNB" varchar(3) NOT NULL default '000',
  "AAABAUNC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGNHE","AAABAUNB","AAABAUNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(3) NOT NULL default ' ',
  "AAABAUNB" varchar(3) NOT NULL default '000',
  "AAABAUNC" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHE","AAABAUNB","AAABAUNC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUNE" (
  "AAABAUNI" varchar(4) NOT NULL default ' ',
  "AAABAUNJ" varchar(8) NOT NULL default ' ',
  "AAABAUNH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABAUNI","AAABAUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUQY" varchar(5) NOT NULL default ' ',
  "AAABAUQL" varchar(40) NOT NULL default ' ',
  "AAABAUQM" varchar(16) NOT NULL default ' ',
  "AAABGCOK" varchar(1) NOT NULL default ' ',
  "AAABGCVF" varchar(40) NOT NULL default ' ',
  "AAABAUQP" varchar(16) NOT NULL default ' ',
  "AAABGCOM" varchar(1) NOT NULL default ' ',
  "AAABGCVI" varchar(40) NOT NULL default ' ',
  "AAABAUQS" varchar(16) NOT NULL default ' ',
  "AAABGCOO" varchar(1) NOT NULL default ' ',
  "AAABGCVL" varchar(40) NOT NULL default ' ',
  "AAABAUQV" varchar(16) NOT NULL default ' ',
  "AAABGCOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABAUOQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVBO" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVBV","AAABAVBW","AAABAVBX","AAABAUOQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(3) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAABAUPF" varchar(1) NOT NULL default ' ',
  "AAABAUPG" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABAVBV","AAABAVBW","AAABAVBX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(3) NOT NULL default ' ',
  "AAABAUQY" varchar(5) NOT NULL default ' ',
  "AAABAUQL" varchar(40) NOT NULL default ' ',
  "AAABAUQM" varchar(16) NOT NULL default ' ',
  "AAABGCOK" varchar(1) NOT NULL default ' ',
  "AAABGCVF" varchar(40) NOT NULL default ' ',
  "AAABAUQP" varchar(16) NOT NULL default ' ',
  "AAABGCOM" varchar(1) NOT NULL default ' ',
  "AAABGCVI" varchar(40) NOT NULL default ' ',
  "AAABAUQS" varchar(16) NOT NULL default ' ',
  "AAABGCOO" varchar(1) NOT NULL default ' ',
  "AAABGCVL" varchar(40) NOT NULL default ' ',
  "AAABAUQV" varchar(16) NOT NULL default ' ',
  "AAABGCOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG","AAABAUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUQY" varchar(5) NOT NULL default ' ',
  "AAABAUQL" varchar(40) NOT NULL default ' ',
  "AAABAUQM" varchar(16) NOT NULL default ' ',
  "AAABGCOK" varchar(1) NOT NULL default ' ',
  "AAABGCVF" varchar(40) NOT NULL default ' ',
  "AAABAUQP" varchar(16) NOT NULL default ' ',
  "AAABGCOM" varchar(1) NOT NULL default ' ',
  "AAABGCVI" varchar(40) NOT NULL default ' ',
  "AAABAUQS" varchar(16) NOT NULL default ' ',
  "AAABGCOO" varchar(1) NOT NULL default ' ',
  "AAABGCVL" varchar(40) NOT NULL default ' ',
  "AAABAUQV" varchar(16) NOT NULL default ' ',
  "AAABGCOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAVBV","AAABAVBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAURG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVBV","AAABAVBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAURQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABAUSB" varchar(10) NOT NULL default ' ',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABAUSB","AAABAVBU","AAABKFCZ"),
  KEY "AAABAUSE" ("AAABAVBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABAUSJ" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(3) NOT NULL default ' ',
  "AAABAUSZ" varchar(128) NOT NULL default ' ',
  "AAABAUTA" varchar(1) NOT NULL default ' ',
  "AAABAUTB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABAUZL" varchar(46) NOT NULL default ' ',
  "AAABAUZN" varchar(5) NOT NULL default ' ',
  "AAABAUZO" varchar(6) NOT NULL default ' ',
  "AAABAUZP" varchar(6) NOT NULL default ' ',
  "AAABAUZQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUUO" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABAUZL" varchar(46) NOT NULL default ' ',
  "AAABAUZM" varchar(46) NOT NULL default ' ',
  "AAABAUZN" varchar(5) NOT NULL default ' ',
  "AAABAUZO" varchar(6) NOT NULL default ' ',
  "AAABAUZP" varchar(6) NOT NULL default ' ',
  "AAABAUZQ" varchar(8) NOT NULL default ' ',
  "AAABAUZR" varchar(20) NOT NULL default ' ',
  "AAABAUZS" varchar(20) NOT NULL default ' ',
  "AAABAUZT" varchar(20) NOT NULL default ' ',
  "AAABAUZU" varchar(20) NOT NULL default ' ',
  "AAABAUZV" varchar(20) NOT NULL default ' ',
  "AAABAUZW" varchar(20) NOT NULL default ' ',
  "AAABAUZX" varchar(20) NOT NULL default ' ',
  "AAABAUZY" varchar(20) NOT NULL default ' ',
  "AAABAUZZ" varchar(20) NOT NULL default ' ',
  "AAABAVAA" varchar(20) NOT NULL default ' ',
  "AAABAVAB" varchar(20) NOT NULL default ' ',
  "AAABAVAC" varchar(20) NOT NULL default ' ',
  "AAABAVAD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUUO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUVI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAUVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUVI" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAUVI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAUVI" varchar(3) NOT NULL default '000',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABAUVI","AAABAVBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVBQ" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABAUXM" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABAUXK" varchar(3) NOT NULL default '000',
  "AAABAUWT" varchar(8) NOT NULL default '00000000',
  "AAABAUWU" varchar(8) NOT NULL default '00000000',
  "AAABAVBO" varchar(2) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABAUXD" varchar(1) NOT NULL default ' ',
  "AAABAUXE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAVBU","AAABAUXM","AAABAVBW","AAABAVBX","AAABAUXK"),
  KEY "AAABAUXL" ("AAABAUXM","AAABAVBW","AAABAVBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABAUXT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(3) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABAVBT" varchar(30) NOT NULL default ' ',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABAUZA" varchar(3) NOT NULL default '000',
  "AAABAUYM" varchar(1) NOT NULL default ' ',
  "AAABAVBG" varchar(75) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVBO" varchar(2) NOT NULL default ' ',
  "AAABAVBZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABAVBT","AAABAVBU","AAABAVBV","AAABAVBW","AAABAVBX","AAABAUZA"),
  KEY "AAABAUZB" ("AAABAVBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABAVBT" varchar(30) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABAVBZ" varchar(10) NOT NULL default '0000000000',
  "AAABAVBQ" varchar(2) NOT NULL default ' ',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABAUZL" varchar(46) NOT NULL default ' ',
  "AAABAUZM" varchar(46) NOT NULL default ' ',
  "AAABAUZN" varchar(5) NOT NULL default ' ',
  "AAABAUZO" varchar(6) NOT NULL default ' ',
  "AAABAUZP" varchar(6) NOT NULL default ' ',
  "AAABAUZQ" varchar(8) NOT NULL default ' ',
  "AAABAUZR" varchar(20) NOT NULL default ' ',
  "AAABAUZS" varchar(20) NOT NULL default ' ',
  "AAABAUZT" varchar(20) NOT NULL default ' ',
  "AAABAUZU" varchar(20) NOT NULL default ' ',
  "AAABAUZV" varchar(20) NOT NULL default ' ',
  "AAABAUZW" varchar(20) NOT NULL default ' ',
  "AAABAUZX" varchar(20) NOT NULL default ' ',
  "AAABAUZY" varchar(20) NOT NULL default ' ',
  "AAABAUZZ" varchar(20) NOT NULL default ' ',
  "AAABAVAA" varchar(20) NOT NULL default ' ',
  "AAABAVAB" varchar(20) NOT NULL default ' ',
  "AAABAVAC" varchar(20) NOT NULL default ' ',
  "AAABAVAD" varchar(1) NOT NULL default ' ',
  "AAABAVAE" varchar(50) NOT NULL default ' ',
  "AAABAVAF" varchar(50) NOT NULL default ' ',
  "AAABAVAG" varchar(50) NOT NULL default ' ',
  "AAABAVAH" varchar(50) NOT NULL default ' ',
  "AAABAVAI" varchar(50) NOT NULL default ' ',
  "AAABAVAJ" varchar(50) NOT NULL default ' ',
  "AAABAVAK" varchar(50) NOT NULL default ' ',
  "AAABAVAL" varchar(50) NOT NULL default ' ',
  "AAABAVAM" varchar(50) NOT NULL default ' ',
  "AAABAVAN" varchar(50) NOT NULL default ' ',
  "AAABAVAO" varchar(50) NOT NULL default ' ',
  "AAABAVAP" varchar(50) NOT NULL default ' ',
  "AAABAVAQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABAVBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABAVBT" varchar(30) NOT NULL default ' ',
  "AAABAVBU" varchar(6) NOT NULL default '000000',
  "AAABAVBV" varchar(3) NOT NULL default '000',
  "AAABAVBW" varchar(2) NOT NULL default '00',
  "AAABAVBX" varchar(5) NOT NULL default '00000',
  "AAABAVBC" varchar(1) NOT NULL default ' ',
  "AAABAVBD" varchar(1) NOT NULL default ' ',
  "AAABAVBE" varchar(1) NOT NULL default ' ',
  "AAABAVBF" varchar(1) NOT NULL default ' ',
  "AAABAVBG" varchar(90) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABAVBI" varchar(20) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABAVBK" varchar(1) NOT NULL default ' ',
  "AAABAVBL" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVBO" varchar(2) NOT NULL default ' ',
  "AAABAVBZ" varchar(10) NOT NULL default '0000000000',
  "AAABAVBQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABAVBT","AAABAVBU","AAABAVBV","AAABAVBW","AAABAVBX"),
  KEY "AAABAVBY" ("AAABAVBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVCN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAVCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVCN" varchar(2) NOT NULL default '00',
  "AAABAVCJ" varchar(10) NOT NULL default ' ',
  "AAABAVCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVDA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVDA" varchar(1) NOT NULL default ' ',
  "AAABAVCX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVTA" varchar(12) NOT NULL default '000000000000',
  "AAABAVTB" varchar(12) NOT NULL default ' ',
  "AAABAVDF" varchar(10) NOT NULL default ' ',
  "AAABAVDG" varchar(35) NOT NULL default ' ',
  "AAABAVDH" varchar(10) NOT NULL default ' ',
  "AAABAVDI" varchar(18) NOT NULL default ' ',
  "AAABLUYZ" varchar(8) NOT NULL default '00000000',
  "AAABLVAN" varchar(8) NOT NULL default '00000000',
  "AAABAVYS" varchar(8) NOT NULL default ' ',
  "AAABAVDM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVTA","AAABAVTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVFD" varchar(10) NOT NULL default ' ',
  "AAABAVRQ" varchar(1) NOT NULL default ' ',
  "AAABAVQU" varchar(1) NOT NULL default ' ',
  "AAABAVPF" varchar(1) NOT NULL default ' ',
  "AAABAVDY" varchar(1) NOT NULL default ' ',
  "AAABAVFG" varchar(8) NOT NULL default '00000000',
  "AAABAVEA" varchar(8) NOT NULL default '00000000',
  "AAABAVEB" varchar(8) NOT NULL default '00000000',
  "AAABAVEC" varchar(3) NOT NULL default ' ',
  "AAABAVED" varchar(1) NOT NULL default ' ',
  "AAABAVEE" varchar(1) NOT NULL default ' ',
  "AAABAVOR" varchar(8) NOT NULL default '00000000',
  "AAABAVEG" varchar(1) NOT NULL default ' ',
  "AAABAVPP" varchar(10) NOT NULL default ' ',
  "AAABAVNW" varchar(1) NOT NULL default ' ',
  "AAABAVEJ" decimal(5,0) NOT NULL default '0',
  "AAABAVEK" decimal(15,2) NOT NULL default '0.00',
  "AAABAVEL" varchar(5) NOT NULL default ' ',
  "AAABAVEM" decimal(5,0) NOT NULL default '0',
  "AAABAVEN" decimal(15,2) NOT NULL default '0.00',
  "AAABAVEO" varchar(5) NOT NULL default ' ',
  "AAABAVEP" decimal(5,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVES" varchar(10) NOT NULL default ' ',
  "AAABAVET" varchar(20) NOT NULL default '00000000000000000000',
  "AAABAVEU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLKNN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC"),
  KEY "AAABAVEY" ("AAABMRIL","AAABMCRV"),
  KEY "AAABAVFB" ("AAABMRIL","AAABAVFD"),
  KEY "AAABAVFE" ("AAABMRIL","AAABAVFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVHG" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVHG" varchar(10) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAVFV" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVHG","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVGQ" varchar(6) NOT NULL default '000000',
  "AAABAVHL" varchar(2) NOT NULL default ' ',
  "AAABAVHM" varchar(6) NOT NULL default ' ',
  "AAABAVHN" varchar(1) NOT NULL default ' ',
  "AAABAVGH" varchar(8) NOT NULL default '00000000',
  "AAABAVGI" varchar(2) NOT NULL default ' ',
  "AAABAVGJ" varchar(10) NOT NULL default ' ',
  "AAABAVNW" varchar(1) NOT NULL default ' ',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVGQ"),
  KEY "AAABAVGR" ("AAABAVRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVHG" varchar(10) NOT NULL default ' ',
  "AAABAVGW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABAVHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVHG" varchar(10) NOT NULL default ' ',
  "AAABAVHD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVIF" varchar(3) NOT NULL default '000',
  "AAABAVHL" varchar(2) NOT NULL default ' ',
  "AAABAVHM" varchar(6) NOT NULL default ' ',
  "AAABAVHN" varchar(1) NOT NULL default ' ',
  "AAABAVHO" varchar(8) NOT NULL default '00000000',
  "AAABAVHP" varchar(8) NOT NULL default '00000000',
  "AAABAVHQ" varchar(8) NOT NULL default '00000000',
  "AAABAVHR" varchar(1) NOT NULL default ' ',
  "AAABAVHS" varchar(1) NOT NULL default ' ',
  "AAABAVHT" varchar(2) NOT NULL default ' ',
  "AAABAVHU" varchar(2) NOT NULL default ' ',
  "AAABAVHV" varchar(2) NOT NULL default ' ',
  "AAABAVHW" varchar(8) NOT NULL default ' ',
  "AAABAVHX" varchar(10) NOT NULL default ' ',
  "AAABAVNW" varchar(1) NOT NULL default ' ',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVIC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVIF"),
  KEY "AAABAVIG" ("AAABAVRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVJH" varchar(17) NOT NULL default ' ',
  "AAABAVJI" varchar(4) NOT NULL default '0000',
  "AAABAVIO" varchar(8) NOT NULL default '00000000',
  "AAABAVIP" varchar(6) NOT NULL default '000000',
  "AAABAVIQ" varchar(1) NOT NULL default ' ',
  "AAABAVIR" varchar(1) NOT NULL default ' ',
  "AAABAVIS" varchar(1) NOT NULL default ' ',
  "AAABAVIT" varchar(8) NOT NULL default '00000000',
  "AAABAVIU" varchar(10) NOT NULL default ' ',
  "AAABAVIV" varchar(10) NOT NULL default ' ',
  "AAABAVIW" varchar(20) NOT NULL default ' ',
  "AAABAVIX" varchar(20) NOT NULL default ' ',
  "AAABAVIY" varchar(1) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVJD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWB","AAABAVJH","AAABAVJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVJV" varchar(3) NOT NULL default ' ',
  "AAABAVJS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVKL" varchar(3) NOT NULL default ' ',
  "AAABAVKM" varchar(8) NOT NULL default '00000000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVKC" varchar(8) NOT NULL default '00000000',
  "AAABAVKD" varchar(132) NOT NULL default ' ',
  "AAABAVKE" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVKH" varchar(1) NOT NULL default ' ',
  "AAABAVKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAVKL","AAABAVKM"),
  KEY "AAABAVKN" ("AAABAVRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVMR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVMR" varchar(6) NOT NULL default ' ',
  "AAABAVMS" varchar(6) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVMR","AAABAVMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVMR" varchar(6) NOT NULL default ' ',
  "AAABAVMS" varchar(6) NOT NULL default ' ',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAVLL" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVMR","AAABAVMS","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVMS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVMS" varchar(6) NOT NULL default ' ',
  "AAABAVMA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVMR" varchar(6) NOT NULL default ' ',
  "AAABAVMI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVMR" varchar(6) NOT NULL default ' ',
  "AAABAVMS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVMR","AAABAVMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVNF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVNF" varchar(3) NOT NULL default ' ',
  "AAABAVNC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVNL" varchar(20) NOT NULL default ' ',
  "AAABAVWY" varchar(3) NOT NULL default ' ',
  "AAABAVNN" varchar(3) NOT NULL default ' ',
  "AAABAVNO" varchar(8) NOT NULL default '00000000',
  "AAABAVNP" varchar(1) NOT NULL default ' ',
  "AAABAVNQ" varchar(8) NOT NULL default '00000000',
  "AAABAVNR" varchar(3) NOT NULL default ' ',
  "AAABAVNS" varchar(8) NOT NULL default '00000000',
  "AAABAVNT" varchar(8) NOT NULL default '00000000',
  "AAABAVNU" varchar(8) NOT NULL default '00000000',
  "AAABAVPP" varchar(10) NOT NULL default ' ',
  "AAABAVNW" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABAVNZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVWW","AAABAVWX"),
  KEY "AAABAVOE" ("AAABMRIL","AAABAVWW","AAABAVWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVOR" varchar(8) NOT NULL default '00000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAVON" varchar(75) NOT NULL default ' ',
  "AAABAVOO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVOR","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVPF" varchar(1) NOT NULL default ' ',
  "AAABAVPC" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVPV" varchar(4) NOT NULL default '0000',
  "AAABAVPK" varchar(8) NOT NULL default '00000000',
  "AAABAVPL" varchar(6) NOT NULL default '000000',
  "AAABAVPM" decimal(5,0) NOT NULL default '0',
  "AAABAVPN" varchar(1) NOT NULL default ' ',
  "AAABAVPO" varchar(1) NOT NULL default ' ',
  "AAABAVPP" varchar(10) NOT NULL default ' ',
  "AAABAVPQ" varchar(132) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVQH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVQH" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVQF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVQH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVQU" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVQS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVQU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRC" varchar(12) NOT NULL default '000000000000',
  "AAABAVRD" varchar(6) NOT NULL default '000000',
  "AAABAVRA" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRC","AAABAVRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVRQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVRQ" varchar(1) NOT NULL default ' ',
  "AAABAVRN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVSD" varchar(1) NOT NULL default ' ',
  "AAABAVSA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVTA" varchar(12) NOT NULL default '000000000000',
  "AAABAVTB" varchar(12) NOT NULL default ' ',
  "AAABAVTC" varchar(4) NOT NULL default ' ',
  "AAABAVTD" varchar(4) NOT NULL default ' ',
  "AAABAWQE" varchar(7) NOT NULL default ' ',
  "AAABAVYS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVTA","AAABAVTB","AAABAVTC","AAABAVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVTH" varchar(12) NOT NULL default ' ',
  "AAABAVTI" varchar(8) NOT NULL default '00000000',
  "AAABAVTJ" varchar(6) NOT NULL default '000000',
  "AAABJDMJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVTZ" varchar(1) NOT NULL default ' ',
  "AAABAVTW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESRL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESRL" varchar(2) NOT NULL default ' ',
  "AAABAVUJ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABESRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVUN" (
  "AAABIFOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIFOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVUQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIFOX" varchar(1) NOT NULL default ' ',
  "AAABAVUT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIFOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVVI" varchar(1) NOT NULL default ' ',
  "AAABAVVF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVVZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAVVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVVZ" varchar(2) NOT NULL default '00',
  "AAABAVVS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVWB" varchar(2) NOT NULL default '00',
  "AAABAVVZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAVWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVWC" (
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
  "AAABAVWS" decimal(15,0) NOT NULL default '0',
  "AAABAVWT" decimal(15,0) NOT NULL default '0',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAVXT" varchar(12) NOT NULL default '000000000000',
  "AAABAVWW" varchar(3) NOT NULL default ' ',
  "AAABAVWX" varchar(3) NOT NULL default ' ',
  "AAABAVWY" varchar(3) NOT NULL default ' ',
  "AAABAVXN" varchar(20) NOT NULL default ' ',
  "AAABAVXA" varchar(8) NOT NULL default '00000000',
  "AAABAVXB" varchar(1) NOT NULL default ' ',
  "AAABAVXC" varchar(50) NOT NULL default ' ',
  "AAABAVXD" varchar(20) NOT NULL default ' ',
  "AAABAVXE" varchar(75) NOT NULL default ' ',
  "AAABAVXF" varchar(10) NOT NULL default ' ',
  "AAABAVXG" varchar(1) NOT NULL default ' ',
  "AAABAVXH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAABAVXL" ("AAABMRIL","AAABAVXN"),
  KEY "AAABAVXO" ("AAABMRIL","AAABMCRV"),
  KEY "AAABAVXR" ("AAABMRIL","AAABAVXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(16) NOT NULL default ' ',
  "AAABAXKZ" varchar(2) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABAVYC" varchar(25) NOT NULL default ' ',
  "AAABAVYD" varchar(25) NOT NULL default ' ',
  "AAABAVYE" varchar(25) NOT NULL default ' ',
  "AAABAVYF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMRHS","AAABLESS","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABAVYS" varchar(8) NOT NULL default ' ',
  "AAABAVYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABAVYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(16) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLESS","AAABJLPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVZI" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAVZG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVZI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAVZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAVZY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABAWYE" varchar(11) NOT NULL default ' ',
  "AAABLTWK" varchar(20) NOT NULL default ' ',
  "AAABAVZS" varchar(1) NOT NULL default ' ',
  "AAABAVZT" varchar(1) NOT NULL default ' ',
  "AAABAWAT" varchar(2) NOT NULL default '00',
  "AAABAVZV" decimal(4,2) NOT NULL default '0.00',
  "AAABAWZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAVZY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWYF" varchar(2) NOT NULL default ' ',
  "AAABAWAD" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWYG" varchar(3) NOT NULL default ' ',
  "AAABAWAJ" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWAT" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWAR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWAT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWBB" varchar(3) NOT NULL default '000',
  "AAABAWAY" varchar(30) NOT NULL default ' ',
  "AAABAWAZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABAWBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWBK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABAWYE" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWBK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBK" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAXBH" varchar(10) NOT NULL default ' ',
  "AAABAXBI" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAXBK","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABAWKT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWCC","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWKT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWKU" varchar(13) NOT NULL default ' ',
  "AAABAWCK" varchar(13) NOT NULL default ' ',
  "AAABAWCL" varchar(1) NOT NULL default ' ',
  "AAABAWCM" varchar(1) NOT NULL default ' ',
  "AAABAWCN" decimal(4,2) NOT NULL default '0.00',
  "AAABAWCO" varchar(9) NOT NULL default '000000000',
  "AAABAWCP" varchar(1) NOT NULL default ' ',
  "AAABAWCQ" varchar(12) NOT NULL default '000000000000',
  "AAABAWCR" varchar(5) NOT NULL default ' ',
  "AAABAWCS" varchar(5) NOT NULL default ' ',
  "AAABAWCT" varchar(5) NOT NULL default ' ',
  "AAABAWCU" varchar(1) NOT NULL default '0',
  "AAABBHUU" varchar(2) NOT NULL default '00',
  "AAABAWCW" decimal(5,3) NOT NULL default '0.000',
  "AAABAWCX" decimal(5,3) NOT NULL default '0.000',
  "AAABAWCY" decimal(5,3) NOT NULL default '0.000',
  "AAABAWCZ" decimal(5,3) NOT NULL default '0.000',
  "AAABAWDA" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABAWKT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAWET" varchar(2) NOT NULL default '00',
  "AAABAWEU" varchar(10) NOT NULL default '0000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXQO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFHB","AAABAWET","AAABAWEU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAWET" varchar(2) NOT NULL default '00',
  "AAABAWEU" varchar(10) NOT NULL default '0000000000',
  "AAABAWEA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABJFHB","AAABAWET","AAABAWEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAWET" varchar(2) NOT NULL default '00',
  "AAABAWEU" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWEP" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFHB","AAABAWET","AAABAWEU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABAXDY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABAWFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABAWFZ" varchar(1) NOT NULL default ' ',
  "AAABAWGA" varchar(1) NOT NULL default ' ',
  "AAABAWGB" varchar(1) NOT NULL default ' ',
  "AAABAWGC" varchar(4) NOT NULL default '0000',
  "AAABAWGD" varchar(2) NOT NULL default '00',
  "AAABAWGE" varchar(4) NOT NULL default '0000',
  "AAABAWGF" varchar(2) NOT NULL default '00',
  "AAABAWGG" varchar(4) NOT NULL default '0000',
  "AAABAWGH" varchar(2) NOT NULL default '00',
  "AAABAWGI" varchar(4) NOT NULL default '0000',
  "AAABAWGJ" varchar(2) NOT NULL default '00',
  "AAABAWGK" varchar(4) NOT NULL default '0000',
  "AAABAWGL" varchar(2) NOT NULL default '00',
  "AAABAWGM" varchar(4) NOT NULL default '0000',
  "AAABAWGN" varchar(2) NOT NULL default '00',
  "AAABAWGO" varchar(4) NOT NULL default '0000',
  "AAABAWGP" varchar(2) NOT NULL default '00',
  "AAABAWGQ" varchar(4) NOT NULL default '0000',
  "AAABAWGR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABAXBU","AAABBGCX","AAABAWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABELZJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABELZJ" varchar(3) NOT NULL default '000',
  "AAABAZHF" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABBDCA" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABELZI" varchar(3) NOT NULL default '000',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABBDCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDCA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABMKXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABBDCA" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABMCWW","AAABBDCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJGBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWKT" varchar(4) NOT NULL default ' ',
  "AAABAWKU" varchar(13) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWKP" varchar(30) NOT NULL default ' ',
  "AAABAWKQ" varchar(4) NOT NULL default '0000',
  "AAABAWKR" varchar(13) NOT NULL default '0000000000000',
  PRIMARY KEY ("AAABMRIL","AAABAWKT","AAABAWKU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWLC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWMH" varchar(4) NOT NULL default ' ',
  "AAABAWMP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCIA" varchar(4) NOT NULL default ' ',
  "AAABHXLS" varchar(5) NOT NULL default ' ',
  "AAABAWMH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCIA","AAABHXLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWMP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAWMH" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAWMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAWMP" varchar(4) NOT NULL default ' ',
  "AAABGGHA" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAWMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWOG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBRWV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWOG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBRWV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWOG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAWOG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWOG" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWOG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWOG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAWOE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWOG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHVU" varchar(1) NOT NULL default ' ',
  "AAABKWRL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWPB" varchar(2) NOT NULL default '00',
  "AAABAWPC" varchar(2) NOT NULL default '00',
  "AAABAWOQ" decimal(5,2) NOT NULL default '0.00',
  "AAABAWOR" decimal(9,2) NOT NULL default '0.00',
  "AAABAWOS" decimal(9,3) NOT NULL default '0.000',
  "AAABBIBY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHVU","AAABKWRL","AAABKFCZ"),
  KEY "AAABAWOY" ("AAABMRIL","AAABKWRL","AAABAWPB","AAABAWPC","AAABKFCZ","AAABKFCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWQE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWQE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAWQE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWQE" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWQE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWQT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABIIZX" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABIIZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  "AAABAWRL" varchar(1) NOT NULL default ' ',
  "AAABLNYQ" varchar(2) NOT NULL default '00',
  "AAABIIZX" varchar(1) NOT NULL default ' ',
  "AAABAWVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABAWWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  "AAABAWRZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWH","AAABAWWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABHNOW" varchar(8) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABHNOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABHNOW" varchar(8) NOT NULL default ' ',
  "AAABAWSV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWH","AAABHNOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWUK" varchar(6) NOT NULL default ' ',
  "AAABAWTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABAWUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWUK" varchar(6) NOT NULL default ' ',
  "AAABAWTR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWH","AAABAWUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABHNOW" varchar(8) NOT NULL default ' ',
  "AAABAWUK" varchar(6) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABAWUE" varchar(4) NOT NULL default '0000',
  "AAABAWUF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABHNOW","AAABAWUK","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  "AAABAWWJ" varchar(1) NOT NULL default ' ',
  "AAABAWUS" varchar(3) NOT NULL default '000',
  "AAABAWUT" varchar(3) NOT NULL default '000',
  "AAABAWUU" varchar(3) NOT NULL default '000',
  "AAABAWUV" varchar(3) NOT NULL default '000',
  "AAABIIZX" varchar(1) NOT NULL default ' ',
  "AAABAWUX" varchar(1) NOT NULL default ' ',
  "AAABAWVN" varchar(1) NOT NULL default ' ',
  "AAABGNXB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABAWWI","AAABAWWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  "AAABAWWJ" varchar(1) NOT NULL default ' ',
  "AAABKMWI" varchar(3) NOT NULL default '000',
  "AAABIIZX" varchar(1) NOT NULL default ' ',
  "AAABAWVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABAWWI","AAABAWWJ","AAABKMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWH" varchar(6) NOT NULL default ' ',
  "AAABAWWI" varchar(4) NOT NULL default ' ',
  "AAABAWWJ" varchar(1) NOT NULL default ' ',
  "AAABKMWI" varchar(3) NOT NULL default '000',
  "AAABAWWL" varchar(3) NOT NULL default '000',
  "AAABHFNC" varchar(3) NOT NULL default '000',
  "AAABAWWD" varchar(3) NOT NULL default '000',
  "AAABAWWE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWH","AAABAWWI","AAABAWWJ","AAABKMWI","AAABAWWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAABAWWY" varchar(10) NOT NULL default ' ',
  "AAABAWWZ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWWU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWX","AAABAWWY","AAABAWWZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBA" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLLJG" decimal(7,2) NOT NULL default '0.00',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXBA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXBA" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABEPWA" varchar(11) NOT NULL default ' ',
  "AAABMLPV" varchar(52) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABAWYE" varchar(11) NOT NULL default ' ',
  "AAABAWYF" varchar(2) NOT NULL default ' ',
  "AAABAWYG" varchar(3) NOT NULL default ' ',
  "AAABIIAU" varchar(1) NOT NULL default ' ',
  "AAABAWYI" varchar(8) NOT NULL default '00000000',
  "AAABAWYJ" varchar(4) NOT NULL default ' ',
  "AAABAWYK" decimal(9,2) NOT NULL default '0.00',
  "AAABAWYL" decimal(9,2) NOT NULL default '0.00',
  "AAABKQMJ" varchar(8) NOT NULL default '00000000',
  "AAABAWYN" varchar(1) NOT NULL default ' ',
  "AAABAWYO" decimal(9,2) NOT NULL default '0.00',
  "AAABAWYP" decimal(9,2) NOT NULL default '0.00',
  "AAABAWYQ" varchar(8) NOT NULL default '00000000',
  "AAABAWYR" varchar(8) NOT NULL default '00000000',
  "AAABAWYS" varchar(1) NOT NULL default ' ',
  "AAABAWYT" varchar(11) NOT NULL default ' ',
  "AAABAWYU" varchar(2) NOT NULL default ' ',
  "AAABAWYV" varchar(3) NOT NULL default ' ',
  "AAABAWYW" varchar(2) NOT NULL default ' ',
  "AAABAWYX" varchar(8) NOT NULL default '00000000',
  "AAABAWYY" varchar(3) NOT NULL default ' ',
  "AAABAWYZ" varchar(8) NOT NULL default '00000000',
  "AAABAWZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABFSUI","AAABHKSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABAWZS" varchar(3) NOT NULL default '000',
  "AAABAWZL" varchar(1) NOT NULL default ' ',
  "AAABAWZM" varchar(8) NOT NULL default '00000000',
  "AAABAWZN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABFSUI","AAABHKSD","AAABAWZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAWZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBA" varchar(3) NOT NULL default '000',
  "AAABAXBB" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLLJG" decimal(7,2) NOT NULL default '0.00',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXBA","AAABAXBB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXBA" varchar(3) NOT NULL default '000',
  "AAABAXBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBAHO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXBA" varchar(3) NOT NULL default '000',
  "AAABAXBB" varchar(4) NOT NULL default '0000',
  "AAABAXAX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXBA","AAABAXBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBK" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXBH" varchar(10) NOT NULL default ' ',
  "AAABAXBI" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAXBK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABAXDP" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXBT" varchar(3) NOT NULL default ' ',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  "AAABAXBV" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAXBX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLIXT","AAABAXDP","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXDP" varchar(4) NOT NULL default ' ',
  "AAABAXCH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABAXCX" varchar(2) NOT NULL default '00',
  "AAABAXCY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABAXCX","AAABAXCY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLLTH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLLTH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXDP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXDY" varchar(2) NOT NULL default '00',
  "AAABAXDZ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIWAM" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAXDY","AAABAXDZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGCIA" varchar(4) NOT NULL default ' ',
  "AAABAXEN" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXEI" varchar(3) NOT NULL default '000',
  "AAABAXEJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABGCIA","AAABAXEN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXFB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXFB" varchar(2) NOT NULL default ' ',
  "AAABAXEY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXFO" varchar(2) NOT NULL default ' ',
  "AAABAXFL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXGF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXGF" varchar(2) NOT NULL default ' ',
  "AAABAXFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXID" varchar(2) NOT NULL default ' ',
  "AAABAXGF" varchar(2) NOT NULL default ' ',
  "AAABAXGG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAXID" varchar(2) NOT NULL default ' ',
  "AAABAXGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAXID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXIH" varchar(2) NOT NULL default ' ',
  "AAABAXIF" varchar(2) NOT NULL default ' ',
  "AAABAXHB" varchar(1) NOT NULL default ' ',
  "AAABAXGW" varchar(1) NOT NULL default ' ',
  "AAABAXGX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXIH","AAABAXIF","AAABAXHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXHO" varchar(2) NOT NULL default '00',
  "AAABAXHP" varchar(4) NOT NULL default '0000',
  "AAABAXHQ" varchar(2) NOT NULL default '00',
  "AAABAXHR" varchar(2) NOT NULL default ' ',
  "AAABAXHS" varchar(2) NOT NULL default '00',
  "AAABAXHT" varchar(2) NOT NULL default '00',
  "AAABAXHK" varchar(4) NOT NULL default '0000',
  "AAABAXHL" varchar(2) NOT NULL default '00',
  "AAABAXHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXHO","AAABAXHP","AAABAXHQ","AAABAXHR","AAABAXHS","AAABAXHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXIF" varchar(2) NOT NULL default ' ',
  "AAABAXIG" varchar(4) NOT NULL default ' ',
  "AAABAXIH" varchar(2) NOT NULL default ' ',
  "AAABAXII" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXIC" varchar(4) NOT NULL default ' ',
  "AAABAXID" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXIF","AAABAXIG","AAABAXIH","AAABAXII","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXIZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXIZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXIZ" varchar(4) NOT NULL default ' ',
  "AAABAXIW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXJG" varchar(2) NOT NULL default ' ',
  "AAABAXJD" decimal(4,2) NOT NULL default '0.00',
  "AAABAXJE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXKB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAXJO" varchar(8) NOT NULL default '00000000',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXKB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABAXLK" varchar(2) NOT NULL default ' ',
  "AAABCWSJ" varchar(2) NOT NULL default ' ',
  "AAABAXKB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXKT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXKT" varchar(2) NOT NULL default ' ',
  "AAABAXKQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXLK" varchar(2) NOT NULL default ' ',
  "AAABAXKX" varchar(30) NOT NULL default ' ',
  "AAABAXKY" varchar(15) NOT NULL default ' ',
  "AAABAXKZ" varchar(2) NOT NULL default ' ',
  "AAABAXLA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXLK" varchar(2) NOT NULL default ' ',
  "AAABBHOB" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXLK","AAABBHOB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYAU" varchar(2) NOT NULL default ' ',
  "AAABAXLV" varchar(1) NOT NULL default ' ',
  "AAABAXLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXWR" varchar(2) NOT NULL default ' ',
  "AAABAXMF" varchar(20) NOT NULL default ' ',
  "AAABAXMG" varchar(4) NOT NULL default ' ',
  "AAABAXMH" varchar(4) NOT NULL default ' ',
  "AAABAXMI" varchar(4) NOT NULL default ' ',
  "AAABAXMJ" varchar(4) NOT NULL default ' ',
  "AAABAXMK" varchar(4) NOT NULL default ' ',
  "AAABAXXB" varchar(2) NOT NULL default ' ',
  "AAABEPJZ" varchar(4) NOT NULL default ' ',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAABAXWW" varchar(1) NOT NULL default ' ',
  "AAABAYAU" varchar(2) NOT NULL default ' ',
  "AAABAXMQ" varchar(1) NOT NULL default ' ',
  "AAABAXMR" varchar(4) NOT NULL default ' ',
  "AAABAXMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXQD" varchar(2) NOT NULL default ' ',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXQD","AAABEOYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXQD" varchar(2) NOT NULL default ' ',
  "AAABAXNP" varchar(4) NOT NULL default ' ',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXQD","AAABAXNP","AAABEOYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXWB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXOG" decimal(9,2) NOT NULL default '0.00',
  "AAABAXOH" varchar(5) NOT NULL default ' ',
  "AAABAXOI" decimal(9,2) NOT NULL default '0.00',
  "AAABAXOJ" varchar(5) NOT NULL default ' ',
  "AAABAZJU" varchar(3) NOT NULL default '000',
  "AAABAXOL" varchar(3) NOT NULL default '000',
  "AAABAXOM" varchar(3) NOT NULL default '000',
  "AAABAXON" varchar(3) NOT NULL default '000',
  "AAABAXOO" varchar(3) NOT NULL default '000',
  "AAABAXOP" varchar(3) NOT NULL default '000',
  "AAABAXOQ" decimal(9,2) NOT NULL default '0.00',
  "AAABAXOR" varchar(5) NOT NULL default ' ',
  "AAABAXOS" decimal(9,2) NOT NULL default '0.00',
  "AAABAXOT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPJZ" varchar(4) NOT NULL default ' ',
  "AAABLWJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXWR" varchar(2) NOT NULL default ' ',
  "AAABAXPG" varchar(30) NOT NULL default ' ',
  "AAABAXPH" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABAXPJ" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABAXPL" varchar(30) NOT NULL default ' ',
  "AAABAXPM" varchar(30) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABAXPP" varchar(3) NOT NULL default ' ',
  "AAABAXPQ" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(25) NOT NULL default ' ',
  "AAABAXPS" varchar(25) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXQD" varchar(2) NOT NULL default ' ',
  "AAABBTMQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXWB" varchar(3) NOT NULL default ' ',
  "AAABAXWC" varchar(2) NOT NULL default ' ',
  "AAABAXQV" varchar(3) NOT NULL default ' ',
  "AAABAXQJ" varchar(3) NOT NULL default '000',
  "AAABAXQK" varchar(3) NOT NULL default '000',
  "AAABAXQL" decimal(15,2) NOT NULL default '0.00',
  "AAABAXQM" varchar(5) NOT NULL default ' ',
  "AAABAXQN" varchar(20) NOT NULL default ' ',
  "AAABAXVW" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVX" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVY" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVZ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAXWB","AAABAXWC","AAABAXQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXWB" varchar(3) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXWB" varchar(3) NOT NULL default ' ',
  "AAABAXZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGNC" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXXB" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXXB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABAXTI" decimal(15,2) NOT NULL default '0.00',
  "AAABAXSP" varchar(5) NOT NULL default ' ',
  "AAABAXTJ" decimal(15,2) NOT NULL default '0.00',
  "AAABAXSR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHSV","AAABBHVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABAXTI" decimal(5,2) NOT NULL default '0.00',
  "AAABAXTJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBHSV","AAABBHVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABAXTI" decimal(5,2) NOT NULL default '0.00',
  "AAABAXTJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBHSV","AAABBHVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAXWR" varchar(2) NOT NULL default ' ',
  "AAABAXTR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAXWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBHSV","AAABBHVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBHSV","AAABBHVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBHSV","AAABBHVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABBHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXWB" varchar(3) NOT NULL default ' ',
  "AAABAXWC" varchar(2) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXVW" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVX" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVY" decimal(5,2) NOT NULL default '0.00',
  "AAABAXVZ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAXWB","AAABAXWC","AAABBHVU","AAABBHVV","AAABBHVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXWR" varchar(2) NOT NULL default ' ',
  "AAABLTWK" varchar(20) NOT NULL default ' ',
  "AAABAYBM" varchar(2) NOT NULL default ' ',
  "AAABAXWU" varchar(3) NOT NULL default '000',
  "AAABAXWV" varchar(1) NOT NULL default ' ',
  "AAABAXWW" varchar(1) NOT NULL default ' ',
  "AAABAXWX" varchar(8) NOT NULL default '00000000',
  "AAABAXWY" varchar(8) NOT NULL default '00000000',
  "AAABEPJZ" varchar(4) NOT NULL default ' ',
  "AAABAXXA" varchar(4) NOT NULL default ' ',
  "AAABAXXB" varchar(2) NOT NULL default ' ',
  "AAABAYAM" varchar(1) NOT NULL default ' ',
  "AAABAYAU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYMO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXZQ" varchar(2) NOT NULL default ' ',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXZQ","AAABAYMO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXZQ" varchar(2) NOT NULL default ' ',
  "AAABAXYA" varchar(4) NOT NULL default ' ',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAXZQ","AAABAXYA","AAABAYMO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYAD" varchar(2) NOT NULL default ' ',
  "AAABAXYG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYBM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAYBM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABAYDB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXYZ" varchar(3) NOT NULL default '000',
  "AAABAXZA" decimal(7,4) NOT NULL default '0.0000',
  "AAABAYCZ" varchar(20) NOT NULL default ' ',
  "AAABAXZC" decimal(5,2) NOT NULL default '0.00',
  "AAABAXZD" decimal(5,2) NOT NULL default '0.00',
  "AAABCPET" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAYMO","AAABHVXC","AAABBHFX","AAABAYDB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXZQ" varchar(2) NOT NULL default ' ',
  "AAABAXZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAXZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABAYAD" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXZX" varchar(4) NOT NULL default ' ',
  "AAABAXZY" varchar(4) NOT NULL default ' ',
  "AAABAXZZ" varchar(4) NOT NULL default ' ',
  "AAABAYAA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYMO","AAABAYAD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYAF" (
  "AAABAYAM" varchar(1) NOT NULL default ' ',
  "AAABAYAH" varchar(30) NOT NULL default ' ',
  "AAABAYAI" varchar(30) NOT NULL default ' ',
  "AAABAYAJ" varchar(30) NOT NULL default ' ',
  "AAABAYAK" varchar(30) NOT NULL default ' ',
  "AAABAYAL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABAYAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAYAU" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAYAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAYMO","AAABBLIL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYBM" varchar(2) NOT NULL default ' ',
  "AAABAYBJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAYBS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAYCJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYCD" decimal(6,4) NOT NULL default '0.0000',
  "AAABAYCE" decimal(7,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIXT","AAABBCSI","AAABAYCJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYCS" varchar(8) NOT NULL default '00000000',
  "AAABAYCO" varchar(2) NOT NULL default '00',
  "AAABBHVD" decimal(9,2) NOT NULL default '0.00',
  "AAABAYCQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYDB" varchar(2) NOT NULL default ' ',
  "AAABAYCW" varchar(3) NOT NULL default '000',
  "AAABAYCX" decimal(6,4) NOT NULL default '0.0000',
  "AAABAYCY" decimal(5,2) NOT NULL default '0.00',
  "AAABAYCZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABAYDV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAYKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKM" varchar(2) NOT NULL default ' ',
  "AAABAYEM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAYKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABAYKM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYKL","AAABAYKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYFM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABAYKM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYGC" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABAYKL","AAABAYKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYGT" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYHJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABAYKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABAYKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYIB" decimal(5,2) NOT NULL default '0.00',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABAYKL","AAABAYKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABAYKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYIU" decimal(5,2) NOT NULL default '0.00',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABAYKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYJL" decimal(5,2) NOT NULL default '0.00',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYKI" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  "AAABAYKL" varchar(1) NOT NULL default ' ',
  "AAABAYKM" varchar(2) NOT NULL default ' ',
  "AAABAYKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYKE" decimal(5,2) NOT NULL default '0.00',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYKI","AAABAYMG","AAABAYLM","AAABAYKL","AAABAYKM","AAABAYKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYLF" varchar(3) NOT NULL default ' ',
  "AAABAYLB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAYLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAYLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYLW" varchar(4) NOT NULL default ' ',
  "AAABAYLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAYLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAYMG" varchar(4) NOT NULL default ' ',
  "AAABAYMC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAYMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYMO" varchar(4) NOT NULL default ' ',
  "AAABAYML" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABEOYB" varchar(4) NOT NULL default ' ',
  "AAABAYMT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABEOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYTY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYNC" varchar(15) NOT NULL default ' ',
  "AAABAYRY" varchar(2) NOT NULL default ' ',
  "AAABAYNE" varchar(4) NOT NULL default ' ',
  "AAABAYXE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYTY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKGJN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYXR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYOA" varchar(3) NOT NULL default '000',
  "AAABBGOP" varchar(1) NOT NULL default ' ',
  "AAABBGOQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABBGOU","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHALW" varchar(3) NOT NULL default ' ',
  "AAABBGDP" varchar(3) NOT NULL default ' ',
  "AAABAYON" varchar(38) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHALW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYOV" varchar(4) NOT NULL default ' ',
  "AAABAYOT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYPD" varchar(2) NOT NULL default '00',
  "AAABBGOP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABBGOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABAYTY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(4) NOT NULL default ' ',
  "AAABAYPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYRX" varchar(4) NOT NULL default ' ',
  "AAABIVPM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(4) NOT NULL default ' ',
  "AAABAYRY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZBX" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZBW" decimal(7,4) NOT NULL default '0.0000',
  "AAABAYQP" varchar(1) NOT NULL default ' ',
  "AAABAYQQ" varchar(1) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAYQS" decimal(15,2) NOT NULL default '0.00',
  "AAABAYQT" varchar(1) NOT NULL default ' ',
  "AAABAYQU" decimal(15,2) NOT NULL default '0.00',
  "AAABAYQV" varchar(1) NOT NULL default ' ',
  "AAABAYQW" varchar(2) NOT NULL default ' ',
  "AAABAYQX" varchar(2) NOT NULL default ' ',
  "AAABAYQY" decimal(15,2) NOT NULL default '0.00',
  "AAABAYQZ" decimal(15,2) NOT NULL default '0.00',
  "AAABAYRA" decimal(15,2) NOT NULL default '0.00',
  "AAABAYRB" decimal(15,2) NOT NULL default '0.00',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAABAYRD" varchar(4) NOT NULL default ' ',
  "AAABAYRE" varchar(4) NOT NULL default ' ',
  "AAABAYRF" varchar(1) NOT NULL default ' ',
  "AAABAYRG" varchar(1) NOT NULL default ' ',
  "AAABAYRH" varchar(1) NOT NULL default ' ',
  "AAABAYRI" varchar(1) NOT NULL default ' ',
  "AAABAYRJ" varchar(1) NOT NULL default ' ',
  "AAABAYRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX","AAABAYRY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYRX" varchar(4) NOT NULL default ' ',
  "AAABAYRY" varchar(2) NOT NULL default ' ',
  "AAABAYRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYRX","AAABAYRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDWG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDDWG" varchar(2) NOT NULL default ' ',
  "AAABDDSS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDDWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYXE" varchar(5) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABAZBK" varchar(30) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABAYSX" varchar(4) NOT NULL default ' ',
  "AAABAYSY" varchar(30) NOT NULL default ' ',
  "AAABAYSZ" varchar(4) NOT NULL default ' ',
  "AAABAYTA" varchar(30) NOT NULL default ' ',
  "AAABAYTB" varchar(4) NOT NULL default ' ',
  "AAABAYTC" varchar(30) NOT NULL default ' ',
  "AAABAYTD" varchar(4) NOT NULL default ' ',
  "AAABAYTE" varchar(30) NOT NULL default ' ',
  "AAABAYTF" varchar(4) NOT NULL default ' ',
  "AAABAYTG" varchar(30) NOT NULL default ' ',
  "AAABAYTH" varchar(4) NOT NULL default ' ',
  "AAABAYTI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYTY" varchar(4) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYTY" varchar(4) NOT NULL default ' ',
  "AAABAYTV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYUG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYUY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYUG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(2) NOT NULL default ' ',
  "AAABKZAW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBQFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYUY" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABBAHC" decimal(15,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABEHAW" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABAYUY","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBQFF" varchar(2) NOT NULL default ' ',
  "AAABAYWW" varchar(2) NOT NULL default ' ',
  "AAABKRZV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYVH" decimal(15,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBQFF","AAABAYWW","AAABKRZV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFCH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHC" decimal(15,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABEHAW" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBFCH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABAYWJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBQFF" varchar(2) NOT NULL default ' ',
  "AAABAYWW" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBQFF","AAABAYWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYXE" varchar(5) NOT NULL default ' ',
  "AAABAYXB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYXR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYYJ" varchar(4) NOT NULL default ' ',
  "AAABAYXX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAYYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYYJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYYG" decimal(15,2) NOT NULL default '0.00',
  "AAABAYYH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYYJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYYL" (
  "AAABAYYW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYYP" decimal(11,2) NOT NULL default '0.00',
  "AAABHZZJ" decimal(7,4) NOT NULL default '0.0000',
  "AAABAYYR" decimal(11,2) NOT NULL default '0.00',
  "AAABAYYS" decimal(7,4) NOT NULL default '0.0000',
  "AAABAYYT" decimal(11,2) NOT NULL default '0.00',
  "AAABAYYU" decimal(7,4) NOT NULL default '0.0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAYYW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYYY" (
  "AAABAYZR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYZC" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZD" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZE" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZF" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZG" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZH" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZI" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZJ" decimal(11,2) NOT NULL default '0.00',
  "AAABAYZK" decimal(7,2) NOT NULL default '0.00',
  "AAABAYZL" decimal(7,2) NOT NULL default '0.00',
  "AAABAYZM" decimal(7,2) NOT NULL default '0.00',
  "AAABAYZN" varchar(2) NOT NULL default ' ',
  "AAABAYZO" varchar(2) NOT NULL default ' ',
  "AAABAYZP" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAYZR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAYZT" (
  "AAABAZAC" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAZAE" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAYZY" varchar(2) NOT NULL default '00',
  "AAABAYZZ" decimal(11,2) NOT NULL default '0.00',
  "AAABAZAA" decimal(7,4) NOT NULL default '0.0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAZAC","AAABKFCZ","AAABAZAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZAS" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJHFY" varchar(30) NOT NULL default ' ',
  "AAABAZAL" varchar(30) NOT NULL default ' ',
  "AAABAZAM" varchar(30) NOT NULL default ' ',
  "AAABAZAN" varchar(30) NOT NULL default ' ',
  "AAABAZAO" varchar(3) NOT NULL default '000',
  "AAABAZAP" varchar(6) NOT NULL default '000000',
  "AAABAZAQ" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAZAS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZBD" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZAZ" varchar(1) NOT NULL default '0',
  "AAABFSUI" varchar(1) NOT NULL default ' ',
  "AAABAZBB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZBD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZBO" varchar(4) NOT NULL default ' ',
  "AAABFQYU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABAZBK" varchar(20) NOT NULL default ' ',
  "AAABJHFY" varchar(30) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAZBO","AAABFQYU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZBW" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZBX" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZBY" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZBZ" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABAZCB" varchar(1) NOT NULL default ' ',
  "AAABAZCC" varchar(1) NOT NULL default ' ',
  "AAABAZCD" varchar(8) NOT NULL default '00000000',
  "AAABFSUI" varchar(1) NOT NULL default ' ',
  "AAABAZCF" varchar(5) NOT NULL default ' ',
  "AAABAZCG" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZCH" varchar(5) NOT NULL default ' ',
  "AAABAZCI" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZCJ" varchar(5) NOT NULL default ' ',
  "AAABAZCK" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZCL" varchar(4) NOT NULL default ' ',
  "AAABAZCM" varchar(4) NOT NULL default ' ',
  "AAABAZCN" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBDYW" varchar(2) NOT NULL default '00',
  "AAABBACI" varchar(2) NOT NULL default '00',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  "AAABBAJL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZDP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEOO","AAABBEON","AAABBEOP","AAABGLBU","AAABGLBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABAZEO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAZFO" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABAZOV" decimal(12,2) NOT NULL default '0.00',
  "AAABAZEY" varchar(1) NOT NULL default ' ',
  "AAABAZFN" decimal(12,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZFB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABAZFO","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZFN" decimal(12,2) NOT NULL default '0.00',
  "AAABAZFO" varchar(2) NOT NULL default ' ',
  "AAABAZFP" varchar(1) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZFR" varchar(1) NOT NULL default ' ',
  "AAABAZFS" decimal(15,2) NOT NULL default '0.00',
  "AAABAZFT" varchar(1) NOT NULL default ' ',
  "AAABAZFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAABAZGI" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABBDCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABAZGW" decimal(12,2) NOT NULL default '0.00',
  "AAABAZGX" decimal(12,2) NOT NULL default '0.00',
  "AAABILEY" varchar(1) NOT NULL default ' ',
  "AAABAZGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABAZHA" decimal(15,2) NOT NULL default '0.00',
  "AAABAZHB" decimal(15,2) NOT NULL default '0.00',
  "AAABAZHC" decimal(12,2) NOT NULL default '0.00',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAABAZHE" varchar(6) NOT NULL default ' ',
  "AAABAZHF" varchar(1) NOT NULL default '0',
  "AAABMCMV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBDCA","AAABKFCZ","AAABKFCV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZHT" varchar(4) NOT NULL default ' ',
  "AAABAZHU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZJL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZJL" varchar(4) NOT NULL default ' ',
  "AAABAZII" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAZJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZJL" varchar(4) NOT NULL default ' ',
  "AAABAZJM" varchar(1) NOT NULL default ' ',
  "AAABAZIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZJL" varchar(4) NOT NULL default ' ',
  "AAABAZJM" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABAZJE" decimal(9,2) NOT NULL default '0.00',
  "AAABAZJF" decimal(9,2) NOT NULL default '0.00',
  "AAABAZJG" decimal(9,2) NOT NULL default '0.00',
  "AAABAZJH" decimal(5,2) NOT NULL default '0.00',
  "AAABAZJI" varchar(1) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZJL","AAABAZJM","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZJU" varchar(2) NOT NULL default '00',
  "AAABAZJV" varchar(2) NOT NULL default '00',
  "AAABAZJW" decimal(15,3) NOT NULL default '0.000',
  "AAABAZJX" varchar(1) NOT NULL default ' ',
  "AAABAZJY" varchar(8) NOT NULL default ' ',
  "AAABAZJZ" varchar(2) NOT NULL default ' ',
  "AAABAZKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZKM" decimal(15,3) NOT NULL default '0.000',
  "AAABAZKN" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAVI" varchar(20) NOT NULL default ' ',
  "AAABAZKW" varchar(20) NOT NULL default ' ',
  "AAABAZKX" varchar(2) NOT NULL default '00',
  "AAABAZKY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABAZMQ" varchar(1) NOT NULL default ' ',
  "AAABBAVI" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZLJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABAZMQ","AAABBAVI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZLQ" (
  "AAABAZMB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZLU" decimal(6,2) NOT NULL default '0.00',
  "AAABAZLV" decimal(6,2) NOT NULL default '0.00',
  "AAABAZLW" decimal(6,2) NOT NULL default '0.00',
  "AAABAZLX" decimal(6,2) NOT NULL default '0.00',
  "AAABLMLB" varchar(5) NOT NULL default '00000',
  "AAABGZKC" varchar(2) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABAZMB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABAZMQ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZMK" varchar(1) NOT NULL default '0',
  "AAABAZML" decimal(12,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABAZMQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABAZND" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZMZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABAZND","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZNS" varchar(1) NOT NULL default ' ',
  "AAABAZNI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZNS" varchar(1) NOT NULL default ' ',
  "AAABBAET" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAZNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZPR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZPR" varchar(4) NOT NULL default ' ',
  "AAABAZOC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAZPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZPR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZOL" decimal(7,2) NOT NULL default '0.00',
  "AAABAZOM" decimal(7,2) NOT NULL default '0.00',
  "AAABAZQJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZPR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZPQ" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABAZOV" decimal(12,2) NOT NULL default '0.00',
  "AAABAZPO" decimal(12,2) NOT NULL default '0.00',
  "AAABLLOQ" decimal(5,2) NOT NULL default '0.00',
  "AAABAZPR" varchar(4) NOT NULL default ' ',
  "AAABAZQG" decimal(6,2) NOT NULL default '0.00',
  "AAABAZPA" decimal(6,2) NOT NULL default '0.00',
  "AAABAZPB" decimal(12,2) NOT NULL default '0.00',
  "AAABAZPC" decimal(5,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZPQ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHFFR" varchar(2) NOT NULL default '00',
  "AAABAZPO" decimal(12,2) NOT NULL default '0.00',
  "AAABLLOQ" decimal(5,2) NOT NULL default '0.00',
  "AAABAZPQ" varchar(2) NOT NULL default ' ',
  "AAABAZPR" varchar(4) NOT NULL default ' ',
  "AAABAZQG" decimal(6,2) NOT NULL default '0.00',
  "AAABAZPT" decimal(12,2) NOT NULL default '0.00',
  "AAABAZPU" varchar(4) NOT NULL default ' ',
  "AAABAZPV" varchar(4) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZQJ" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABAZQF" decimal(7,2) NOT NULL default '0.00',
  "AAABAZQG" decimal(6,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZQJ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDYW" varchar(2) NOT NULL default '00',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  "AAABAZQP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBDYW" varchar(2) NOT NULL default '00',
  "AAABAZQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBDYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDYW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZRF" decimal(5,2) NOT NULL default '0.00',
  "AAABAZRG" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDYW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZTO" varchar(4) NOT NULL default ' ',
  "AAABAZRN" varchar(1) NOT NULL default ' ',
  "AAABAZRO" varchar(10) NOT NULL default ' ',
  "AAABAZRP" varchar(4) NOT NULL default ' ',
  "AAABAZRQ" varchar(4) NOT NULL default ' ',
  "AAABAZRR" varchar(4) NOT NULL default ' ',
  "AAABAZRS" varchar(4) NOT NULL default ' ',
  "AAABAZSP" varchar(4) NOT NULL default ' ',
  "AAABAZSQ" varchar(4) NOT NULL default ' ',
  "AAABAZSO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZTO" varchar(4) NOT NULL default ' ',
  "AAABAZSC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAZTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZSL" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSM" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSN" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSO" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSP" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSQ" decimal(5,2) NOT NULL default '0.00',
  "AAABAZSR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZTB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZTO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZTL" decimal(5,2) NOT NULL default '0.00',
  "AAABAZTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZTO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABAZVA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZTX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABAZVA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZVA" varchar(4) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZVA" varchar(4) NOT NULL default ' ',
  "AAABAZUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAZVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZVA" varchar(4) NOT NULL default ' ',
  "AAABBAGL" varchar(1) NOT NULL default ' ',
  "AAABBAGM" varchar(1) NOT NULL default ' ',
  "AAABBAGN" varchar(4) NOT NULL default ' ',
  "AAABBAGO" varchar(4) NOT NULL default ' ',
  "AAABBAGP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZVB" (
  "AAABAZWE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZVG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAZWE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZVJ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZWE" varchar(2) NOT NULL default ' ',
  "AAABKZAW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAZWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZVP" (
  "AAABAZYB" varchar(2) NOT NULL default '00',
  "AAABAZVR" varchar(20) NOT NULL default ' ',
  "AAABAZXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAZYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZVU" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZYB" varchar(2) NOT NULL default '00',
  "AAABAZVX" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAZYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZWA" (
  "AAABAZYB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZWE" varchar(2) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZWG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAZYB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZWJ" (
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABAZWL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABAZYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZWN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABAZWQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAZYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZWT" (
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZWY" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAZYK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZXB" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABAZYL" varchar(2) NOT NULL default '00',
  "AAABAZXF" varchar(65) NOT NULL default ' ',
  "AAABAZXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAZYK","AAABAZYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZXK" (
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABAZYL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABAZXQ" decimal(9,2) NOT NULL default '0.00',
  "AAABAZXR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAZYK","AAABAZYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZYB" varchar(2) NOT NULL default '00',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABAZYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZYB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZYK" varchar(2) NOT NULL default '00',
  "AAABAZYL" varchar(2) NOT NULL default '00',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABAZYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZYK","AAABAZYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBRJH" varchar(4) NOT NULL default ' ',
  "AAABAZYQ" varchar(1) NOT NULL default ' ',
  "AAABAZYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBRJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKODS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZYZ" varchar(2) NOT NULL default '00',
  "AAABAZZA" varchar(2) NOT NULL default '00',
  "AAABAZZB" varchar(1) NOT NULL default ' ',
  "AAABAZZC" varchar(3) NOT NULL default '000',
  "AAABAZZD" varchar(3) NOT NULL default '000',
  "AAABAZZE" varchar(2) NOT NULL default '00',
  "AAABAZZF" varchar(2) NOT NULL default '00',
  "AAABAZZG" decimal(12,2) NOT NULL default '0.00',
  "AAABAZZH" decimal(5,2) NOT NULL default '0.00',
  "AAABAZZI" decimal(5,2) NOT NULL default '0.00',
  "AAABAZZJ" decimal(12,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKODS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAZZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKODS" varchar(4) NOT NULL default ' ',
  "AAABBAAB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZZU" varchar(1) NOT NULL default ' ',
  "AAABLNYV" varchar(5) NOT NULL default '00000',
  "AAABAZZW" varchar(1) NOT NULL default ' ',
  "AAABBAAM" varchar(2) NOT NULL default '00',
  "AAABAZZY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKODS","AAABBAAB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKODS" varchar(4) NOT NULL default ' ',
  "AAABBAAM" varchar(2) NOT NULL default '00',
  "AAABBAAI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKODS","AAABBAAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBICB" tinyint(3) unsigned NOT NULL default '0',
  "AAABBAAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABHEHA" varchar(4) NOT NULL default '0000',
  "AAABBABL" varchar(2) NOT NULL default '00',
  "AAABEJLV" varchar(1) NOT NULL default ' ',
  "AAABBABF" decimal(3,2) NOT NULL default '0.00',
  "AAABBABG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABHEHA","AAABBABL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBABM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBACI" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABBABS" varchar(4) NOT NULL default ' ',
  "AAABBABT" varchar(4) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  "AAABBABV" varchar(40) NOT NULL default ' ',
  "AAABBABW" varchar(20) NOT NULL default ' ',
  "AAABBABX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBACI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBACB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBACI" varchar(2) NOT NULL default '00',
  "AAABBACF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBACI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBACJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBADG" varchar(4) NOT NULL default ' ',
  "AAABBACP" varchar(8) NOT NULL default ' ',
  "AAABBACQ" varchar(8) NOT NULL default ' ',
  "AAABBACR" varchar(1) NOT NULL default ' ',
  "AAABBACS" varchar(2) NOT NULL default '00',
  "AAABBACT" varchar(2) NOT NULL default '00',
  "AAABBACU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBACY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBADG" varchar(4) NOT NULL default ' ',
  "AAABBADH" varchar(2) NOT NULL default '00',
  "AAABBADC" decimal(9,2) NOT NULL default '0.00',
  "AAABBADD" decimal(6,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBADG","AAABBADH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBADI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBADY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBADY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBADQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBADY" varchar(4) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABLXZQ" decimal(9,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBADY","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABBAED" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAFF" varchar(1) NOT NULL default ' ',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  "AAABBAEK" varchar(1) NOT NULL default ' ',
  "AAABBAEL" varchar(1) NOT NULL default ' ',
  "AAABBAEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAFF" varchar(1) NOT NULL default ' ',
  "AAABBAET" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAFF" varchar(1) NOT NULL default ' ',
  "AAABBAFG" varchar(1) NOT NULL default ' ',
  "AAABBAFC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAFF","AAABBAFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABBAGR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAFO" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABBAGR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAGR" varchar(4) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAGR" varchar(4) NOT NULL default ' ',
  "AAABHAJG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAGR" varchar(4) NOT NULL default ' ',
  "AAABBAGL" varchar(1) NOT NULL default ' ',
  "AAABBAGM" varchar(1) NOT NULL default ' ',
  "AAABBAGN" varchar(4) NOT NULL default ' ',
  "AAABBAGO" varchar(4) NOT NULL default ' ',
  "AAABBAGP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAGS" (
  "AAABDHBS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJATT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDHBS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAGZ" (
  "AAABDHBS" varchar(4) NOT NULL default ' ',
  "AAABBAHH" varchar(2) NOT NULL default ' ',
  "AAABBAHC" decimal(12,2) NOT NULL default '0.00',
  "AAABBAHD" varchar(2) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  "AAABBAHF" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABDHBS","AAABBAHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  "AAABBAHL" varchar(20) NOT NULL default ' ',
  "AAABBAHM" varchar(20) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBDXT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCA","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAIV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAABBAIP" varchar(1) NOT NULL default ' ',
  "AAABBAIQ" decimal(9,2) NOT NULL default '0.00',
  "AAABBDCA" varchar(4) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAIV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAJL" varchar(2) NOT NULL default '00',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAJL" varchar(2) NOT NULL default '00',
  "AAABBAJI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAJR" decimal(6,2) NOT NULL default '0.00',
  "AAABBAJS" decimal(6,2) NOT NULL default '0.00',
  "AAABCCUR" varchar(2) NOT NULL default '00',
  "AAABBAJU" varchar(1) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAKF" decimal(6,2) NOT NULL default '0.00',
  "AAABBAKG" varchar(1) NOT NULL default ' ',
  "AAABBAKH" varchar(1) NOT NULL default ' ',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQMM" varchar(4) NOT NULL default ' ',
  "AAABBAKX" varchar(2) NOT NULL default '00',
  "AAABBAKY" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAKU" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKQMM","AAABBAKX","AAABBAKY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBALA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBALO" varchar(2) NOT NULL default ' ',
  "AAABBALD" tinyint(3) unsigned NOT NULL default '0',
  "AAABBALE" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBALO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBALH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBALO" varchar(2) NOT NULL default ' ',
  "AAABBALL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBALO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBALP" (
  "AAABDSZA" varchar(16) NOT NULL default ' ',
  "AAABBALV" varchar(5) NOT NULL default '00000',
  "AAABMNMZ" VARBINARY(255),
  "AAABMNNA" VARBINARY(255),
  PRIMARY KEY ("AAABDSZA","AAABBALV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBALW" (
  "AAABDSZA" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBALZ" (
  "AAABDSZA" varchar(16) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABDSZA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  "AAABJKRT" varchar(4) NOT NULL default ' ',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAABBAMX" decimal(5,2) NOT NULL default '0.00',
  "AAABBAMY" varchar(2) NOT NULL default ' ',
  "AAABBAMZ" varchar(2) NOT NULL default ' ',
  "AAABBANA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBBEK","AAABEPKB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBANG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABBAOD" varchar(1) NOT NULL default ' ',
  "AAABBAOE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBBEK","AAABBAOD","AAABBAOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBANR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABBAOD" varchar(1) NOT NULL default ' ',
  "AAABBAOE" varchar(2) NOT NULL default ' ',
  "AAABBANY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAABBBEK","AAABBAOD","AAABBAOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  "AAABEPJR" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAOW" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAPY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABJFHB","AAABBAPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAPB" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAPY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAPI" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAPY" varchar(2) NOT NULL default '00',
  "AAABIKBY" varchar(4) NOT NULL default '0000',
  "AAABBAPM" decimal(11,2) NOT NULL default '0.00',
  "AAABBAPN" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABJFHB","AAABBAPY","AAABIKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAPR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAPY" varchar(2) NOT NULL default '00',
  "AAABBBGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABJFHB","AAABBAPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBBEK","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAQU" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBARD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  "AAABBARH" varchar(10) NOT NULL default ' ',
  "AAABBARI" varchar(20) NOT NULL default ' ',
  "AAABBARJ" varchar(4) NOT NULL default ' ',
  "AAABBARK" varchar(4) NOT NULL default ' ',
  "AAABBARL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJKXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBARP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBASB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBASB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBARU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBASB" varchar(2) NOT NULL default '00',
  "AAABBARY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBASB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBASC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBASO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBASO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBASH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBASO" varchar(2) NOT NULL default '00',
  "AAABBASL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBASO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBASP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBATC" varchar(2) NOT NULL default '00',
  "AAABBASS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBATC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBASV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBATC" varchar(2) NOT NULL default '00',
  "AAABBASZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBATC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBATD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBATP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBATP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBATI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBATP" varchar(1) NOT NULL default ' ',
  "AAABBATM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBATP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBATQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBATV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAUC" varchar(1) NOT NULL default ' ',
  "AAABBATZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAUP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAUP" varchar(3) NOT NULL default ' ',
  "AAABBAUM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAUQ" (
  "AAABMCUW" varchar(3) NOT NULL default '000',
  "AAABBAUV" varchar(2) NOT NULL default ' ',
  "AAABBAVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCUW","AAABBAUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAVI" varchar(1) NOT NULL default ' ',
  "AAABBAVA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAVT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBAVP" tinyint(3) unsigned NOT NULL default '0',
  "AAABBAVQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABBAVR" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBAVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAABBBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAWP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAWP" varchar(2) NOT NULL default ' ',
  "AAABBAWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAXC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAXC" varchar(2) NOT NULL default ' ',
  "AAABBAWZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAYF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAYF" varchar(4) NOT NULL default ' ',
  "AAABBAXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAYF" varchar(4) NOT NULL default ' ',
  "AAABBAYG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAYF","AAABBAYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAYF" varchar(4) NOT NULL default ' ',
  "AAABBAYG" varchar(4) NOT NULL default ' ',
  "AAABBAYC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAYF","AAABBAYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBBEP" decimal(6,3) NOT NULL default '0.000',
  "AAABBBEQ" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABBBEK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAZD" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAZD" varchar(12) NOT NULL default ' ',
  "AAABBAZA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBAZQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAZL" varchar(2) NOT NULL default '00',
  "AAABBAZM" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBGDJ","AAABHVYL","AAABBAZQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBAZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBAK" varchar(2) NOT NULL default ' ',
  "AAABBBCR" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFLSG" varchar(3) NOT NULL default ' ',
  "AAABBBAI" decimal(10,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABBBAK","AAABBBCR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCSI" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBBAU" varchar(3) NOT NULL default '000',
  "AAABJKRQ" varchar(1) NOT NULL default ' ',
  "AAABBBAW" varchar(3) NOT NULL default '000',
  "AAABBBAX" varchar(1) NOT NULL default '0',
  "AAABBBAY" varchar(30) NOT NULL default ' ',
  "AAABBBAZ" varchar(4) NOT NULL default ' ',
  "AAABBBBA" varchar(3) NOT NULL default '000',
  "AAABBBBB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCSI","AAABGAMX","AAABKFCZ","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDDW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBDDW" varchar(4) NOT NULL default ' ',
  "AAABBBBQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBDDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDDW" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDDW","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCSI" varchar(2) NOT NULL default ' ',
  "AAABBDDW" varchar(4) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBBCG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBCSI","AAABBDDW","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBCL" (
  "AAABBBCR" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBBCP" decimal(15,2) NOT NULL default '0.00',
  "AAABBBCQ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABBBCR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  "AAABBBDA" varchar(2) NOT NULL default ' ',
  "AAABBBDB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBBFS","AAABKFCZ","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGAMX" varchar(2) NOT NULL default ' ',
  "AAABBBDP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMX" varchar(2) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMX","AAABBBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBBEK" varchar(2) NOT NULL default ' ',
  "AAABBBEE" varchar(30) NOT NULL default ' ',
  "AAABMGWU" varchar(10) NOT NULL default ' ',
  "AAABBBEG" varchar(1) NOT NULL default ' ',
  "AAABBBEH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBBEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBEL" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBBEP" decimal(6,3) NOT NULL default '0.000',
  "AAABBBEQ" decimal(6,3) NOT NULL default '0.000',
  "AAABBBER" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBEU" (
  "AAABBHOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBHOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBEX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHOF" varchar(1) NOT NULL default ' ',
  "AAABMJVD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBHOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBFD" (
  "AAABBBGU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABBBGU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABBBFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABETOU","AAABBBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBFT" (
  "AAABBBGU" varchar(2) NOT NULL default '00',
  "AAABBBFZ" varchar(3) NOT NULL default '000',
  "AAABBGAF" decimal(11,2) NOT NULL default '0.00',
  "AAABBBFX" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABBBGU","AAABBBFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHOF" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBBGF" varchar(1) NOT NULL default ' ',
  "AAABLXRR" varchar(1) NOT NULL default ' ',
  "AAABBBGH" varchar(1) NOT NULL default ' ',
  "AAABBBGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHOF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBGM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBBGU" varchar(2) NOT NULL default '00',
  "AAABBBGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBGS" (
  "AAABBBGU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABBBGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBHE" (
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
CREATE TABLE "AAABBBHR" (
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
  KEY "AAABBBIH" ("AAABMRFO","AAABMLQA","AAABHZDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBIL" (
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
  KEY "AAABBBJG" ("AAABLLLD","AAABLLLF","AAABLLLE","AAABLLLG","AAABLLLA","AAABLLLO"),
  KEY "AAABBBJN" ("AAABLLLO","AAABLLLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBKL" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBKP" (
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
  UNIQUE KEY "AAABBBLE" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBLH" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBLY" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBMC" (
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
  UNIQUE KEY "AAABBBMR" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBMU" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBNL" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBNP" (
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
CREATE TABLE "AAABBBOD" (
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
  UNIQUE KEY "AAABBBOT" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBOW" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBPN" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBPR" (
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
  KEY "AAABBBQT" ("AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBQV" (
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
  UNIQUE KEY "AAABBBRL" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBRO" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABLLNN","AAABLLMD","AAABLLMB"),
  KEY "AAABBBSH" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBSX" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBTB" (
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
  UNIQUE KEY "AAABBBTQ" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBTT" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBUK" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBUO" (
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
  UNIQUE KEY "AAABBBVD" ("AAABMRIL","AAABLLNL"),
  KEY "AAABBBVG" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNI" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLNA" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLNI","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABBBVX" ("AAABLLMD","AAABLLME","AAABLLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(80) NOT NULL default ' ',
  "AAABHAZH" varchar(13) NOT NULL default ' ',
  "AAABBBWS" varchar(3) NOT NULL default '000',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABBCDL" varchar(6) NOT NULL default '000000',
  "AAABLVHJ" varchar(40) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABBCKE" varchar(4) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABBBXH" varchar(1) NOT NULL default ' ',
  "AAABBBXI" varchar(1) NOT NULL default ' ',
  "AAABBBXJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABBCGV" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABBBXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ","AAABBCGV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBBYF" varchar(8) NOT NULL default '00000000',
  "AAABBCDU" varchar(11) NOT NULL default ' ',
  "AAABBBYH" varchar(2) NOT NULL default '00',
  "AAABBBYI" varchar(2) NOT NULL default '00',
  "AAABBBYJ" varchar(2) NOT NULL default ' ',
  "AAABBBYK" varchar(8) NOT NULL default ' ',
  "AAABBBYL" varchar(5) NOT NULL default '00000',
  "AAABBBYM" varchar(9) NOT NULL default ' ',
  "AAABBBYN" varchar(2) NOT NULL default '00',
  "AAABBBYO" varchar(2) NOT NULL default '00',
  "AAABBBYP" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABDIJC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABBCAM" decimal(13,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCAH" decimal(13,2) NOT NULL default '0.00',
  "AAABBBZG" decimal(13,2) NOT NULL default '0.00',
  "AAABBBZH" decimal(4,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCBO","AAABBCAM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBBZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABBCAM" decimal(13,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCAH" decimal(13,2) NOT NULL default '0.00',
  "AAABBBZU" decimal(13,2) NOT NULL default '0.00',
  "AAABBBZV" decimal(4,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCBO","AAABBCAM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABBCAM" decimal(13,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCAH" decimal(13,2) NOT NULL default '0.00',
  "AAABBCAI" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCBO","AAABBCAM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCGM" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCAT" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBCGM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCBC" decimal(4,2) NOT NULL default '0.00',
  "AAABBCBD" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCBO" varchar(4) NOT NULL default ' ',
  "AAABBCBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCBV" decimal(9,5) NOT NULL default '0.00000',
  "AAABBCBW" varchar(4) NOT NULL default '0000',
  "AAABLOZD" varchar(3) NOT NULL default ' ',
  "AAABBCBY" varchar(3) NOT NULL default '000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKY" varchar(2) NOT NULL default ' ',
  "AAABBCKZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCKU" decimal(3,0) NOT NULL default '0',
  "AAABBCKV" decimal(5,3) NOT NULL default '0.000',
  "AAABBCKW" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABBCKY","AAABBCKZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCCT" (
  "AAABBCDA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCCX" varchar(4) NOT NULL default ' ',
  "AAABBCCY" varchar(1) NOT NULL default ' ',
  "AAABBCCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCDA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCDC" (
  "AAABBCKZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCDU" varchar(11) NOT NULL default ' ',
  "AAABBCDI" varchar(50) NOT NULL default ' ',
  "AAABBCKY" varchar(2) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABBCDL" varchar(6) NOT NULL default '000000',
  "AAABLVHJ" varchar(40) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABBCKE" varchar(4) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABBCDS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCDV" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCKZ" varchar(4) NOT NULL default ' ',
  "AAABBCDY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBCKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCEJ" varchar(2) NOT NULL default ' ',
  "AAABBCEK" smallint(6) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCEH" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBCEJ","AAABBCEK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCES" varchar(1) NOT NULL default ' ',
  "AAABBCET" varchar(1) NOT NULL default ' ',
  "AAABBCEU" varchar(1) NOT NULL default ' ',
  "AAABBCEV" varchar(1) NOT NULL default ' ',
  "AAABBCEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCFI" varchar(4) NOT NULL default ' ',
  "AAABBCFJ" varchar(4) NOT NULL default ' ',
  "AAABBCFK" varchar(4) NOT NULL default ' ',
  "AAABBCFL" varchar(4) NOT NULL default ' ',
  "AAABBCFM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBGSQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCFS" (
  "AAABBCGL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCFW" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCGL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCGA" (
  "AAABBCGM" varchar(3) NOT NULL default ' ',
  "AAABBCKE" varchar(4) NOT NULL default ' ',
  "AAABBCGD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCGM","AAABBCKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCGG" (
  "AAABBCGM" varchar(3) NOT NULL default ' ',
  "AAABBCKE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCGL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCGM","AAABBCKE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCGP" (
  "AAABBCGW" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCGT" varchar(4) NOT NULL default ' ',
  "AAABBCGU" varchar(1) NOT NULL default ' ',
  "AAABBCGV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBCGW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCIQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCIQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCIH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCIH" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCIQ" varchar(4) NOT NULL default ' ',
  "AAABBCIH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCIQ","AAABBCIH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCIQ" varchar(4) NOT NULL default ' ',
  "AAABBCIR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBCIQ","AAABBCIR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJA" varchar(1) NOT NULL default ' ',
  "AAABBCJB" varchar(2) NOT NULL default ' ',
  "AAABBCIX" varchar(1) NOT NULL default ' ',
  "AAABBCIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJA","AAABBCJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJL" varchar(6) NOT NULL default ' ',
  "AAABBCJM" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBCJH" varchar(1) NOT NULL default ' ',
  "AAABBCJI" varchar(4) NOT NULL default ' ',
  "AAABBCJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJL","AAABBCJM","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJT" varchar(2) NOT NULL default ' ',
  "AAABBCJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABBCKE" varchar(4) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABBCKN" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABLVCU","AAABBCKE","AAABMQNA","AAABBCKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKN" varchar(7) NOT NULL default ' ',
  "AAABBCKK" varchar(40) NOT NULL default ' ',
  "AAABBCKL" varchar(90) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKY" varchar(2) NOT NULL default ' ',
  "AAABBCKZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCKU" decimal(3,0) NOT NULL default '0',
  "AAABBCKV" decimal(5,3) NOT NULL default '0.000',
  "AAABBCKW" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABBCKY","AAABBCKZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBCLF" varchar(3) NOT NULL default ' ',
  "AAABBCLG" varchar(5) NOT NULL default ' ',
  "AAABBCLH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCMJ" varchar(4) NOT NULL default ' ',
  "AAABBCLO" varchar(15) NOT NULL default ' ',
  "AAABBCLP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCMJ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBCMJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBCMJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCML" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBCMT" varchar(1) NOT NULL default ' ',
  "AAABBCMU" varchar(3) NOT NULL default '000',
  "AAABBCMQ" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHVYP","AAABBCMT","AAABBCMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBCMY" varchar(9) NOT NULL default ' ',
  "AAABBCMZ" varchar(32) NOT NULL default ' ',
  "AAABBCNA" varchar(32) NOT NULL default ' ',
  "AAABBCNB" varchar(30) NOT NULL default ' ',
  "AAABBCNC" varchar(30) NOT NULL default ' ',
  "AAABBCND" varchar(30) NOT NULL default ' ',
  "AAABBCNE" varchar(14) NOT NULL default ' ',
  "AAABBCNF" varchar(3) NOT NULL default '000',
  "AAABBCNG" varchar(25) NOT NULL default ' ',
  "AAABBCNH" varchar(25) NOT NULL default ' ',
  "AAABBCNI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABBCTV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBDCQ","AAABBCTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABBCTV" varchar(2) NOT NULL default '00',
  "AAABBCOD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDCP","AAABBDCQ","AAABBCTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFAAU" varchar(4) NOT NULL default '0000',
  "AAABBCRI" varchar(1) NOT NULL default ' ',
  "AAABBCSJ" varchar(4) NOT NULL default ' ',
  "AAABBCRK" varchar(1) NOT NULL default ' ',
  "AAABBCTN" decimal(5,2) NOT NULL default '0.00',
  "AAABBCRM" varchar(1) NOT NULL default ' ',
  "AAABBCRN" varchar(4) NOT NULL default ' ',
  "AAABBCRO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABLUAS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABLUAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABLUAS" varchar(1) NOT NULL default ' ',
  "AAABBCPV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDCP","AAABLUAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBCTK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBCTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBCTK" varchar(2) NOT NULL default ' ',
  "AAABBCQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDCP","AAABBCTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFAAU" varchar(4) NOT NULL default '0000',
  "AAABBCRI" varchar(1) NOT NULL default ' ',
  "AAABBCSJ" varchar(4) NOT NULL default ' ',
  "AAABBCRK" varchar(1) NOT NULL default ' ',
  "AAABBCTN" decimal(5,2) NOT NULL default '0.00',
  "AAABBCRM" varchar(1) NOT NULL default ' ',
  "AAABBCRN" varchar(4) NOT NULL default ' ',
  "AAABBCRO" varchar(4) NOT NULL default ' ',
  "AAABBCRP" varchar(4) NOT NULL default ' ',
  "AAABBCRQ" varchar(4) NOT NULL default ' ',
  "AAABBCRR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHT","AAABGCHS","AAABGCHZ","AAABGCHU","AAABBGYO","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABBCSJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBDCQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABBCTV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFAAU" varchar(4) NOT NULL default '0000',
  "AAABBCSY" varchar(1) NOT NULL default ' ',
  "AAABBCSZ" decimal(9,5) NOT NULL default '0.00000',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  "AAABBCTB" varchar(1) NOT NULL default ' ',
  "AAABBCTC" varchar(1) NOT NULL default ' ',
  "AAABBCTD" varchar(3) NOT NULL default '000',
  "AAABBCTE" varchar(1) NOT NULL default ' ',
  "AAABBCTF" varchar(4) NOT NULL default ' ',
  "AAABBCTG" decimal(9,2) NOT NULL default '0.00',
  "AAABBCTH" decimal(9,2) NOT NULL default '0.00',
  "AAABBCTI" varchar(1) NOT NULL default ' ',
  "AAABBCTJ" varchar(2) NOT NULL default ' ',
  "AAABBCTK" varchar(2) NOT NULL default ' ',
  "AAABLUAS" varchar(1) NOT NULL default ' ',
  "AAABBCTM" varchar(1) NOT NULL default ' ',
  "AAABBCTN" decimal(5,2) NOT NULL default '0.00',
  "AAABBCTO" varchar(1) NOT NULL default ' ',
  "AAABBCTP" varchar(3) NOT NULL default '000',
  "AAABBCTQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBDCQ","AAABBCTV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCUE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABBDCR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDCL" varchar(4) NOT NULL default ' ',
  "AAABBDCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHT","AAABGCHS","AAABGCHU","AAABBDCR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBDCQ","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCWB" decimal(10,5) NOT NULL default '0.00000',
  "AAABBCWC" decimal(10,5) NOT NULL default '0.00000',
  "AAABBCWD" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHT","AAABGCHS","AAABGCHU","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHD","AAABJFHB","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLTWK" varchar(15) NOT NULL default ' ',
  "AAABBCXH" varchar(4) NOT NULL default ' ',
  "AAABBCXI" varchar(4) NOT NULL default ' ',
  "AAABBCXJ" varchar(4) NOT NULL default ' ',
  "AAABBCXK" varchar(4) NOT NULL default ' ',
  "AAABBCXL" decimal(5,2) NOT NULL default '0.00',
  "AAABBCXM" decimal(5,2) NOT NULL default '0.00',
  "AAABBCXN" decimal(5,2) NOT NULL default '0.00',
  "AAABBCXO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMNLW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCXT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBCYA" varchar(2) NOT NULL default ' ',
  "AAABBCXX" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHVYP","AAABBCYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBCYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABBCYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDXT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  "AAABBCZL" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHHA" decimal(7,4) NOT NULL default '0.0000',
  "AAABBCZH" decimal(9,2) NOT NULL default '0.00',
  "AAABBCZI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABBCZL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBCZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  "AAABBCZY" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCZT" decimal(7,4) NOT NULL default '0.0000',
  "AAABBCZU" decimal(9,2) NOT NULL default '0.00',
  "AAABBCZV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABBCZY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBDBM" varchar(3) NOT NULL default ' ',
  "AAABBDAE" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBDBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDBM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  "AAABBDAZ" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDAT" decimal(9,2) NOT NULL default '0.00',
  "AAABCPET" decimal(7,4) NOT NULL default '0.0000',
  "AAABBHOT" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABBDAZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDBM" varchar(3) NOT NULL default ' ',
  "AAABBDBN" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDBV" decimal(9,2) NOT NULL default '0.00',
  "AAABCPET" decimal(7,4) NOT NULL default '0.0000',
  "AAABBHOT" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDBM","AAABBDBN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCA" varchar(6) NOT NULL default ' ',
  "AAABBHOR" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDBV" decimal(9,2) NOT NULL default '0.00',
  "AAABCPET" decimal(7,4) NOT NULL default '0.0000',
  "AAABBHOT" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCA","AAABBHOR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABBDCR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDCL" varchar(4) NOT NULL default ' ',
  "AAABBDCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAABBDCQ","AAABBDCR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDDO" varchar(4) NOT NULL default ' ',
  "AAABESXK" varchar(1) NOT NULL default ' ',
  "AAABKIUC" varchar(2) NOT NULL default ' ',
  "AAABBDCZ" varchar(4) NOT NULL default ' ',
  "AAABBDDA" varchar(2) NOT NULL default ' ',
  "AAABBDDB" varchar(4) NOT NULL default ' ',
  "AAABBDDC" varchar(2) NOT NULL default ' ',
  "AAABBDDD" varchar(4) NOT NULL default ' ',
  "AAABBDDE" varchar(2) NOT NULL default ' ',
  "AAABBDDF" varchar(4) NOT NULL default ' ',
  "AAABBDDG" varchar(2) NOT NULL default ' ',
  "AAABBDDH" varchar(4) NOT NULL default ' ',
  "AAABBDDI" varchar(2) NOT NULL default ' ',
  "AAABBDDJ" varchar(4) NOT NULL default ' ',
  "AAABBDDK" varchar(1) NOT NULL default ' ',
  "AAABHALW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABKIUC" varchar(2) NOT NULL default ' ',
  "AAABBDDT" varchar(2) NOT NULL default '00',
  "AAABBDDU" varchar(1) NOT NULL default ' ',
  "AAABDWBA" varchar(2) NOT NULL default ' ',
  "AAABBDDW" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABBDDY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABKIUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABKIUC" varchar(2) NOT NULL default ' ',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABKIUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDEM" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  "AAABBDEQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABBDEW" varchar(1) NOT NULL default ' ',
  "AAABBDMH" varchar(4) NOT NULL default ' ',
  "AAABGAMX" varchar(4) NOT NULL default ' ',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  "AAABBDFP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDPS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDPS" varchar(4) NOT NULL default ' ',
  "AAABBDGG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABBDGX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDHS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDHS" varchar(4) NOT NULL default ' ',
  "AAABBDHO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(4) NOT NULL default ' ',
  "AAABBDIF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDMH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDMH" varchar(4) NOT NULL default ' ',
  "AAABBDIW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  "AAABBDJN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGAPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABCBMT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABCBMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABCBMT" varchar(4) NOT NULL default ' ',
  "AAABBEWM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABCBMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDOR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDOR" varchar(4) NOT NULL default ' ',
  "AAABBDKV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBDOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDMH" varchar(4) NOT NULL default ' ',
  "AAABBDPS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDMH","AAABBDPS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAPD","AAABMRFO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(4) NOT NULL default ' ',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDMH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMX","AAABCALH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDPS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  "AAABBDMU" varchar(2) NOT NULL default ' ',
  "AAABBDMV" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDPS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDQH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABCBMT" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDNT" varchar(4) NOT NULL default ' ',
  "AAABBDNU" varchar(2) NOT NULL default ' ',
  "AAABBDNV" varchar(4) NOT NULL default ' ',
  "AAABBDOR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABCBMT","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDOR" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLRVC" varchar(4) NOT NULL default ' ',
  "AAABBDOK" varchar(3) NOT NULL default ' ',
  "AAABLRVD" varchar(4) NOT NULL default ' ',
  "AAABBDOM" varchar(1) NOT NULL default ' ',
  "AAABBDON" varchar(1) NOT NULL default ' ',
  "AAABBDOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDOR","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  "AAABKRXK" VARBINARY(255) NOT NULL,
  "AAABBDPS" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(4) NOT NULL default ' ',
  "AAABBDPC" varchar(8) NOT NULL default '00000000',
  "AAABBDPD" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAABLOZE" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABBDPM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMCRV","AAABBDQH","AAABKRXK"(255),"AAABBDPS","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDPU" (
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABBDPW" varchar(40) NOT NULL default ' ',
  "AAABBDPX" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDQH" varchar(4) NOT NULL default ' ',
  "AAABGXGE" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDQH","AAABGXGE","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABBDRP" varchar(4) NOT NULL default ' ',
  "AAABBDRR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABBDRP","AAABBDRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABBDRE" varchar(10) NOT NULL default ' ',
  "AAABBDQX" varchar(1) NOT NULL default ' ',
  "AAABBDQY" varchar(1) NOT NULL default ' ',
  "AAABBDQZ" varchar(1) NOT NULL default ' ',
  "AAABBDRA" varchar(1) NOT NULL default ' ',
  "AAABBDRB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABBDRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABBDRP" varchar(4) NOT NULL default ' ',
  "AAABBDSE" varchar(15) NOT NULL default ' ',
  "AAABBDRR" varchar(1) NOT NULL default ' ',
  "AAABBDRY" varchar(1) NOT NULL default ' ',
  "AAABLWPJ" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABBDRP","AAABBDSE","AAABBDRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSB" varchar(10) NOT NULL default ' ',
  "AAABGCHF" varchar(2) NOT NULL default '00',
  "AAABLGKR" varchar(2) NOT NULL default ' ',
  "AAABBDSE" varchar(15) NOT NULL default ' ',
  "AAABBDRY" varchar(1) NOT NULL default ' ',
  "AAABLWPJ" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDSB","AAABGCHF","AAABLGKR","AAABBDSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSL" varchar(10) NOT NULL default ' ',
  "AAABBDSM" varchar(1) NOT NULL default '0',
  "AAABJSEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSL","AAABBDSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMCQP" varchar(4) NOT NULL default ' ',
  "AAABBDUY" varchar(4) NOT NULL default ' ',
  "AAABBDUK" varchar(4) NOT NULL default ' ',
  "AAABBDUR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDTS" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBDTY" varchar(50) NOT NULL default ' ',
  "AAABBDTZ" varchar(50) NOT NULL default ' ',
  "AAABBDUA" varchar(30) NOT NULL default ' ',
  "AAABBDVC" varchar(13) NOT NULL default ' ',
  "AAABBDUV" varchar(11) NOT NULL default ' ',
  "AAABBDUH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABBDTS","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDVD" varchar(4) NOT NULL default ' ',
  "AAABBDTY" varchar(50) NOT NULL default ' ',
  "AAABBDTZ" varchar(50) NOT NULL default ' ',
  "AAABBDUA" varchar(30) NOT NULL default ' ',
  "AAABBDUB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDUK" varchar(4) NOT NULL default ' ',
  "AAABBDUH" varchar(10) NOT NULL default '0000000000',
  "AAABBDVD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDUR" varchar(4) NOT NULL default ' ',
  "AAABBDUO" varchar(12) NOT NULL default ' ',
  "AAABBDVD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDUY" varchar(4) NOT NULL default ' ',
  "AAABBDUV" varchar(11) NOT NULL default ' ',
  "AAABBDVD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCQP" varchar(4) NOT NULL default ' ',
  "AAABBDVC" varchar(13) NOT NULL default ' ',
  "AAABBDVD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDVP" varchar(2) NOT NULL default ' ',
  "AAABHDBV" varchar(4) NOT NULL default ' ',
  "AAABBDVR" varchar(2) NOT NULL default '00',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBDVM" varchar(10) NOT NULL default ' ',
  "AAABBDVN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBDVP","AAABHDBV","AAABBDVR","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDWE" varchar(2) NOT NULL default ' ',
  "AAABBDWR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDWM" decimal(9,2) NOT NULL default '0.00',
  "AAABBDWO" decimal(9,2) NOT NULL default '0.00',
  "AAABBDWB" decimal(9,2) NOT NULL default '0.00',
  "AAABBDWC" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDWE","AAABBDWR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDWQ" varchar(2) NOT NULL default ' ',
  "AAABBDWR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBDWM" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDWO" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDWQ","AAABBDWR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDZJ" varchar(2) NOT NULL default ' ',
  "AAABKRZV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDWZ" decimal(15,2) NOT NULL default '0.00',
  "AAABBDXA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDZJ","AAABKRZV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDZJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDXK" decimal(15,2) NOT NULL default '0.00',
  "AAABFRKK" decimal(15,2) NOT NULL default '0.00',
  "AAABBDXM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDZJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDZK" varchar(4) NOT NULL default ' ',
  "AAABBDXT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDZK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBDZK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDYU" varchar(2) NOT NULL default ' ',
  "AAABLUFX" varchar(2) NOT NULL default ' ',
  "AAABBDYW" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDYO" varchar(1) NOT NULL default ' ',
  "AAABBDYP" decimal(5,2) NOT NULL default '0.00',
  "AAABBDYQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBDYR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDYU","AAABLUFX","AAABBDYW","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRZV" varchar(2) NOT NULL default '00',
  "AAABBDZJ" varchar(2) NOT NULL default ' ',
  "AAABBDZK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBDZG" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKRZV","AAABBDZJ","AAABBDZK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDZM" (
  "AAABBERD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABBERD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDZP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABBDZS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBERD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBDZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABBEKM" varchar(1) NOT NULL default ' ',
  "AAABBEKN" varchar(1) NOT NULL default ' ',
  "AAABBEKO" varchar(3) NOT NULL default ' ',
  "AAABBEAL" varchar(1) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABBEAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABLYAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABBEAZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABLYAX","AAABBEAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABBEKO" varchar(3) NOT NULL default ' ',
  "AAABBEBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABBEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABBEKO" varchar(3) NOT NULL default ' ',
  "AAABBEBN" varchar(5) NOT NULL default ' ',
  "AAABBEBO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBERD","AAABBEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABBEBX" varchar(1) NOT NULL default ' ',
  "AAABBEBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABKZYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBECC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEDJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBECH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBEDJ" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBEDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBECP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABBELL" varchar(3) NOT NULL default '000',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZYT","AAABBELL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBECX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABBEDJ" varchar(3) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABMCNU","AAABBEDJ","AAABKZYT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABBEDR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBERD","AAABKZYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABBEEB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBERD","AAABLYAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABBEEP" varchar(10) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBEEM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABBEEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEGI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBEGI" varchar(2) NOT NULL default ' ',
  "AAABBEFC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMLQA","AAABBEGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEGI" varchar(2) NOT NULL default ' ',
  "AAABBEGJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABBKTQ" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEGI","AAABBEGJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBEGI" varchar(2) NOT NULL default ' ',
  "AAABBEGJ" varchar(4) NOT NULL default ' ',
  "AAABBEGE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMLQA","AAABBEGI","AAABBEGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABIVIM" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABIVIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABBEHH" varchar(8) NOT NULL default ' ',
  "AAABBEHI" varchar(2) NOT NULL default '00',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABBEHH","AAABBEHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABKPBA" varchar(5) NOT NULL default ' ',
  "AAABBKTQ" varchar(1) NOT NULL default ' ',
  "AAABBELL" decimal(5,2) NOT NULL default '0.00',
  "AAABBETM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABKZYT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETM" varchar(2) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  "AAABBELL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBETM","AAABKZYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEMB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKKBF" varchar(1) NOT NULL default ' ',
  "AAABLWJT" varchar(25) NOT NULL default ' ',
  "AAABLPWW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKKBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(8) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCRGY" varchar(12) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBERD","AAABLYAX","AAABKFCZ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABBEKM" varchar(1) NOT NULL default ' ',
  "AAABBEKN" varchar(1) NOT NULL default ' ',
  "AAABBEKO" varchar(3) NOT NULL default ' ',
  "AAABBEKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABLHPM","AAABBEKM","AAABBEKN","AAABBEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBEKV" varchar(1) NOT NULL default ' ',
  "AAABBEKW" varchar(1) NOT NULL default ' ',
  "AAABBEKX" varchar(1) NOT NULL default ' ',
  "AAABBEKY" varchar(1) NOT NULL default ' ',
  "AAABBEKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABLYAX","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBELF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBELL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBELQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEMB" varchar(1) NOT NULL default '0',
  "AAABBELW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBEON","AAABBEMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBEMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEMK" (
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBERD","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEMP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABBEMT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBERD","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEMX" (
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAABBENE" varchar(1) NOT NULL default ' ',
  "AAABBENB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBERD","AAABMCNJ","AAABBENE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBENF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBENM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABBENP" varchar(1) NOT NULL default ' ',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBENT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABBENX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBEOW" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABBEPJ" varchar(8) NOT NULL default ' ',
  "AAABBEOW" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABBEPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABBEPJ" varchar(8) NOT NULL default ' ',
  "AAABLPUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLHPM","AAABBEPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEQA" varchar(2) NOT NULL default ' ',
  "AAABBEQB" varchar(2) NOT NULL default ' ',
  "AAABBEQC" varchar(1) NOT NULL default ' ',
  "AAABBEQD" varchar(8) NOT NULL default ' ',
  "AAABBEQE" varchar(2) NOT NULL default ' ',
  "AAABBEQF" varchar(2) NOT NULL default ' ',
  "AAABBEQG" varchar(1) NOT NULL default ' ',
  "AAABBEQH" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIWAM" decimal(8,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEQA","AAABBEQB","AAABBEQC","AAABBEQD","AAABBEQE","AAABBEQF","AAABBEQG","AAABBEQH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(2) NOT NULL default ' ',
  "AAABBEQS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERD" varchar(2) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERD","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBERF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABBERJ" varchar(8) NOT NULL default '00000000',
  "AAABBERK" varchar(1) NOT NULL default ' ',
  "AAABBERL" varchar(1) NOT NULL default ' ',
  "AAABBERM" varchar(1) NOT NULL default ' ',
  "AAABBERN" varchar(1) NOT NULL default ' ',
  "AAABBERO" varchar(1) NOT NULL default ' ',
  "AAABFWHB" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIKQ","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBERT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABIYCS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABKFCV","AAABKFCZ","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBESI" (
  "AAABIYCS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBESL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(5) NOT NULL default ' ',
  "AAABBESO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBESR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBESW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBETZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBETA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBETM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBETF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBETM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBETM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBETN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBETZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBETS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBETZ" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBETZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEUU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBEUU" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBEUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEUU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBEUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEUU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEUW" (
  "AAABBEVK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABBEVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEUZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBEVK" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABBEVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEVK" varchar(2) NOT NULL default ' ',
  "AAABLMQH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABBEYS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBFBB" varchar(2) NOT NULL default ' ',
  "AAABBFCQ" varchar(2) NOT NULL default ' ',
  "AAABBFAH" varchar(2) NOT NULL default ' ',
  "AAABBEZA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFYE" varchar(4) NOT NULL default ' ',
  "AAABBEWM" varchar(40) NOT NULL default ' ',
  "AAABBEWN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFAH" varchar(2) NOT NULL default ' ',
  "AAABBEWU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFAH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEZA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFBB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEYS" varchar(2) NOT NULL default ' ',
  "AAABBEXR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFCQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEZA" varchar(2) NOT NULL default ' ',
  "AAABBEYS" varchar(2) NOT NULL default ' ',
  "AAABBGIT" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFAE" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBEZA","AAABBEYS","AAABBGIT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEYS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEZA" varchar(2) NOT NULL default ' ',
  "AAABHIHN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFBB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBEZG" varchar(10) NOT NULL default ' ',
  "AAABBEZH" varchar(6) NOT NULL default ' ',
  "AAABBFYE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFBB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFAS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFAE" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFAS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBEZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFAH" varchar(2) NOT NULL default ' ',
  "AAABBFAS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFAP" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAQ" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAE" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFAH","AAABBFAS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFAS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFAP" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAQ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFAS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFBB" varchar(2) NOT NULL default ' ',
  "AAABBFAY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFCH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFCD" decimal(13,2) NOT NULL default '0.00',
  "AAABEHAW" decimal(5,2) NOT NULL default '0.00',
  "AAABBFCF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFCH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFCQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFBS" varchar(3) NOT NULL default ' ',
  "AAABBFBT" varchar(10) NOT NULL default ' ',
  "AAABBFYE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFCQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFCH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFCD" decimal(13,2) NOT NULL default '0.00',
  "AAABEHAW" decimal(5,2) NOT NULL default '0.00',
  "AAABBFCF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFCH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFCQ" varchar(2) NOT NULL default ' ',
  "AAABBFCN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHS","AAABGCHT","AAABGCHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHS","AAABGCHT","AAABGCHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBFEB" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(2) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABBFEB","AAABKWQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGCIA" varchar(4) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABGCIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABBFIX" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABBFNO" varchar(10) NOT NULL default ' ',
  "AAABEXWS" varchar(100) NOT NULL default ' ',
  "AAABBFEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  "AAABBFFO" varchar(30) NOT NULL default ' ',
  "AAABBFFP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  "AAABBFGT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  "AAABBFOJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFMH","AAABBFOJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFLM" varchar(5) NOT NULL default ' ',
  "AAABBFMA" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFLM" varchar(30) NOT NULL default ' ',
  "AAABBFLK" varchar(30) NOT NULL default ' ',
  "AAABBFLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFID" (
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFMC" varchar(5) NOT NULL default ' ',
  "AAABLUER" varchar(6) NOT NULL default ' ',
  "AAABBFMA" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFMC","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFIP" (
  "AAABBFIX" varchar(15) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFIX","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFIV" (
  "AAABBFIX" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFIY" (
  "AAABBFLH" varchar(30) NOT NULL default ' ',
  "AAABBFJJ" varchar(30) NOT NULL default ' ',
  "AAABBFJK" varchar(30) NOT NULL default ' ',
  "AAABBFJL" varchar(30) NOT NULL default ' ',
  "AAABBFJM" varchar(30) NOT NULL default ' ',
  "AAABBFJN" varchar(30) NOT NULL default ' ',
  "AAABBFJF" varchar(1) NOT NULL default ' ',
  "AAABBFJG" varchar(1) NOT NULL default ' ',
  "AAABMJWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFLH","AAABBFJJ","AAABBFJK","AAABBFJL","AAABBFJM","AAABBFJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFJO" (
  "AAABBFLM" varchar(5) NOT NULL default ' ',
  "AAABBFMA" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBFKE" varchar(2) NOT NULL default ' ',
  "AAABBFKF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR","AAABHVYP","AAABBFKE","AAABBFKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFKG" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABBFLH" varchar(30) NOT NULL default ' ',
  "AAABBFKJ" varchar(1) NOT NULL default ' ',
  "AAABBFKK" varchar(1) NOT NULL default ' ',
  "AAABLOAI" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBFKZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR","AAABHVYP","AAABGCHZ","AAABBFKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFLA" (
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABBFLH" varchar(30) NOT NULL default ' ',
  "AAABBFLD" varchar(1) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLOAI" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLOZE","AAABBFLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFLI" (
  "AAABBFLM" varchar(30) NOT NULL default ' ',
  "AAABBFLK" varchar(30) NOT NULL default ' ',
  "AAABBFLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFLN" (
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFLQ" (
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABLEEW" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABBFLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMYV","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFLX" (
  "AAABBFMC" varchar(5) NOT NULL default ' ',
  "AAABLUER" varchar(6) NOT NULL default ' ',
  "AAABBFMA" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFMC","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFMJ" varchar(4) NOT NULL default ' ',
  "AAABBFMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBHAY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR","AAABGCHT","AAABGCHS","AAABGCHZ","AAABBHAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFNI" varchar(30) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABGOLD" varchar(4) NOT NULL default ' ',
  "AAABGOLE" varchar(4) NOT NULL default ' ',
  "AAABGOLF" varchar(4) NOT NULL default ' ',
  "AAABGOLG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFNI","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFNK" (
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABBFNO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQGL","AAABBFNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFNP" (
  "AAABBFNV" varchar(30) NOT NULL default ' ',
  "AAABBFNW" varchar(30) NOT NULL default ' ',
  "AAABBFNS" varchar(40) NOT NULL default ' ',
  "AAABBFNT" varchar(30) NOT NULL default ' ',
  "AAABCNCP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBFNV","AAABBFNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFOJ" varchar(4) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFOB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFOJ","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFOJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGBI" varchar(3) NOT NULL default '000',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABIGSK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFOY" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBFPH" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(8) NOT NULL default ' ',
  "AAABBFPJ" varchar(1) NOT NULL default ' ',
  "AAABBFPK" varchar(9) NOT NULL default ' ',
  "AAABBGDN" varchar(3) NOT NULL default ' ',
  "AAABBGEP" varchar(4) NOT NULL default ' ',
  "AAABBGDL" varchar(12) NOT NULL default ' ',
  "AAABBGEX" varchar(1) NOT NULL default ' ',
  "AAABBGMH" varchar(1) NOT NULL default ' ',
  "AAABBFYE" varchar(2) NOT NULL default ' ',
  "AAABBFPR" varchar(1) NOT NULL default ' ',
  "AAABBFPS" varchar(10) NOT NULL default ' ',
  "AAABBFXV" varchar(1) NOT NULL default '0',
  "AAABBFXW" varchar(1) NOT NULL default ' ',
  "AAABBFPV" varchar(1) NOT NULL default ' ',
  "AAABBFPW" varchar(9) NOT NULL default ' ',
  "AAABBFPX" varchar(20) NOT NULL default ' ',
  "AAABBFPY" varchar(8) NOT NULL default ' ',
  "AAABBFPZ" varchar(1) NOT NULL default ' ',
  "AAABBFQA" varchar(9) NOT NULL default ' ',
  "AAABBFQB" varchar(3) NOT NULL default ' ',
  "AAABBFQC" varchar(4) NOT NULL default ' ',
  "AAABBFQD" varchar(12) NOT NULL default ' ',
  "AAABBGEA" varchar(60) NOT NULL default ' ',
  "AAABBFQF" varchar(60) NOT NULL default ' ',
  "AAABBFUX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFQK" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSS" varchar(12) NOT NULL default ' ',
  "AAABLRST" varchar(40) NOT NULL default ' ',
  "AAABLRSU" varchar(8) NOT NULL default '00000000',
  "AAABLRSV" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABBFRP" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFVK" decimal(13,2) NOT NULL default '0.00',
  "AAABBFVL" varchar(5) NOT NULL default ' ',
  "AAABBFRL" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZW","AAABBFRP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABBGMU" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFRX" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFZW","AAABBGMU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFSH" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZF" varchar(2) NOT NULL default ' ',
  "AAABBFYX" varchar(2) NOT NULL default ' ',
  "AAABBFSV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZF","AAABBFYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABBFTJ" varchar(1) NOT NULL default ' ',
  "AAABBFTD" varchar(40) NOT NULL default ' ',
  "AAABBFTE" varchar(16) NOT NULL default ' ',
  "AAABBGBI" varchar(3) NOT NULL default '000',
  "AAABBFTG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZW","AAABBFTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFYE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFUU" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFYE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFXV" varchar(1) NOT NULL default '0',
  "AAABBFXW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFUE" decimal(7,2) NOT NULL default '0.00',
  "AAABBFUF" decimal(7,2) NOT NULL default '0.00',
  "AAABBFUG" decimal(7,2) NOT NULL default '0.00',
  "AAABBFUH" decimal(7,2) NOT NULL default '0.00',
  "AAABBFUI" decimal(7,2) NOT NULL default '0.00',
  "AAABBFUJ" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFXV","AAABBFXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFYE" varchar(2) NOT NULL default ' ',
  "AAABBFUX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFUU" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBFYE","AAABBFUX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABFDPJ" varchar(8) NOT NULL default '00000000',
  "AAABBFVI" varchar(30) NOT NULL default ' ',
  "AAABLRBI" varchar(2) NOT NULL default ' ',
  "AAABBFVK" decimal(13,2) NOT NULL default '0.00',
  "AAABBFVL" varchar(5) NOT NULL default ' ',
  "AAABBFVM" varchar(8) NOT NULL default '00000000',
  "AAABBFVN" varchar(8) NOT NULL default '00000000',
  "AAABBFVO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABJFHB","AAABBFVS","AAABLQVJ","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBFWJ" varchar(2) NOT NULL default '00',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  "AAABBFWL" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBGCX","AAABBFWJ","AAABBFWK","AAABBFWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBFZF" varchar(2) NOT NULL default ' ',
  "AAABBFWP" varchar(2) NOT NULL default ' ',
  "AAABBFWQ" varchar(2) NOT NULL default ' ',
  "AAABBFWR" varchar(2) NOT NULL default ' ',
  "AAABBFWS" varchar(2) NOT NULL default ' ',
  "AAABBFWT" varchar(2) NOT NULL default ' ',
  "AAABBFWU" varchar(2) NOT NULL default ' ',
  "AAABBFWV" varchar(2) NOT NULL default ' ',
  "AAABBFWW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBFZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABBFXC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFXM" varchar(1) NOT NULL default ' ',
  "AAABBFXJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFXV" varchar(1) NOT NULL default '0',
  "AAABBFXW" varchar(1) NOT NULL default ' ',
  "AAABBFXS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFXV","AAABBFXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFYE" varchar(2) NOT NULL default ' ',
  "AAABBFYB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFYM" varchar(1) NOT NULL default ' ',
  "AAABBFYJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFZF" varchar(2) NOT NULL default ' ',
  "AAABBFYX" varchar(2) NOT NULL default ' ',
  "AAABBFYS" varchar(40) NOT NULL default ' ',
  "AAABBFYT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFZF","AAABBFYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFZF" varchar(2) NOT NULL default ' ',
  "AAABBFZC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFZN" varchar(2) NOT NULL default ' ',
  "AAABBFZK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAABBFZS" varchar(10) NOT NULL default ' ',
  "AAABBFZT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBFZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBFZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGAI" varchar(3) NOT NULL default '000',
  "AAABBGAJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGAQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBGAR" varchar(5) NOT NULL default ' ',
  "AAABBGAF" decimal(13,2) NOT NULL default '0.00',
  "AAABBGAU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGAI","AAABBGAJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGAW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGAQ" decimal(15,2) NOT NULL default '0.00',
  "AAABBGAR" varchar(5) NOT NULL default ' ',
  "AAABBGDU" decimal(7,2) NOT NULL default '0.00',
  "AAABBGAT" decimal(13,2) NOT NULL default '0.00',
  "AAABBGAU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGAW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGBL" varchar(3) NOT NULL default ' ',
  "AAABBGBG" varchar(40) NOT NULL default ' ',
  "AAABBGBH" varchar(30) NOT NULL default ' ',
  "AAABBGBI" varchar(3) NOT NULL default '000',
  "AAABBGBJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBGBX" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGBS" decimal(7,2) NOT NULL default '0.00',
  "AAABBHVD" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABBGBX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPKB" varchar(8) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBGCL" varchar(7) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBGCY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPKB","AAABBGCX","AAABBGCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGEP" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(8) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBGCY" varchar(3) NOT NULL default ' ',
  "AAABBGCS" varchar(7) NOT NULL default ' ',
  "AAABBGCT" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGEP","AAABEPKB","AAABBGCX","AAABBGCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBGEM" varchar(8) NOT NULL default '00000000',
  "AAABBGEN" varchar(8) NOT NULL default '00000000',
  "AAABBKHW" varchar(3) NOT NULL default ' ',
  "AAABBGEP" varchar(11) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBGDL" varchar(12) NOT NULL default ' ',
  "AAABEPKB" varchar(8) NOT NULL default ' ',
  "AAABBGDN" varchar(3) NOT NULL default ' ',
  "AAABKGJN" varchar(10) NOT NULL default ' ',
  "AAABBGDP" varchar(1) NOT NULL default ' ',
  "AAABBGDQ" varchar(25) NOT NULL default ' ',
  "AAABBGRP" varchar(40) NOT NULL default ' ',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAABIYJF" varchar(60) NOT NULL default ' ',
  "AAABBGDU" decimal(7,2) NOT NULL default '0.00',
  "AAABBGDV" int(11) NOT NULL default '0',
  "AAABBMLA" int(11) NOT NULL default '0',
  "AAABBGDX" int(11) NOT NULL default '0',
  "AAABBGDY" varchar(60) NOT NULL default ' ',
  "AAABBGDZ" varchar(60) NOT NULL default ' ',
  "AAABBGEA" varchar(60) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABBGEC" varchar(1) NOT NULL default ' ',
  "AAABBGED" varchar(1) NOT NULL default ' ',
  "AAABHALW" varchar(3) NOT NULL default ' ',
  "AAABBGEF" varchar(30) NOT NULL default ' ',
  "AAABLRVD" varchar(1) NOT NULL default ' ',
  "AAABBGEH" varchar(8) NOT NULL default '00000000',
  "AAABBGEI" varchar(1) NOT NULL default ' ',
  "AAABBGEJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBGEM","AAABBGEN","AAABBKHW","AAABBGEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGEX" varchar(1) NOT NULL default ' ',
  "AAABJGBR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGMH" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGFD" decimal(13,2) NOT NULL default '0.00',
  "AAABBGFE" varchar(5) NOT NULL default ' ',
  "AAABBGFF" decimal(13,2) NOT NULL default '0.00',
  "AAABBGFG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGMH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGFT" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGFP" decimal(13,2) NOT NULL default '0.00',
  "AAABBGFQ" varchar(5) NOT NULL default ' ',
  "AAABBGFR" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBGFT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGLZ" varchar(1) NOT NULL default ' ',
  "AAABBGGH" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGIT" varchar(3) NOT NULL default '000',
  "AAABBGGC" decimal(6,2) NOT NULL default '0.00',
  "AAABGZKC" varchar(1) NOT NULL default ' ',
  "AAABBGIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGLZ","AAABBGGH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBGLZ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGGQ" decimal(8,2) NOT NULL default '0.00',
  "AAABBGGR" decimal(13,2) NOT NULL default '0.00',
  "AAABBGGS" varchar(5) NOT NULL default ' ',
  "AAABBGLB" varchar(1) NOT NULL default ' ',
  "AAABBGKT" varchar(1) NOT NULL default ' ',
  "AAABBGJN" varchar(2) NOT NULL default '00',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  "AAABBGJP" decimal(8,2) NOT NULL default '0.00',
  "AAABBGJQ" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABBGLZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGLR" varchar(1) NOT NULL default ' ',
  "AAABBGHQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGIT" varchar(3) NOT NULL default '000',
  "AAABBGHL" decimal(6,2) NOT NULL default '0.00',
  "AAABGZKC" varchar(1) NOT NULL default ' ',
  "AAABBGIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGLR","AAABBGHQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBGLR" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGHZ" decimal(8,2) NOT NULL default '0.00',
  "AAABBGIA" decimal(13,2) NOT NULL default '0.00',
  "AAABBGIB" varchar(5) NOT NULL default ' ',
  "AAABBGLB" varchar(1) NOT NULL default ' ',
  "AAABBGKT" varchar(1) NOT NULL default ' ',
  "AAABBGJN" varchar(2) NOT NULL default '00',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  "AAABBGJP" decimal(8,2) NOT NULL default '0.00',
  "AAABBGJQ" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABBGLR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGLJ" varchar(1) NOT NULL default ' ',
  "AAABBGIZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGIT" varchar(3) NOT NULL default '000',
  "AAABBGIU" decimal(6,2) NOT NULL default '0.00',
  "AAABGZKC" varchar(1) NOT NULL default ' ',
  "AAABBGIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGLJ","AAABBGIZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBGLJ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGJI" decimal(8,2) NOT NULL default '0.00',
  "AAABBGJJ" decimal(13,2) NOT NULL default '0.00',
  "AAABBGJK" varchar(5) NOT NULL default ' ',
  "AAABBGLB" varchar(1) NOT NULL default ' ',
  "AAABBGKT" varchar(1) NOT NULL default ' ',
  "AAABBGJN" varchar(2) NOT NULL default '00',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  "AAABBGJP" decimal(8,2) NOT NULL default '0.00',
  "AAABBGJQ" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABBGLJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGLB" varchar(1) NOT NULL default ' ',
  "AAABBGJZ" varchar(2) NOT NULL default '00',
  "AAABBGKA" varchar(3) NOT NULL default '000',
  "AAABBGKB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBGLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGKT" varchar(1) NOT NULL default ' ',
  "AAABBGKH" varchar(2) NOT NULL default '00',
  "AAABBGKI" varchar(3) NOT NULL default '000',
  "AAABBGKJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBGKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGKT" varchar(1) NOT NULL default ' ',
  "AAABBGKQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGLB" varchar(1) NOT NULL default ' ',
  "AAABBGKY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGLJ" varchar(1) NOT NULL default ' ',
  "AAABBGLG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGLR" varchar(1) NOT NULL default ' ',
  "AAABBGLO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGLZ" varchar(1) NOT NULL default ' ',
  "AAABBGLW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGMH" varchar(1) NOT NULL default ' ',
  "AAABBGME" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGMU" varchar(1) NOT NULL default ' ',
  "AAABBGMR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGNC" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABGAMR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGNN" varchar(4) NOT NULL default ' ',
  "AAABBGNO" varchar(4) NOT NULL default ' ',
  "AAABBGNP" varchar(4) NOT NULL default ' ',
  "AAABBGNQ" varchar(4) NOT NULL default ' ',
  "AAABBGOA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGOA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABMKGE" varchar(100) NOT NULL default ' ',
  "AAABBGOM" varchar(3) NOT NULL default '000',
  "AAABBGON" varchar(1) NOT NULL default ' ',
  "AAABBGOO" varchar(3) NOT NULL default '000',
  "AAABBGOP" varchar(1) NOT NULL default ' ',
  "AAABBGOQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABBGOU","AAABLUVE","AAABGCHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGZJM" varchar(6) NOT NULL default ' ',
  "AAABBGPC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABGZJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGQB" varchar(4) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  "AAABBGPY" varchar(1) NOT NULL default ' ',
  "AAABBGPZ" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBGQB","AAABBGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGQJ" varchar(3) NOT NULL default '000',
  "AAABBGQK" varchar(4) NOT NULL default ' ',
  "AAABBGQL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABHFFR" varchar(4) NOT NULL default ' ',
  "AAABBGRT" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBGRF" varchar(8) NOT NULL default '00000000',
  "AAABBGQX" varchar(8) NOT NULL default '00000000',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAABBGRP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABHFFR","AAABBGRT","AAABBICN","AAABBGRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABHFFR" varchar(4) NOT NULL default ' ',
  "AAABBGRT" varchar(2) NOT NULL default ' ',
  "AAABBGRL" varchar(1) NOT NULL default ' ',
  "AAABBGRM" varchar(8) NOT NULL default '00000000',
  "AAABBGRN" varchar(8) NOT NULL default '00000000',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAABBGRP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABHFFR","AAABBGRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABBGSO" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAABBGSR" varchar(4) NOT NULL default ' ',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAABMMJT" decimal(15,2) NOT NULL default '0.00',
  "AAABBGSG" varchar(5) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBICM","AAABBICN","AAABEKQV","AAABBGSO","AAABLUVE","AAABBGSQ","AAABBGSR","AAABBLIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(5) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBGSY" varchar(1) NOT NULL default '0',
  "AAABBGSZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJFGP","AAABGCHD","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGTN" varchar(2) NOT NULL default ' ',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHZ","AAABBGTN","AAABBGYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGTZ" varchar(2) NOT NULL default ' ',
  "AAABBGUA" varchar(3) NOT NULL default '000',
  "AAABBGUB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHZ","AAABBGTZ","AAABBGUA","AAABBGUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(18) NOT NULL default ' ',
  "AAABGMHB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(18) NOT NULL default ' ',
  "AAABBGUN" varchar(1) NOT NULL default ' ',
  "AAABBGUO" varchar(1) NOT NULL default ' ',
  "AAABBGUP" varchar(3) NOT NULL default ' ',
  "AAABBGUQ" varchar(3) NOT NULL default ' ',
  "AAABBGUR" varchar(1) NOT NULL default ' ',
  "AAABBGUS" varchar(1) NOT NULL default ' ',
  "AAABBGUT" varchar(20) NOT NULL default ' ',
  "AAABBGUU" varchar(2) NOT NULL default ' ',
  "AAABBGUV" varchar(1) NOT NULL default ' ',
  "AAABBGUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJFGP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(5) NOT NULL default ' ',
  "AAABBGVL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHZ","AAABBGYO","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGWH" decimal(5,2) NOT NULL default '0.00',
  "AAABBGWI" varchar(1) NOT NULL default ' ',
  "AAABBGWJ" varchar(2) NOT NULL default '00',
  "AAABBGWK" varchar(2) NOT NULL default ' ',
  "AAABBGWL" decimal(3,0) NOT NULL default '0',
  "AAABBGWM" varchar(3) NOT NULL default ' ',
  "AAABBGWN" decimal(3,0) NOT NULL default '0',
  "AAABBGWO" varchar(3) NOT NULL default ' ',
  "AAABBGWP" varchar(3) NOT NULL default '000',
  "AAABBGWQ" varchar(4) NOT NULL default ' ',
  "AAABBGWR" decimal(7,2) NOT NULL default '0.00',
  "AAABBGWS" decimal(3,0) NOT NULL default '0',
  "AAABBGWT" varchar(3) NOT NULL default ' ',
  "AAABBGWU" varchar(1) NOT NULL default ' ',
  "AAABBGWV" varchar(4) NOT NULL default ' ',
  "AAABBGWW" varchar(8) NOT NULL default ' ',
  "AAABBGWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHZ","AAABBGYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABBGYK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGCHZ","AAABBGYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGXV" varchar(1) NOT NULL default ' ',
  "AAABBGXW" decimal(8,3) NOT NULL default '0.000',
  "AAABBGXX" varchar(1) NOT NULL default '0',
  "AAABBGXY" varchar(4) NOT NULL default ' ',
  "AAABBGXZ" varchar(8) NOT NULL default ' ',
  "AAABBGYA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGCHZ","AAABBGYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBGYO" varchar(3) NOT NULL default '000',
  "AAABBGYK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCHZ","AAABBGYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(40) NOT NULL default ' ',
  "AAABBGYT" varchar(1) NOT NULL default ' ',
  "AAABBGYU" varchar(15) NOT NULL default ' ',
  "AAABBGYV" varchar(15) NOT NULL default ' ',
  "AAABBHAG" varchar(15) NOT NULL default ' ',
  "AAABBGYX" varchar(10) NOT NULL default ' ',
  "AAABBGYY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBGZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBKLO" varchar(40) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABBGZI" varchar(9) NOT NULL default ' ',
  "AAABBGZJ" varchar(40) NOT NULL default ' ',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAABBGZL" varchar(40) NOT NULL default ' ',
  "AAABBGZM" decimal(15,2) NOT NULL default '0.00',
  "AAABBGZN" varchar(5) NOT NULL default ' ',
  "AAABBGZO" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHKL" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHEZ" varchar(2) NOT NULL default '00',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  "AAABBGZS" varchar(4) NOT NULL default ' ',
  "AAABBGZT" decimal(15,2) NOT NULL default '0.00',
  "AAABBGZU" varchar(5) NOT NULL default ' ',
  "AAABLXRR" decimal(15,2) NOT NULL default '0.00',
  "AAABBGZW" varchar(5) NOT NULL default ' ',
  "AAABBGZX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAABBHAG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABBHAY" varchar(2) NOT NULL default ' ',
  "AAABBHAZ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHAT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHZ","AAABBHAY","AAABBHAZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABBHBN" varchar(4) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABMAYY" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABEJKE" decimal(5,2) NOT NULL default '0.00',
  "AAABBHBJ" varchar(4) NOT NULL default ' ',
  "AAABBREO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHDA","AAABBHBN","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABBHBY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHCH" varchar(30) NOT NULL default ' ',
  "AAABBHCI" varchar(4) NOT NULL default ' ',
  "AAABBHCJ" varchar(4) NOT NULL default ' ',
  "AAABBHCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHDA" varchar(4) NOT NULL default ' ',
  "AAABBHCX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDZ" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHEK" varchar(1) NOT NULL default ' ',
  "AAABBHEL" varchar(1) NOT NULL default ' ',
  "AAABBHEM" varchar(1) NOT NULL default ' ',
  "AAABBHEN" varchar(1) NOT NULL default ' ',
  "AAABBHEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHEZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBHEZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBHFH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAABBHFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABHXKL" varchar(4) NOT NULL default ' ',
  "AAABBHFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABBHGE" varchar(2) NOT NULL default '00',
  "AAABBHGF" varchar(4) NOT NULL default '0000',
  "AAABBHGG" decimal(15,2) NOT NULL default '0.00',
  "AAABFQOM" varchar(2) NOT NULL default ' ',
  "AAABBHNG" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHGJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBHNF" varchar(1) NOT NULL default ' ',
  "AAABIFWM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBHGZ" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHA" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHHB" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHC" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHD" decimal(15,2) NOT NULL default '0.00',
  "AAABLXRR" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHF" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHG" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHH" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHI" decimal(15,2) NOT NULL default '0.00',
  "AAABBHHJ" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHHK" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFQOM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHNG" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABFQOM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABBHII" decimal(15,2) NOT NULL default '0.00',
  "AAABBHIJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBHIK" decimal(15,2) NOT NULL default '0.00',
  "AAABBHIL" decimal(15,2) NOT NULL default '0.00',
  "AAABBHIM" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABBHIZ" varchar(1) NOT NULL default ' ',
  "AAABBHJA" decimal(15,2) NOT NULL default '0.00',
  "AAABBHJB" decimal(15,2) NOT NULL default '0.00',
  "AAABFPTJ" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHJD" decimal(9,4) NOT NULL default '0.0000',
  "AAABIFWM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJBLB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAYY" varchar(4) NOT NULL default ' ',
  "AAABKWRL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHKA" varchar(3) NOT NULL default '000',
  "AAABBHKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAYY","AAABKWRL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHKN" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHKL" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBHKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHKW" varchar(1) NOT NULL default ' ',
  "AAABBHKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHLD" (
  "AAABJBLB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHLH" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHLI" decimal(9,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABJBLB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHLL" (
  "AAABBHLT" varchar(4) NOT NULL default '0000',
  "AAABBHLN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABBHLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBHLT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABBHMG" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMH" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMI" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMK" decimal(15,2) NOT NULL default '0.00',
  "AAABBHML" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMM" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMN" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMO" decimal(15,2) NOT NULL default '0.00',
  "AAABBHMP" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABBHNC" varchar(2) NOT NULL default ' ',
  "AAABBHND" decimal(15,2) NOT NULL default '0.00',
  "AAABBHNE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHNF" varchar(1) NOT NULL default ' ',
  "AAABBHNG" decimal(9,4) NOT NULL default '0.0000',
  "AAABIFWM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHNM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJBLB" varchar(2) NOT NULL default '00',
  "AAABBHNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABJBLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAABBWGQ" varchar(5) NOT NULL default '00000',
  "AAABBHNX" VARBINARY(255),
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABBHOB" varchar(4) NOT NULL default '0000',
  "AAABBHOC" varchar(2) NOT NULL default '00',
  "AAABBHOD" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHOE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHOF" decimal(15,2) NOT NULL default '0.00',
  "AAABBHOG" decimal(15,2) NOT NULL default '0.00',
  "AAABBHOH" decimal(15,2) NOT NULL default '0.00',
  "AAABIFWM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLUVE","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHON" (
  "AAABBHOU" decimal(2,0) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHOR" decimal(11,2) NOT NULL default '0.00',
  "AAABKWVR" decimal(9,4) NOT NULL default '0.0000',
  "AAABBHOT" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABBHOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHPC" varchar(4) NOT NULL default ' ',
  "AAABBHPD" varchar(4) NOT NULL default ' ',
  "AAABBHPE" varchar(4) NOT NULL default ' ',
  "AAABBHPF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHPT" varchar(4) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABBHSB" varchar(4) NOT NULL default ' ',
  "AAABBHPQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHPT","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHSB" varchar(4) NOT NULL default ' ',
  "AAABBHUZ" decimal(3,0) NOT NULL default '0',
  "AAABBHSO" varchar(2) NOT NULL default '00',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSS" varchar(1) NOT NULL default ' ',
  "AAABBHQF" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABBHTH" varchar(2) NOT NULL default '00',
  "AAABBHTI" varchar(2) NOT NULL default '00',
  "AAABBHTK" decimal(5,2) NOT NULL default '0.00',
  "AAABBHTL" varchar(2) NOT NULL default '00',
  "AAABBHTM" varchar(2) NOT NULL default '00',
  "AAABBHTN" varchar(1) NOT NULL default ' ',
  "AAABBICF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHSB" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBHSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHSB" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABBIBL" varchar(4) NOT NULL default ' ',
  "AAABBHVX" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHUR" decimal(15,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHSB","AAABBHVT","AAABBHVU","AAABBHVV","AAABBIBL","AAABBHVX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWG" varchar(4) NOT NULL default ' ',
  "AAABBHSN" decimal(3,0) NOT NULL default '0',
  "AAABBHSO" varchar(2) NOT NULL default '00',
  "AAABBHSP" varchar(1) NOT NULL default ' ',
  "AAABBHSQ" varchar(1) NOT NULL default ' ',
  "AAABBHSR" varchar(1) NOT NULL default ' ',
  "AAABBHSS" varchar(1) NOT NULL default ' ',
  "AAABBHST" varchar(1) NOT NULL default ' ',
  "AAABBHSU" varchar(1) NOT NULL default ' ',
  "AAABBHSV" varchar(4) NOT NULL default ' ',
  "AAABBHSW" varchar(2) NOT NULL default '00',
  "AAABBHSX" varchar(2) NOT NULL default '00',
  "AAABBHSY" varchar(2) NOT NULL default '00',
  "AAABBHSZ" varchar(2) NOT NULL default '00',
  "AAABBHTA" varchar(2) NOT NULL default '00',
  "AAABBHTB" varchar(2) NOT NULL default '00',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABBHTD" varchar(2) NOT NULL default ' ',
  "AAABBHTE" varchar(4) NOT NULL default ' ',
  "AAABBHTF" varchar(2) NOT NULL default '00',
  "AAABBHTG" varchar(2) NOT NULL default '00',
  "AAABBHTH" varchar(2) NOT NULL default '00',
  "AAABBHTI" varchar(2) NOT NULL default '00',
  "AAABBHTJ" varchar(1) NOT NULL default ' ',
  "AAABBHTK" decimal(5,2) NOT NULL default '0.00',
  "AAABBHTL" varchar(2) NOT NULL default '00',
  "AAABBHTM" varchar(2) NOT NULL default '00',
  "AAABBHTN" varchar(1) NOT NULL default ' ',
  "AAABBICF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWG" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBICN","AAABBHWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWG" varchar(4) NOT NULL default ' ',
  "AAABBHVT" varchar(4) NOT NULL default ' ',
  "AAABBHVU" varchar(4) NOT NULL default ' ',
  "AAABBHVV" varchar(4) NOT NULL default ' ',
  "AAABBIAU" varchar(4) NOT NULL default ' ',
  "AAABBHVX" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHUR" decimal(15,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  "AAABBHUT" varchar(1) NOT NULL default ' ',
  "AAABBHUU" decimal(3,0) NOT NULL default '0',
  "AAABBHUV" decimal(15,2) NOT NULL default '0.00',
  "AAABBHUW" decimal(15,2) NOT NULL default '0.00',
  "AAABBHWS" decimal(5,2) NOT NULL default '0.00',
  "AAABBHUY" varchar(1) NOT NULL default ' ',
  "AAABBHUZ" decimal(3,0) NOT NULL default '0',
  "AAABBHVA" varchar(2) NOT NULL default '00',
  "AAABBHVB" varchar(1) NOT NULL default ' ',
  "AAABBHVC" varchar(4) NOT NULL default '0000',
  "AAABBHVD" decimal(15,2) NOT NULL default '0.00',
  "AAABBHVE" decimal(5,2) NOT NULL default '0.00',
  "AAABBHVF" decimal(15,2) NOT NULL default '0.00',
  "AAABBHVG" varchar(1) NOT NULL default ' ',
  "AAABBHVH" decimal(5,2) NOT NULL default '0.00',
  "AAABBHVI" decimal(15,2) NOT NULL default '0.00',
  "AAABBHVJ" varchar(1) NOT NULL default ' ',
  "AAABBHVK" varchar(4) NOT NULL default ' ',
  "AAABBHXM" decimal(5,2) NOT NULL default '0.00',
  "AAABBHVM" decimal(15,2) NOT NULL default '0.00',
  "AAABBHVN" varchar(1) NOT NULL default ' ',
  "AAABBHXP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWG","AAABBHVT","AAABBHVU","AAABBHVV","AAABBIAU","AAABBHVX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABBHWL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHWG" varchar(4) NOT NULL default ' ',
  "AAABBIAD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBICN","AAABBHWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHXP" varchar(4) NOT NULL default ' ',
  "AAABBHWR" varchar(2) NOT NULL default '00',
  "AAABBHWS" decimal(5,2) NOT NULL default '0.00',
  "AAABBHWT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHXP" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBHXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBHXP" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBHXM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBHXP","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABMBXO" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIAD" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBIAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIAD" varchar(4) NOT NULL default ' ',
  "AAABMBXO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBIAD","AAABMBXO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBICP" varchar(2) NOT NULL default ' ',
  "AAABEUQG" varchar(8) NOT NULL default '00000000',
  "AAABEUQF" varchar(8) NOT NULL default '00000000',
  "AAABBHZE" varchar(4) NOT NULL default '0000',
  "AAABBHZF" varchar(4) NOT NULL default '0000',
  "AAABBHZG" decimal(15,2) NOT NULL default '0.00',
  "AAABBIBY" varchar(5) NOT NULL default ' ',
  "AAABBHZI" varchar(8) NOT NULL default '00000000',
  "AAABBHZJ" decimal(15,2) NOT NULL default '0.00',
  "AAABBHZK" decimal(15,2) NOT NULL default '0.00',
  "AAABBHZL" varchar(5) NOT NULL default ' ',
  "AAABBHZM" decimal(3,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBICM","AAABBICN","AAABJFHB","AAABBICP","AAABEUQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBHZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIAD" varchar(4) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBIAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIAU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBIAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIAU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBIAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIBL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBICM","AAABBIBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBIBL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBICM","AAABBIBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBICP" varchar(2) NOT NULL default ' ',
  "AAABBICQ" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBIBV" varchar(8) NOT NULL default '00000000',
  "AAABBIBW" varchar(10) NOT NULL default ' ',
  "AAABBIBX" decimal(15,2) NOT NULL default '0.00',
  "AAABBIBY" varchar(5) NOT NULL default ' ',
  "AAABMBXO" varchar(4) NOT NULL default ' ',
  "AAABBICA" varchar(1) NOT NULL default ' ',
  "AAABBICB" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABBICD" decimal(15,2) NOT NULL default '0.00',
  "AAABBICE" decimal(15,2) NOT NULL default '0.00',
  "AAABBICF" varchar(1) NOT NULL default ' ',
  "AAABBICG" decimal(15,2) NOT NULL default '0.00',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABBICI" decimal(15,2) NOT NULL default '0.00',
  "AAABJFGP" varchar(240) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBICM","AAABBICN","AAABJFHB","AAABBICP","AAABBICQ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBICS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEW" varchar(40) NOT NULL default ' ',
  "AAABBIHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBICY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJSEW" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJSEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSEW" varchar(40) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSEW","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABFRWH" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABBIDV" varchar(30) NOT NULL default ' ',
  "AAABBIDW" varchar(255) NOT NULL default ' ',
  "AAABBIDX" varchar(255) NOT NULL default ' ',
  "AAABBIDY" varchar(1) NOT NULL default ' ',
  "AAABBIDZ" varchar(12) NOT NULL default ' ',
  "AAABBYHH" varchar(14) NOT NULL default ' ',
  "AAABBIEB" varchar(70) NOT NULL default ' ',
  "AAABBIEC" varchar(70) NOT NULL default ' ',
  "AAABJSDF" varchar(6) NOT NULL default '000000',
  "AAABHCEC" varchar(2) NOT NULL default '00',
  "AAABBIEF" varchar(1) NOT NULL default ' ',
  "AAABBIEG" varchar(1) NOT NULL default ' ',
  "AAABBIKC" varchar(40) NOT NULL default ' ',
  "AAABBIEI" varchar(255) NOT NULL default ' ',
  "AAABBIEJ" varchar(255) NOT NULL default ' ',
  "AAABLBEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABBIES" varchar(60) NOT NULL default ' ',
  "AAABBIET" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBIFO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNHW","AAABBIFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBIFO" varchar(40) NOT NULL default ' ',
  "AAABBIFK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFNHW","AAABBIFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBILK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBILK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBILK","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIIQ" varchar(40) NOT NULL default ' ',
  "AAABBIGN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIIQ" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIIQ" varchar(40) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIIQ","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(40) NOT NULL default ' ',
  "AAABBIHL" varchar(1) NOT NULL default ' ',
  "AAABHDUG" varchar(1) NOT NULL default ' ',
  "AAABBIHN" varchar(1) NOT NULL default ' ',
  "AAABBIHO" varchar(1) NOT NULL default ' ',
  "AAABBIHP" varchar(2) NOT NULL default '00',
  "AAABBIHQ" varchar(2) NOT NULL default '00',
  "AAABBIHR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(40) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLILW" varchar(40) NOT NULL default ' ',
  "AAABBILM" varchar(40) NOT NULL default ' ',
  "AAABBIIQ" varchar(40) NOT NULL default ' ',
  "AAABJSEW" varchar(40) NOT NULL default ' ',
  "AAABGFBX" varchar(4) NOT NULL default '0000',
  "AAABBIIT" decimal(5,2) NOT NULL default '0.00',
  "AAABBIIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLILW" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIKC" varchar(40) NOT NULL default ' ',
  "AAABBIJJ" varchar(255) NOT NULL default ' ',
  "AAABGOVG" varchar(4) NOT NULL default ' ',
  "AAABBIJL" varchar(255) NOT NULL default ' ',
  "AAABGOVH" varchar(4) NOT NULL default ' ',
  "AAABBIJN" varchar(255) NOT NULL default ' ',
  "AAABBIJO" varchar(4) NOT NULL default ' ',
  "AAABBIJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIKC" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABBIJX" varchar(60) NOT NULL default ' ',
  "AAABBIJY" varchar(60) NOT NULL default ' ',
  "AAABBIJZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABBIKG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIKQ" varchar(5) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBILM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBILM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBILM" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBILM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBILF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBILM" varchar(40) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAABBILK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBILM","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBILO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBILU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLTXZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABHVYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFZUL" varchar(4) NOT NULL default ' ',
  "AAABLRTQ" varchar(4) NOT NULL default ' ',
  "AAABBIMH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABFZUL","AAABLRTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKGBL","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABBIRC" varchar(40) NOT NULL default ' ',
  "AAABMLRB" varchar(10) NOT NULL default ' ',
  "AAABBIPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBINC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABBIRC" varchar(40) NOT NULL default ' ',
  "AAABMLRB" varchar(10) NOT NULL default ' ',
  "AAABBIPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBINK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABDKAV" varchar(80) NOT NULL default ' ',
  "AAABBIOC" varchar(80) NOT NULL default ' ',
  "AAABBIOD" varchar(40) NOT NULL default ' ',
  "AAABBIOE" varchar(4) NOT NULL default '0000',
  "AAABBIOF" varchar(32) NOT NULL default ' ',
  "AAABBIOG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBINX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABDKAV" varchar(80) NOT NULL default ' ',
  "AAABBIOC" varchar(80) NOT NULL default ' ',
  "AAABBIOD" varchar(40) NOT NULL default ' ',
  "AAABBIOE" varchar(4) NOT NULL default '0000',
  "AAABBIOF" varchar(32) NOT NULL default ' ',
  "AAABBIOG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKGBL","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIQQ","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABMLRB" varchar(10) NOT NULL default ' ',
  "AAABBIPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABMLRB" varchar(10) NOT NULL default ' ',
  "AAABBIPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABBIQD" varchar(80) NOT NULL default ' ',
  "AAABBIQE" varchar(80) NOT NULL default ' ',
  "AAABBIQF" varchar(80) NOT NULL default ' ',
  "AAABBIQG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABBIQD" varchar(80) NOT NULL default ' ',
  "AAABBIQE" varchar(80) NOT NULL default ' ',
  "AAABBIQF" varchar(80) NOT NULL default ' ',
  "AAABBIQG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIQQ" varchar(40) NOT NULL default ' ',
  "AAABKGBL" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIQQ","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABBIRC" varchar(40) NOT NULL default ' ',
  "AAABBIRD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABBIRC" varchar(40) NOT NULL default ' ',
  "AAABBIRD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABBIRS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDNYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(40) NOT NULL default ' ',
  "AAABBIRS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDNYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEIS" varchar(40) NOT NULL default ' ',
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAABBISO" varchar(255) NOT NULL default ' ',
  "AAABBISP" varchar(255) NOT NULL default ' ',
  "AAABBISQ" varchar(32) NOT NULL default ' ',
  "AAABBISR" varchar(255) NOT NULL default ' ',
  "AAABBISS" varchar(32) NOT NULL default ' ',
  "AAABBIST" varchar(1) NOT NULL default ' ',
  "AAABBISU" varchar(80) NOT NULL default ' ',
  "AAABBISV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBISK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEIS" varchar(40) NOT NULL default ' ',
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAABBISO" varchar(255) NOT NULL default ' ',
  "AAABBISP" varchar(255) NOT NULL default ' ',
  "AAABBISQ" varchar(32) NOT NULL default ' ',
  "AAABBISR" varchar(255) NOT NULL default ' ',
  "AAABBISS" varchar(32) NOT NULL default ' ',
  "AAABBIST" varchar(1) NOT NULL default ' ',
  "AAABBISU" varchar(80) NOT NULL default ' ',
  "AAABBISV" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBISY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEIS" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBITG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEIS" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBITO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIVC","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBITW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABBIUS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABBIUS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIVC" varchar(40) NOT NULL default ' ',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABMLZS" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBIVC","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLITM" varchar(40) NOT NULL default ' ',
  "AAABJRGL" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(132) NOT NULL default ' ',
  "AAABLQHQ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLITM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIWC" varchar(10) NOT NULL default ' ',
  "AAABBIWY" varchar(40) NOT NULL default ' ',
  "AAABBIWE" varchar(1) NOT NULL default ' ',
  "AAABBIWF" varchar(1) NOT NULL default ' ',
  "AAABBIWG" varchar(1) NOT NULL default ' ',
  "AAABBIWH" varchar(40) NOT NULL default ' ',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIWC" varchar(10) NOT NULL default ' ',
  "AAABBIWY" varchar(40) NOT NULL default ' ',
  "AAABBIWE" varchar(1) NOT NULL default ' ',
  "AAABBIWF" varchar(1) NOT NULL default ' ',
  "AAABBIWG" varchar(1) NOT NULL default ' ',
  "AAABBIWH" varchar(40) NOT NULL default ' ',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  "AAABBIWJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIXB" varchar(2) NOT NULL default ' ',
  "AAABBIWY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEF","AAABBIXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIXB" varchar(2) NOT NULL default ' ',
  "AAABBIWY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLGEF","AAABBIXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIYP" varchar(132) NOT NULL default ' ',
  "AAABBIYQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBIYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  "AAABBIYH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBIYK" varchar(40) NOT NULL default ' ',
  "AAABBIYH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBIYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLGEF" varchar(40) NOT NULL default ' ',
  "AAABBIYP" varchar(132) NOT NULL default ' ',
  "AAABBIYQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIZI" (
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBIZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABBKAF" varchar(6) NOT NULL default '000000',
  "AAABBIZW" varchar(1) NOT NULL default ' ',
  "AAABLPGG" varchar(7) NOT NULL default ' ',
  "AAABBIZY" varchar(4) NOT NULL default ' ',
  "AAABBIZZ" varchar(4) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABBJAB" varchar(55) NOT NULL default ' ',
  "AAABBJAC" varchar(55) NOT NULL default ' ',
  "AAABBJAD" varchar(128) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABJFLS" varchar(1) NOT NULL default ' ',
  "AAABBJAG" varchar(1) NOT NULL default ' ',
  "AAABBJAH" varchar(1) NOT NULL default ' ',
  "AAABBJAI" varchar(3) NOT NULL default ' ',
  "AAABLYYV" varchar(1) NOT NULL default ' ',
  "AAABBJOT" varchar(3) NOT NULL default '000',
  "AAABBJOU" varchar(1) NOT NULL default ' ',
  "AAABBJOV" varchar(2) NOT NULL default ' ',
  "AAABBJOW" varchar(24) NOT NULL default ' ',
  "AAABBJOX" varchar(1) NOT NULL default ' ',
  "AAABBJOY" varchar(1) NOT NULL default ' ',
  "AAABBJOZ" varchar(1) NOT NULL default ' ',
  "AAABBJPA" varchar(3) NOT NULL default '000',
  "AAABBJPB" varchar(1) NOT NULL default ' ',
  "AAABBJPC" varchar(2) NOT NULL default ' ',
  "AAABBJPD" varchar(24) NOT NULL default ' ',
  "AAABBJPE" varchar(1) NOT NULL default ' ',
  "AAABBJPF" varchar(1) NOT NULL default ' ',
  "AAABBJUX" varchar(1) NOT NULL default ' ',
  "AAABBJUY" varchar(1) NOT NULL default ' ',
  "AAABBJUZ" varchar(2) NOT NULL default ' ',
  "AAABBJVH" varchar(3) NOT NULL default ' ',
  "AAABBJVI" varchar(3) NOT NULL default ' ',
  "AAABBJVJ" varchar(2) NOT NULL default ' ',
  "AAABBJVK" varchar(2) NOT NULL default ' ',
  "AAABBJVL" varchar(2) NOT NULL default ' ',
  "AAABBJVM" varchar(2) NOT NULL default ' ',
  "AAABBJVN" varchar(1) NOT NULL default ' ',
  "AAABBJVO" varchar(1) NOT NULL default ' ',
  "AAABBJVP" varchar(1) NOT NULL default ' ',
  "AAABBJVQ" varchar(1) NOT NULL default ' ',
  "AAABBJVR" varchar(1) NOT NULL default ' ',
  "AAABBJVS" varchar(1) NOT NULL default ' ',
  "AAABBJVT" varchar(1) NOT NULL default ' ',
  "AAABBJVU" varchar(2) NOT NULL default ' ',
  "AAABBJVV" varchar(2) NOT NULL default ' ',
  "AAABBJVW" varchar(3) NOT NULL default ' ',
  "AAABBJVX" varchar(1) NOT NULL default ' ',
  "AAABBJVY" varchar(1) NOT NULL default ' ',
  "AAABBJVZ" varchar(1) NOT NULL default ' ',
  "AAABBJZZ" varchar(1) NOT NULL default ' ',
  "AAABBJWB" varchar(1) NOT NULL default ' ',
  "AAABBJWC" varchar(1) NOT NULL default ' ',
  "AAABBJWD" varchar(1) NOT NULL default ' ',
  "AAABBJWE" varchar(1) NOT NULL default ' ',
  "AAABBJWF" varchar(2) NOT NULL default ' ',
  "AAABBJWG" varchar(2) NOT NULL default ' ',
  "AAABBJWH" varchar(2) NOT NULL default ' ',
  "AAABBJWI" varchar(2) NOT NULL default ' ',
  "AAABBJWJ" varchar(2) NOT NULL default ' ',
  "AAABBJWK" varchar(2) NOT NULL default ' ',
  "AAABBJWL" varchar(1) NOT NULL default ' ',
  "AAABBJWM" varchar(1) NOT NULL default ' ',
  "AAABBJWN" varchar(1) NOT NULL default ' ',
  "AAABBJWO" varchar(1) NOT NULL default ' ',
  "AAABBJWP" varchar(1) NOT NULL default ' ',
  "AAABBJWQ" varchar(1) NOT NULL default ' ',
  "AAABBJWR" varchar(1) NOT NULL default ' ',
  "AAABBJWS" varchar(1) NOT NULL default ' ',
  "AAABBJWT" varchar(1) NOT NULL default ' ',
  "AAABBJWU" varchar(1) NOT NULL default ' ',
  "AAABBJWV" varchar(1) NOT NULL default ' ',
  "AAABBJWW" varchar(1) NOT NULL default ' ',
  "AAABBJWX" varchar(1) NOT NULL default ' ',
  "AAABBOBP" varchar(1) NOT NULL default ' ',
  "AAABBOBS" varchar(1) NOT NULL default ' ',
  "AAABBOBV" varchar(1) NOT NULL default ' ',
  "AAABBJXB" varchar(1) NOT NULL default ' ',
  "AAABBJXC" varchar(1) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default ' ',
  "AAABBJXE" varchar(1) NOT NULL default ' ',
  "AAABBJXF" varchar(1) NOT NULL default ' ',
  "AAABBJXG" varchar(1) NOT NULL default ' ',
  "AAABBJXH" varchar(1) NOT NULL default ' ',
  "AAABBJXI" varchar(1) NOT NULL default ' ',
  "AAABBJXJ" varchar(1) NOT NULL default ' ',
  "AAABBJXK" varchar(1) NOT NULL default ' ',
  "AAABBJXL" varchar(1) NOT NULL default ' ',
  "AAABBJXM" varchar(1) NOT NULL default ' ',
  "AAABBJXN" varchar(1) NOT NULL default ' ',
  "AAABBJXO" varchar(1) NOT NULL default ' ',
  "AAABBJXP" varchar(1) NOT NULL default ' ',
  "AAABIGAY" varchar(1) NOT NULL default ' ',
  "AAABBJXR" varchar(1) NOT NULL default ' ',
  "AAABBJXS" varchar(1) NOT NULL default ' ',
  "AAABBJXT" varchar(1) NOT NULL default ' ',
  "AAABBJXU" varchar(2) NOT NULL default ' ',
  "AAABBJXV" varchar(1) NOT NULL default ' ',
  "AAABBJXW" varchar(2) NOT NULL default ' ',
  "AAABBJXX" varchar(2) NOT NULL default ' ',
  "AAABBJXY" varchar(1) NOT NULL default ' ',
  "AAABBJXZ" varchar(3) NOT NULL default ' ',
  "AAABBJYA" varchar(2) NOT NULL default ' ',
  "AAABBJYB" varchar(2) NOT NULL default ' ',
  "AAABBJYC" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(1) NOT NULL default ' ',
  "AAABBJYE" varchar(1) NOT NULL default ' ',
  "AAABBJYF" varchar(1) NOT NULL default ' ',
  "AAABBJYG" varchar(1) NOT NULL default ' ',
  "AAABBJYH" varchar(1) NOT NULL default ' ',
  "AAABBJYI" varchar(1) NOT NULL default ' ',
  "AAABBJEC" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABBJEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJEI" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABGNDC" varchar(3) NOT NULL default '000',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABBVTY" varchar(1) NOT NULL default ' ',
  "AAABBJES" varchar(1) NOT NULL default ' ',
  "AAABBJET" varchar(30) NOT NULL default ' ',
  "AAABBJEU" varchar(30) NOT NULL default ' ',
  "AAABDGIU" varchar(30) NOT NULL default ' ',
  "AAABBJEW" varchar(7) NOT NULL default ' ',
  "AAABBJEX" varchar(30) NOT NULL default ' ',
  "AAABBJEY" varchar(30) NOT NULL default ' ',
  "AAABBJEZ" varchar(30) NOT NULL default ' ',
  "AAABBJFA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABIASH" varchar(3) NOT NULL default ' ',
  "AAABLTQC" varchar(3) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABBJFF" varchar(1) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABBJFH" varchar(30) NOT NULL default ' ',
  "AAABBJFI" varchar(30) NOT NULL default ' ',
  "AAABBJFJ" varchar(30) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default ' ',
  "AAABBJFL" varchar(1) NOT NULL default ' ',
  "AAABBJFM" varchar(30) NOT NULL default ' ',
  "AAABBJFN" varchar(30) NOT NULL default ' ',
  "AAABBJFO" varchar(30) NOT NULL default ' ',
  "AAABBORQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJFR" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBJFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJFV" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBJFX" varchar(30) NOT NULL default ' ',
  "AAABBJFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJGA" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBOTQ" varchar(3) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABBJGE" varchar(2) NOT NULL default ' ',
  "AAABBJGF" varchar(1) NOT NULL default ' ',
  "AAABBLCF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ","AAABBOTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJGJ" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABBJHM" varchar(1) NOT NULL default ' ',
  "AAABBJHN" varchar(1) NOT NULL default ' ',
  "AAABBJHO" varchar(30) NOT NULL default ' ',
  "AAABBJHP" varchar(30) NOT NULL default ' ',
  "AAABBJHQ" varchar(30) NOT NULL default ' ',
  "AAABBJHR" varchar(30) NOT NULL default ' ',
  "AAABBJHS" varchar(30) NOT NULL default ' ',
  "AAABBJHT" varchar(30) NOT NULL default ' ',
  "AAABBJHU" varchar(1) NOT NULL default ' ',
  "AAABBJHV" varchar(1) NOT NULL default ' ',
  "AAABBJHB" varchar(30) NOT NULL default ' ',
  "AAABBJHC" varchar(30) NOT NULL default ' ',
  "AAABBJHD" varchar(30) NOT NULL default ' ',
  "AAABENFL" varchar(30) NOT NULL default ' ',
  "AAABBJHF" varchar(30) NOT NULL default ' ',
  "AAABBJHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJHK" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABBJHM" varchar(1) NOT NULL default ' ',
  "AAABBJHN" varchar(1) NOT NULL default ' ',
  "AAABBJHO" varchar(30) NOT NULL default ' ',
  "AAABBJHP" varchar(30) NOT NULL default ' ',
  "AAABBJHQ" varchar(30) NOT NULL default ' ',
  "AAABBJHR" varchar(30) NOT NULL default ' ',
  "AAABBJHS" varchar(30) NOT NULL default ' ',
  "AAABBJHT" varchar(30) NOT NULL default ' ',
  "AAABBJHU" varchar(1) NOT NULL default ' ',
  "AAABBJHV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJHX" (
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABERMO","AAABERMQ","AAABERMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJII" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBJIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJIM" (
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABBPIP" varchar(4) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABERMO","AAABERMP","AAABERMQ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJIY" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABBJJC" varchar(40) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(10) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABBJJO" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABBJJR" varchar(12) NOT NULL default ' ',
  "AAABBJJS" varchar(8) NOT NULL default '00000000',
  "AAABMMCV" varchar(10) NOT NULL default ' ',
  "AAABBJJU" varchar(1) NOT NULL default ' ',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  "AAABBJJW" varchar(1) NOT NULL default ' ',
  "AAABBJJX" varchar(24) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABBJKE" varchar(4) NOT NULL default ' ',
  "AAABBJKF" varchar(8) NOT NULL default '00000000',
  "AAABBJKG" varchar(1) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABGHTH" varchar(6) NOT NULL default '000000',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABBJKK" int(11) NOT NULL default '0',
  "AAABBJKL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLPUZ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHUWW","AAABHDEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(10) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAABMECK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKIQ","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(10) NOT NULL default ' ',
  "AAABIWTO" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHUWW","AAABHDEU","AAABLQVJ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLENB" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABHLPB" varchar(3) NOT NULL default '000',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBJMI" varchar(2) NOT NULL default ' ',
  "AAABBJMJ" varchar(2) NOT NULL default ' ',
  "AAABBJMK" varchar(2) NOT NULL default ' ',
  "AAABBJML" varchar(2) NOT NULL default ' ',
  "AAABBJMM" varchar(2) NOT NULL default ' ',
  "AAABBJMN" varchar(1) NOT NULL default ' ',
  "AAABBJMO" varchar(1) NOT NULL default ' ',
  "AAABBJMP" varchar(2) NOT NULL default ' ',
  "AAABBJMQ" varchar(2) NOT NULL default ' ',
  "AAABBJMR" varchar(1) NOT NULL default ' ',
  "AAABBJMS" varchar(1) NOT NULL default ' ',
  "AAABBJMT" varchar(1) NOT NULL default ' ',
  "AAABBJMU" varchar(1) NOT NULL default ' ',
  "AAABBJMV" varchar(2) NOT NULL default ' ',
  "AAABJLPR" varchar(1) NOT NULL default ' ',
  "AAABBJNO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLENB","AAABLQVJ","AAABHLPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBJNL" varchar(3) NOT NULL default ' ',
  "AAABBJNM" varchar(2) NOT NULL default '00',
  "AAABBJNN" varchar(1) NOT NULL default ' ',
  "AAABBJNO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLENB" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBJOF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLENB","AAABLQVJ","AAABBJOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBJOL" varchar(1) NOT NULL default ' ',
  "AAABFRGT" varchar(2) NOT NULL default ' ',
  "AAABBKDN" varchar(1) NOT NULL default ' ',
  "AAABBKDO" varchar(1) NOT NULL default ' ',
  "AAABBJOP" varchar(1) NOT NULL default ' ',
  "AAABBJOQ" varchar(1) NOT NULL default ' ',
  "AAABBJOR" varchar(1) NOT NULL default ' ',
  "AAABLYYV" varchar(1) NOT NULL default ' ',
  "AAABBJOT" varchar(3) NOT NULL default '000',
  "AAABBJOU" varchar(1) NOT NULL default ' ',
  "AAABBJOV" varchar(2) NOT NULL default ' ',
  "AAABBJOW" varchar(24) NOT NULL default ' ',
  "AAABBJOX" varchar(1) NOT NULL default ' ',
  "AAABBJOY" varchar(1) NOT NULL default ' ',
  "AAABBJOZ" varchar(1) NOT NULL default ' ',
  "AAABBJPA" varchar(3) NOT NULL default '000',
  "AAABBJPB" varchar(1) NOT NULL default ' ',
  "AAABBJPC" varchar(2) NOT NULL default ' ',
  "AAABBJPD" varchar(24) NOT NULL default ' ',
  "AAABBJPE" varchar(1) NOT NULL default ' ',
  "AAABBJPF" varchar(1) NOT NULL default ' ',
  "AAABBKDU" varchar(1) NOT NULL default ' ',
  "AAABBKDV" varchar(3) NOT NULL default '000',
  "AAABBJPI" varchar(2) NOT NULL default ' ',
  "AAABBJPJ" varchar(1) NOT NULL default ' ',
  "AAABBJXP" varchar(1) NOT NULL default ' ',
  "AAABIGAY" varchar(1) NOT NULL default ' ',
  "AAABBJXR" varchar(1) NOT NULL default ' ',
  "AAABBJXS" varchar(1) NOT NULL default ' ',
  "AAABBJXT" varchar(1) NOT NULL default ' ',
  "AAABBJXU" varchar(2) NOT NULL default ' ',
  "AAABBJXV" varchar(1) NOT NULL default ' ',
  "AAABBJXW" varchar(2) NOT NULL default ' ',
  "AAABBJXX" varchar(2) NOT NULL default ' ',
  "AAABBJXY" varchar(1) NOT NULL default ' ',
  "AAABBJXZ" varchar(3) NOT NULL default ' ',
  "AAABBJYA" varchar(2) NOT NULL default ' ',
  "AAABBJYB" varchar(2) NOT NULL default ' ',
  "AAABBJYC" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(1) NOT NULL default ' ',
  "AAABBJYE" varchar(1) NOT NULL default ' ',
  "AAABBJYF" varchar(1) NOT NULL default ' ',
  "AAABBJYG" varchar(1) NOT NULL default ' ',
  "AAABBJYH" varchar(1) NOT NULL default ' ',
  "AAABBJYI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABBKFL" varchar(8) NOT NULL default ' ',
  "AAABBJQM" varchar(3) NOT NULL default '000',
  "AAABBJQN" varchar(3) NOT NULL default '000',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABBJQS" varchar(3) NOT NULL default '000',
  "AAABKIXS" varchar(3) NOT NULL default '000',
  "AAABHKGL" varchar(6) NOT NULL default '000000',
  "AAABHVIE" varchar(6) NOT NULL default '000000',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABBKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABBJRI" varchar(2) NOT NULL default ' ',
  "AAABBJRJ" varchar(4) NOT NULL default ' ',
  "AAABBJRK" varchar(8) NOT NULL default ' ',
  "AAABBJRL" varchar(12) NOT NULL default ' ',
  "AAABBJRM" varchar(4) NOT NULL default ' ',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABBKAF" varchar(6) NOT NULL default '000000',
  "AAABBJRP" varchar(40) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABJCFN" varchar(20) NOT NULL default ' ',
  "AAABBJRS" varchar(24) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABBJSA" varchar(1) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABFYRN" varchar(34) NOT NULL default ' ',
  "AAABBJTK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLENB" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABFRGT" varchar(2) NOT NULL default ' ',
  "AAABBJSQ" varchar(1) NOT NULL default ' ',
  "AAABBJSR" varchar(3) NOT NULL default '000',
  "AAABBJYA" varchar(2) NOT NULL default ' ',
  "AAABBJYB" varchar(2) NOT NULL default ' ',
  "AAABBKDT" varchar(1) NOT NULL default ' ',
  "AAABBJSV" varchar(3) NOT NULL default '000',
  "AAABBKDU" varchar(1) NOT NULL default ' ',
  "AAABBKDV" varchar(3) NOT NULL default '000',
  "AAABBJSY" varchar(1) NOT NULL default ' ',
  "AAABBKDN" varchar(1) NOT NULL default ' ',
  "AAABBKDO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLENB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABBJTK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBKDF" varchar(8) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABBJUI" varchar(3) NOT NULL default '000',
  "AAABBJTW" varchar(34) NOT NULL default ' ',
  "AAABBJTX" varchar(8) NOT NULL default ' ',
  "AAABBJTY" varchar(34) NOT NULL default ' ',
  "AAABBJTZ" varchar(8) NOT NULL default ' ',
  "AAABBJUA" varchar(3) NOT NULL default ' ',
  "AAABBJUB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLQVJ","AAABBKDF","AAABFLEJ","AAABBJUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABBKFL" varchar(8) NOT NULL default ' ',
  "AAABKZAW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHUWW","AAABBKFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(7) NOT NULL default ' ',
  "AAABBJUX" varchar(1) NOT NULL default ' ',
  "AAABBJUY" varchar(1) NOT NULL default ' ',
  "AAABBJUZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHFFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFFQ" varchar(7) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABBJVH" varchar(3) NOT NULL default ' ',
  "AAABBJVI" varchar(3) NOT NULL default ' ',
  "AAABBJVJ" varchar(2) NOT NULL default ' ',
  "AAABBJVK" varchar(2) NOT NULL default ' ',
  "AAABBJVL" varchar(2) NOT NULL default ' ',
  "AAABBJVM" varchar(2) NOT NULL default ' ',
  "AAABBJVN" varchar(1) NOT NULL default ' ',
  "AAABBJVO" varchar(1) NOT NULL default ' ',
  "AAABBJVP" varchar(1) NOT NULL default ' ',
  "AAABBJVQ" varchar(1) NOT NULL default ' ',
  "AAABBJVR" varchar(1) NOT NULL default ' ',
  "AAABBJVS" varchar(1) NOT NULL default ' ',
  "AAABBJVT" varchar(1) NOT NULL default ' ',
  "AAABBJVU" varchar(2) NOT NULL default ' ',
  "AAABBJVV" varchar(2) NOT NULL default ' ',
  "AAABBJVW" varchar(3) NOT NULL default ' ',
  "AAABBJVX" varchar(1) NOT NULL default ' ',
  "AAABBJVY" varchar(1) NOT NULL default ' ',
  "AAABBJVZ" varchar(1) NOT NULL default ' ',
  "AAABBJZZ" varchar(1) NOT NULL default ' ',
  "AAABBJWB" varchar(1) NOT NULL default ' ',
  "AAABBJWC" varchar(1) NOT NULL default ' ',
  "AAABBJWD" varchar(1) NOT NULL default ' ',
  "AAABBJWE" varchar(1) NOT NULL default ' ',
  "AAABBJWF" varchar(2) NOT NULL default ' ',
  "AAABBJWG" varchar(2) NOT NULL default ' ',
  "AAABBJWH" varchar(2) NOT NULL default ' ',
  "AAABBJWI" varchar(2) NOT NULL default ' ',
  "AAABBJWJ" varchar(2) NOT NULL default ' ',
  "AAABBJWK" varchar(2) NOT NULL default ' ',
  "AAABBJWL" varchar(1) NOT NULL default ' ',
  "AAABBJWM" varchar(1) NOT NULL default ' ',
  "AAABBJWN" varchar(1) NOT NULL default ' ',
  "AAABBJWO" varchar(1) NOT NULL default ' ',
  "AAABBJWP" varchar(1) NOT NULL default ' ',
  "AAABBJWQ" varchar(1) NOT NULL default ' ',
  "AAABBJWR" varchar(1) NOT NULL default ' ',
  "AAABBJWS" varchar(1) NOT NULL default ' ',
  "AAABBJWT" varchar(1) NOT NULL default ' ',
  "AAABBJWU" varchar(1) NOT NULL default ' ',
  "AAABBJWV" varchar(1) NOT NULL default ' ',
  "AAABBJWW" varchar(1) NOT NULL default ' ',
  "AAABBJWX" varchar(1) NOT NULL default ' ',
  "AAABBOBP" varchar(1) NOT NULL default ' ',
  "AAABBOBS" varchar(1) NOT NULL default ' ',
  "AAABBOBV" varchar(1) NOT NULL default ' ',
  "AAABBJXB" varchar(1) NOT NULL default ' ',
  "AAABBJXC" varchar(1) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default ' ',
  "AAABBJXE" varchar(1) NOT NULL default ' ',
  "AAABBJXF" varchar(1) NOT NULL default ' ',
  "AAABBJXG" varchar(1) NOT NULL default ' ',
  "AAABBJXH" varchar(1) NOT NULL default ' ',
  "AAABBJXI" varchar(1) NOT NULL default ' ',
  "AAABBJXJ" varchar(1) NOT NULL default ' ',
  "AAABBJXK" varchar(1) NOT NULL default ' ',
  "AAABBJXL" varchar(1) NOT NULL default ' ',
  "AAABBJXM" varchar(1) NOT NULL default ' ',
  "AAABBJXN" varchar(1) NOT NULL default ' ',
  "AAABBJXO" varchar(1) NOT NULL default ' ',
  "AAABBJXP" varchar(1) NOT NULL default ' ',
  "AAABIGAY" varchar(1) NOT NULL default ' ',
  "AAABBJXR" varchar(1) NOT NULL default ' ',
  "AAABBJXS" varchar(1) NOT NULL default ' ',
  "AAABBJXT" varchar(1) NOT NULL default ' ',
  "AAABBJXU" varchar(2) NOT NULL default ' ',
  "AAABBJXV" varchar(1) NOT NULL default ' ',
  "AAABBJXW" varchar(2) NOT NULL default ' ',
  "AAABBJXX" varchar(2) NOT NULL default ' ',
  "AAABBJXY" varchar(1) NOT NULL default ' ',
  "AAABBJXZ" varchar(3) NOT NULL default ' ',
  "AAABBJYA" varchar(2) NOT NULL default ' ',
  "AAABBJYB" varchar(2) NOT NULL default ' ',
  "AAABBJYC" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(1) NOT NULL default ' ',
  "AAABBJYE" varchar(1) NOT NULL default ' ',
  "AAABBJYF" varchar(1) NOT NULL default ' ',
  "AAABBJYG" varchar(1) NOT NULL default ' ',
  "AAABBJYH" varchar(1) NOT NULL default ' ',
  "AAABBJYI" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABBKAF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABHFFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBJZR" varchar(8) NOT NULL default ' ',
  "AAABHGHK" varchar(30) NOT NULL default ' ',
  "AAABBJZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBJZR","AAABHGHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBJZR" varchar(8) NOT NULL default ' ',
  "AAABHGHK" varchar(30) NOT NULL default ' ',
  "AAABBJZL" varchar(30) NOT NULL default ' ',
  "AAABBJZF" varchar(1) NOT NULL default ' ',
  "AAABBJZG" varchar(1) NOT NULL default ' ',
  "AAABBJZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBJZR","AAABHGHK","AAABBJZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBJZR" varchar(8) NOT NULL default ' ',
  "AAABHGHK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBJZR","AAABHGHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBJZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBKCD" varchar(8) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(24) NOT NULL default ' ',
  "AAABBJZZ" varchar(1) NOT NULL default ' ',
  "AAABLKQL" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABBKAF" varchar(6) NOT NULL default '000000',
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABBKAI" varchar(1) NOT NULL default ' ',
  "AAABBKAJ" varchar(1) NOT NULL default ' ',
  "AAABBKAK" varchar(250) NOT NULL default ' ',
  "AAABBKAZ" varchar(34) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABBKAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBKCD"),
  KEY "AAABBKAR" ("AAABMKIQ","AAABMRIL"),
  KEY "AAABBKAU" ("AAABMRIL","AAABHUGE"),
  KEY "AAABBKAX" ("AAABMRIL","AAABBKAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  "AAABIMZU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIAEU","AAABLQVJ","AAABIMZU"),
  KEY "AAABBKBJ" ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBKBR" varchar(8) NOT NULL default ' ',
  "AAABIMZU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBKBR","AAABIMZU"),
  KEY "AAABBKBT" ("AAABMRIL","AAABIMZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBKCD" varchar(8) NOT NULL default ' ',
  "AAABMECK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBKCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABMMRH" varchar(3) NOT NULL default '000',
  "AAABBKCR" varchar(3) NOT NULL default '000',
  "AAABBKDQ" varchar(254) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLQVJ","AAABMMRH","AAABBKCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABBKDF" varchar(8) NOT NULL default ' ',
  "AAABLENB" varchar(1) NOT NULL default ' ',
  "AAABBKDH" varchar(3) NOT NULL default '000',
  "AAABJVQM" varchar(34) NOT NULL default ' ',
  "AAABBKDA" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABBKDF","AAABLENB","AAABBKDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBKDN" varchar(1) NOT NULL default ' ',
  "AAABBKDO" varchar(1) NOT NULL default ' ',
  "AAABJSEW" varchar(1) NOT NULL default ' ',
  "AAABBKDQ" varchar(254) NOT NULL default ' ',
  "AAABBKDR" varchar(1) NOT NULL default ' ',
  "AAABBKDS" varchar(3) NOT NULL default '000',
  "AAABBKDT" varchar(1) NOT NULL default ' ',
  "AAABBKDU" varchar(1) NOT NULL default ' ',
  "AAABBKDV" varchar(3) NOT NULL default '000',
  "AAABBKDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABHDEU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABHKGM" varchar(12) NOT NULL default ' ',
  "AAABHKGK" varchar(8) NOT NULL default '00000000',
  "AAABBKEH" varchar(12) NOT NULL default ' ',
  "AAABBKEI" varchar(8) NOT NULL default '00000000',
  "AAABHGFL" varchar(8) NOT NULL default '00000000',
  "AAABBKGR" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABHKGL" varchar(6) NOT NULL default '000000',
  "AAABBKEN" varchar(6) NOT NULL default '000000',
  "AAABHGFM" varchar(6) NOT NULL default '000000',
  "AAABDKZZ" varchar(24) NOT NULL default ' ',
  "AAABBKEQ" varchar(1) NOT NULL default ' ',
  "AAABBKER" varchar(1) NOT NULL default ' ',
  "AAABBKES" varchar(4) NOT NULL default ' ',
  "AAABBKGS" varchar(4) NOT NULL default ' ',
  "AAABBKEU" varchar(1) NOT NULL default ' ',
  "AAABBKEV" varchar(1) NOT NULL default ' ',
  "AAABBKEW" varchar(1) NOT NULL default ' ',
  "AAABBKEX" varchar(1) NOT NULL default ' ',
  "AAABBKEY" varchar(1) NOT NULL default ' ',
  "AAABILXZ" varchar(8) NOT NULL default '00000000',
  "AAABILYA" varchar(6) NOT NULL default '000000',
  "AAABBKFB" varchar(12) NOT NULL default ' ',
  "AAABBKFC" int(11) NOT NULL default '0',
  "AAABBKFD" varchar(8) NOT NULL default '00000000',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFASV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABBKFL" varchar(8) NOT NULL default ' ',
  "AAABHDEU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFASV","AAABLQVJ"),
  KEY "AAABBKFQ" ("AAABMRIL","AAABHDEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABBUWX" varchar(35) NOT NULL default ' ',
  "AAABBKFY" varchar(35) NOT NULL default ' ',
  "AAABBKFZ" varchar(35) NOT NULL default ' ',
  "AAABBKGA" varchar(35) NOT NULL default ' ',
  "AAABBKGB" varchar(35) NOT NULL default ' ',
  "AAABBKGC" varchar(35) NOT NULL default ' ',
  "AAABBKGD" varchar(35) NOT NULL default ' ',
  "AAABBKGE" varchar(35) NOT NULL default ' ',
  "AAABBKGF" varchar(35) NOT NULL default ' ',
  "AAABBKGG" varchar(35) NOT NULL default ' ',
  "AAABBKGH" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFASV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(8) NOT NULL default '00000000',
  "AAABBKGP" decimal(15,0) NOT NULL default '0',
  "AAABBKGQ" decimal(15,0) NOT NULL default '0',
  "AAABBKGR" varchar(12) NOT NULL default ' ',
  "AAABBKGS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFASV","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(8) NOT NULL default '00000000',
  "AAABBKHQ" varchar(20) NOT NULL default ' ',
  "AAABICEE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFASV","AAABKRXK","AAABBKHQ"),
  UNIQUE KEY "AAABBKHG" ("AAABICEE","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABBKHQ" varchar(20) NOT NULL default ' ',
  "AAABICEE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFASV","AAABBKHQ"),
  UNIQUE KEY "AAABBKHR" ("AAABICEE","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKHU" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBKHW" varchar(30) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABLUMI" varchar(1) NOT NULL default ' ',
  "AAABBKHZ" varchar(30) NOT NULL default ' ',
  "AAABBKIA" varchar(30) NOT NULL default ' ',
  "AAABBKIB" varchar(30) NOT NULL default ' ',
  "AAABBKIC" varchar(30) NOT NULL default ' ',
  "AAABBKID" varchar(30) NOT NULL default ' ',
  "AAABBKIE" varchar(30) NOT NULL default ' ',
  "AAABBLFD" varchar(30) NOT NULL default ' ',
  "AAABBKIG" varchar(1) NOT NULL default ' ',
  "AAABBKIH" varchar(30) NOT NULL default ' ',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAABBKIJ" varchar(30) NOT NULL default ' ',
  "AAABBKIK" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABBKMR" varchar(30) NOT NULL default ' ',
  "AAABKGXT" varchar(30) NOT NULL default ' ',
  "AAABBKIO" varchar(30) NOT NULL default ' ',
  "AAABBKIP" varchar(3) NOT NULL default '000',
  "AAABBKIQ" varchar(3) NOT NULL default '000',
  "AAABBKIR" varchar(1) NOT NULL default ' ',
  "AAABBKIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKIU" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABBLBS" varchar(30) NOT NULL default ' ',
  "AAABBLBT" varchar(30) NOT NULL default ' ',
  "AAABBLBU" varchar(30) NOT NULL default ' ',
  "AAABBLBV" varchar(30) NOT NULL default ' ',
  "AAABBLBW" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default '0',
  "AAABBLBY" varchar(3) NOT NULL default '000',
  "AAABBLBZ" varchar(3) NOT NULL default ' ',
  "AAABBLCA" varchar(3) NOT NULL default '000',
  "AAABBLCB" varchar(30) NOT NULL default ' ',
  "AAABLKWV" varchar(1) NOT NULL default '0',
  "AAABBNSU" varchar(30) NOT NULL default ' ',
  "AAABBLCE" varchar(24) NOT NULL default ' ',
  "AAABBLCF" varchar(30) NOT NULL default ' ',
  "AAABBLCG" varchar(30) NOT NULL default ' ',
  "AAABBLCH" varchar(3) NOT NULL default '000',
  "AAABBLCI" varchar(1) NOT NULL default ' ',
  "AAABBLCJ" varchar(2) NOT NULL default '00',
  "AAABBLCK" varchar(1) NOT NULL default ' ',
  "AAABBLCL" varchar(2) NOT NULL default '00',
  "AAABBLCM" varchar(1) NOT NULL default ' ',
  "AAABBLCN" varchar(2) NOT NULL default '00',
  "AAABBLCO" varchar(10) NOT NULL default ' ',
  "AAABBLCP" varchar(10) NOT NULL default ' ',
  "AAABBLCQ" varchar(10) NOT NULL default ' ',
  "AAABBLCR" varchar(30) NOT NULL default ' ',
  "AAABBLCS" varchar(30) NOT NULL default ' ',
  "AAABBLCT" varchar(30) NOT NULL default ' ',
  "AAABBLCU" varchar(30) NOT NULL default ' ',
  "AAABBLCV" varchar(30) NOT NULL default ' ',
  "AAABBLCW" varchar(1) NOT NULL default ' ',
  "AAABBLCX" varchar(2) NOT NULL default '00',
  "AAABBLCY" varchar(30) NOT NULL default ' ',
  "AAABBLCZ" varchar(1) NOT NULL default ' ',
  "AAABMNMM" varchar(1) NOT NULL default ' ',
  "AAABBLDB" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABBLDD" varchar(1) NOT NULL default ' ',
  "AAABBKKJ" varchar(30) NOT NULL default ' ',
  "AAABBKKK" varchar(30) NOT NULL default ' ',
  "AAABBKKL" varchar(30) NOT NULL default ' ',
  "AAABBKKM" varchar(30) NOT NULL default ' ',
  "AAABBKKN" varchar(30) NOT NULL default ' ',
  "AAABBKKO" varchar(61) NOT NULL default ' ',
  "AAABBKKP" varchar(61) NOT NULL default ' ',
  "AAABBKKQ" varchar(61) NOT NULL default ' ',
  "AAABBKKR" varchar(20) NOT NULL default ' ',
  "AAABBKKS" varchar(20) NOT NULL default ' ',
  "AAABBKKT" varchar(20) NOT NULL default ' ',
  "AAABBKKU" varchar(1) NOT NULL default ' ',
  "AAABBKKV" varchar(1) NOT NULL default ' ',
  "AAABBKKW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKKZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABBKLE" varchar(30) NOT NULL default ' ',
  "AAABBKLF" varchar(30) NOT NULL default ' ',
  "AAABBKLG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKIQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKLK" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBKMH" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABBKLO" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default '0',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABBKLS" varchar(10) NOT NULL default ' ',
  "AAABCLKG" varchar(4) NOT NULL default '0000',
  "AAABBKLU" varchar(30) NOT NULL default ' ',
  "AAABEJLD" varchar(24) NOT NULL default ' ',
  "AAABBKLW" varchar(1) NOT NULL default ' ',
  "AAABBKLX" varchar(30) NOT NULL default ' ',
  "AAABBKLY" varchar(2) NOT NULL default '00',
  "AAABBKLZ" varchar(30) NOT NULL default ' ',
  "AAABBKMA" varchar(30) NOT NULL default ' ',
  "AAABBKMB" varchar(30) NOT NULL default ' ',
  "AAABBKMC" varchar(30) NOT NULL default ' ',
  "AAABBKMD" varchar(30) NOT NULL default ' ',
  "AAABBKME" varchar(1) NOT NULL default ' ',
  "AAABBKMF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ","AAABBKMH","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKMJ" (
  "AAABFTXC" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBKMN" varchar(30) NOT NULL default ' ',
  "AAABCLKG" varchar(4) NOT NULL default '0000',
  "AAABBKMP" varchar(1) NOT NULL default ' ',
  "AAABBKMQ" varchar(1) NOT NULL default ' ',
  "AAABBKMR" varchar(30) NOT NULL default ' ',
  "AAABBKMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTXC","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBKOJ" varchar(24) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABBKNL" varchar(34) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBKOJ"),
  KEY "AAABBKNG" ("AAABBKNL","AAABMRIL"),
  KEY "AAABBKNJ" ("AAABMRIL","AAABBKNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBKNT" varchar(34) NOT NULL default ' ',
  "AAABLQVJ" int(11) NOT NULL default '0',
  "AAABBKNX" varchar(30) NOT NULL default ' ',
  "AAABBKOJ" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBKNT","AAABLQVJ"),
  KEY "AAABBKNV" ("AAABMRIL","AAABBKNX","AAABBKOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBKOJ" varchar(24) NOT NULL default ' ',
  "AAABBKOE" varchar(30) NOT NULL default ' ',
  "AAABBKOF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKIQ","AAABBKOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKOK" (
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABBKOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIKF","AAABJLPG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKOS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKOV" (
  "AAABFTXC" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTXC","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKPC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  "AAABBKQG" varchar(34) NOT NULL default ' ',
  "AAABFUAM" varchar(11) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  "AAABBKPQ" decimal(6,3) NOT NULL default '0.000',
  "AAABBKPR" decimal(6,3) NOT NULL default '0.000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABBKPU" varchar(30) NOT NULL default ' ',
  "AAABBLWE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABLQVJ"),
  KEY "AAABBKPZ" ("AAABMRIL","AAABBKQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABEUXC" varchar(30) NOT NULL default ' ',
  "AAABBKQG" varchar(34) NOT NULL default ' ',
  "AAABBKQH" varchar(30) NOT NULL default ' ',
  "AAABEBTF" varchar(30) NOT NULL default ' ',
  "AAABBKQJ" varchar(34) NOT NULL default ' ',
  "AAABBKQK" varchar(11) NOT NULL default ' ',
  "AAABBPDH" varchar(1) NOT NULL default ' ',
  "AAABBKQM" varchar(1) NOT NULL default ' ',
  "AAABBKQN" varchar(1) NOT NULL default ' ',
  "AAABBKQO" varchar(1) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABFUGI" varchar(12) NOT NULL default ' ',
  "AAABFUGJ" varchar(8) NOT NULL default '00000000',
  "AAABBKQW" varchar(3) NOT NULL default ' ',
  "AAABBKQX" varchar(24) NOT NULL default ' ',
  "AAABBKQY" varchar(1) NOT NULL default ' ',
  "AAABBKQZ" varchar(1) NOT NULL default ' ',
  "AAABBKRA" varchar(1) NOT NULL default ' ',
  "AAABMKCG" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" varchar(6) NOT NULL default '000000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABBKRR" varchar(10) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABETOU","AAABLRNO","AAABMPVB","AAABMQCJ","AAABMQHW","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFUKI","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABBKTG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKSZ" (
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKTD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABBKTG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABBKTP" varchar(1) NOT NULL default ' ',
  "AAABBKTQ" varchar(1) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABKQYU" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABBLYR" varchar(8) NOT NULL default '00000000',
  "AAABBLYS" varchar(12) NOT NULL default ' ',
  "AAABBLYT" varchar(6) NOT NULL default '000000',
  "AAABBKTY" varchar(1) NOT NULL default ' ',
  "AAABLETR" varchar(1) NOT NULL default ' ',
  "AAABBKUA" varchar(12) NOT NULL default ' ',
  "AAABBLOM" varchar(1) NOT NULL default '0',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABJURT" varchar(4) NOT NULL default ' ',
  "AAABBKUF" varchar(1) NOT NULL default ' ',
  "AAABBKUG" varchar(1) NOT NULL default ' ',
  "AAABBKUH" varchar(1) NOT NULL default ' ',
  "AAABBLNA" varchar(4) NOT NULL default ' ',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  "AAABBKUK" varchar(1) NOT NULL default ' ',
  "AAABFKWA" varchar(1) NOT NULL default ' ',
  "AAABBKUM" varchar(1) NOT NULL default ' ',
  "AAABBKUN" varchar(1) NOT NULL default ' ',
  "AAABBKUO" varchar(1) NOT NULL default ' ',
  "AAABBKUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABBLZZ" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFSUG" varchar(10) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABBLZO" varchar(30) NOT NULL default ' ',
  "AAABFJYT" varchar(3) NOT NULL default '000',
  "AAABBLZP" varchar(10) NOT NULL default ' ',
  "AAABBLZQ" varchar(6) NOT NULL default ' ',
  "AAABBLZR" varchar(2) NOT NULL default ' ',
  "AAABBKVM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABMDBB","AAABKPXV","AAABBLZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABBLZZ" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFSUG" varchar(10) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABMDBB","AAABKPXV","AAABBLZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABBLZZ" varchar(4) NOT NULL default '0000',
  "AAABBLSK" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABBLGB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABMDBB","AAABKPXV","AAABBLZZ","AAABBLSK","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABBLZZ" varchar(4) NOT NULL default '0000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABBKXV" varchar(3) NOT NULL default '000',
  "AAABBKXW" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFSUG" varchar(10) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABBLZO" varchar(30) NOT NULL default ' ',
  "AAABFJYT" varchar(3) NOT NULL default '000',
  "AAABBLZP" varchar(10) NOT NULL default ' ',
  "AAABBLZQ" varchar(6) NOT NULL default ' ',
  "AAABBLZR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLRPK","AAABMDBB","AAABKPXV","AAABBLZZ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(15) NOT NULL default ' ',
  "AAABMLPV" varchar(28) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABLEQR" varchar(17) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(15) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABHAPO" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMKXW","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(15) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABLRJM" smallint(6) NOT NULL default '0',
  "AAABHAPO" text,
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMKXW","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBKZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABBLAT" varchar(1) NOT NULL default ' ',
  "AAABHZZT" varchar(4) NOT NULL default '0000',
  "AAABBLNK" varchar(24) NOT NULL default ' ',
  "AAABBLNL" varchar(24) NOT NULL default ' ',
  "AAABBLNM" varchar(24) NOT NULL default ' ',
  "AAABBLNN" varchar(24) NOT NULL default ' ',
  "AAABBLNO" varchar(24) NOT NULL default ' ',
  "AAABBLNP" varchar(24) NOT NULL default ' ',
  "AAABBLNQ" varchar(24) NOT NULL default ' ',
  "AAABBLNR" varchar(24) NOT NULL default ' ',
  "AAABBLNS" varchar(24) NOT NULL default ' ',
  "AAABBLNT" varchar(24) NOT NULL default ' ',
  "AAABMMJT" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ","AAABBLAT","AAABHZZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABGPQK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLBP" (
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABBLBS" varchar(30) NOT NULL default ' ',
  "AAABBLBT" varchar(30) NOT NULL default ' ',
  "AAABBLBU" varchar(30) NOT NULL default ' ',
  "AAABBLBV" varchar(30) NOT NULL default ' ',
  "AAABBLBW" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(1) NOT NULL default '0',
  "AAABBLBY" varchar(3) NOT NULL default '000',
  "AAABBLBZ" varchar(3) NOT NULL default ' ',
  "AAABBLCA" varchar(3) NOT NULL default '000',
  "AAABBLCB" varchar(30) NOT NULL default ' ',
  "AAABLKWV" varchar(1) NOT NULL default '0',
  "AAABBNSU" varchar(30) NOT NULL default ' ',
  "AAABBLCE" varchar(24) NOT NULL default ' ',
  "AAABBLCF" varchar(30) NOT NULL default ' ',
  "AAABBLCG" varchar(30) NOT NULL default ' ',
  "AAABBLCH" varchar(3) NOT NULL default '000',
  "AAABBLCI" varchar(1) NOT NULL default ' ',
  "AAABBLCJ" varchar(2) NOT NULL default '00',
  "AAABBLCK" varchar(1) NOT NULL default ' ',
  "AAABBLCL" varchar(2) NOT NULL default '00',
  "AAABBLCM" varchar(1) NOT NULL default ' ',
  "AAABBLCN" varchar(2) NOT NULL default '00',
  "AAABBLCO" varchar(10) NOT NULL default ' ',
  "AAABBLCP" varchar(10) NOT NULL default ' ',
  "AAABBLCQ" varchar(10) NOT NULL default ' ',
  "AAABBLCR" varchar(30) NOT NULL default ' ',
  "AAABBLCS" varchar(30) NOT NULL default ' ',
  "AAABBLCT" varchar(30) NOT NULL default ' ',
  "AAABBLCU" varchar(30) NOT NULL default ' ',
  "AAABBLCV" varchar(30) NOT NULL default ' ',
  "AAABBLCW" varchar(1) NOT NULL default ' ',
  "AAABBLCX" varchar(2) NOT NULL default '00',
  "AAABBLCY" varchar(30) NOT NULL default ' ',
  "AAABBLCZ" varchar(1) NOT NULL default ' ',
  "AAABMNMM" varchar(1) NOT NULL default ' ',
  "AAABBLDB" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABBLDD" varchar(1) NOT NULL default ' ',
  "AAABBLDE" varchar(30) NOT NULL default ' ',
  "AAABBLDF" varchar(30) NOT NULL default ' ',
  "AAABBLDG" varchar(30) NOT NULL default ' ',
  "AAABBLDH" varchar(30) NOT NULL default ' ',
  "AAABBLDI" varchar(30) NOT NULL default ' ',
  "AAABBLDJ" varchar(30) NOT NULL default ' ',
  "AAABBLDK" varchar(30) NOT NULL default ' ',
  "AAABBLDL" varchar(30) NOT NULL default ' ',
  "AAABBLDM" varchar(30) NOT NULL default ' ',
  "AAABBLDN" varchar(30) NOT NULL default ' ',
  "AAABBLDO" varchar(1) NOT NULL default ' ',
  "AAABBLDP" varchar(1) NOT NULL default ' ',
  "AAABBLDQ" varchar(1) NOT NULL default ' ',
  "AAABBLDR" varchar(1) NOT NULL default ' ',
  "AAABBLDS" varchar(1) NOT NULL default ' ',
  "AAABBLDT" varchar(1) NOT NULL default ' ',
  "AAABBLDU" varchar(1) NOT NULL default ' ',
  "AAABBLDV" varchar(1) NOT NULL default ' ',
  "AAABBLDW" varchar(1) NOT NULL default ' ',
  "AAABBLDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKIQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABBLEH" varchar(1) NOT NULL default ' ',
  "AAABBLEI" varchar(1) NOT NULL default ' ',
  "AAABBLGQ" varchar(1) NOT NULL default ' ',
  "AAABBLGR" varchar(1) NOT NULL default ' ',
  "AAABBLGS" varchar(1) NOT NULL default ' ',
  "AAABBLGT" varchar(1) NOT NULL default ' ',
  "AAABBLGU" varchar(1) NOT NULL default ' ',
  "AAABBLGV" varchar(1) NOT NULL default ' ',
  "AAABBLON" varchar(1) NOT NULL default ' ',
  "AAABBLOO" varchar(1) NOT NULL default ' ',
  "AAABEBTJ" varchar(1) NOT NULL default ' ',
  "AAABBLOQ" varchar(1) NOT NULL default ' ',
  "AAABBLHA" varchar(1) NOT NULL default ' ',
  "AAABBLHB" varchar(1) NOT NULL default ' ',
  "AAABBLHC" varchar(1) NOT NULL default ' ',
  "AAABBLHD" varchar(1) NOT NULL default ' ',
  "AAABBLHE" varchar(1) NOT NULL default ' ',
  "AAABBLHF" varchar(1) NOT NULL default ' ',
  "AAABBLHG" varchar(1) NOT NULL default ' ',
  "AAABBLHH" varchar(1) NOT NULL default ' ',
  "AAABBLHI" varchar(1) NOT NULL default ' ',
  "AAABBLHJ" varchar(1) NOT NULL default ' ',
  "AAABBLFD" varchar(30) NOT NULL default ' ',
  "AAABLKKD" varchar(1) NOT NULL default ' ',
  "AAABBLFF" varchar(30) NOT NULL default ' ',
  "AAABBLFG" varchar(1) NOT NULL default ' ',
  "AAABBLFH" varchar(1) NOT NULL default ' ',
  "AAABBLFI" varchar(30) NOT NULL default ' ',
  "AAABBLFJ" varchar(1) NOT NULL default ' ',
  "AAABBLFK" varchar(2) NOT NULL default '00',
  "AAABBLFL" varchar(1) NOT NULL default ' ',
  "AAABBLFM" varchar(1) NOT NULL default ' ',
  "AAABBLHK" varchar(1) NOT NULL default ' ',
  "AAABBLHL" varchar(1) NOT NULL default ' ',
  "AAABBLHM" varchar(1) NOT NULL default ' ',
  "AAABBLHN" varchar(1) NOT NULL default ' ',
  "AAABBLFR" varchar(1) NOT NULL default ' ',
  "AAABBLFS" varchar(1) NOT NULL default ' ',
  "AAABBLFT" varchar(1) NOT NULL default ' ',
  "AAABBLFU" varchar(1) NOT NULL default ' ',
  "AAABBLFV" varchar(1) NOT NULL default ' ',
  "AAABBLFW" varchar(1) NOT NULL default ' ',
  "AAABBLFX" varchar(1) NOT NULL default '0',
  "AAABBLFY" varchar(1) NOT NULL default '0',
  "AAABHGTR" varchar(1) NOT NULL default ' ',
  "AAABBLGA" varchar(1) NOT NULL default ' ',
  "AAABBLGB" varchar(1) NOT NULL default ' ',
  "AAABBLGC" varchar(3) NOT NULL default ' ',
  "AAABBLGD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBLTD","AAABBLTE","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABBLGQ" varchar(1) NOT NULL default ' ',
  "AAABBLGR" varchar(1) NOT NULL default ' ',
  "AAABBLGS" varchar(1) NOT NULL default ' ',
  "AAABBLGT" varchar(1) NOT NULL default ' ',
  "AAABBLGU" varchar(1) NOT NULL default ' ',
  "AAABBLGV" varchar(1) NOT NULL default ' ',
  "AAABBLON" varchar(1) NOT NULL default ' ',
  "AAABBLOO" varchar(1) NOT NULL default ' ',
  "AAABEBTJ" varchar(1) NOT NULL default ' ',
  "AAABBLOQ" varchar(1) NOT NULL default ' ',
  "AAABBLHA" varchar(1) NOT NULL default ' ',
  "AAABBLHB" varchar(1) NOT NULL default ' ',
  "AAABBLHC" varchar(1) NOT NULL default ' ',
  "AAABBLHD" varchar(1) NOT NULL default ' ',
  "AAABBLHE" varchar(1) NOT NULL default ' ',
  "AAABBLHF" varchar(1) NOT NULL default ' ',
  "AAABBLHG" varchar(1) NOT NULL default ' ',
  "AAABBLHH" varchar(1) NOT NULL default ' ',
  "AAABBLHI" varchar(1) NOT NULL default ' ',
  "AAABBLHJ" varchar(1) NOT NULL default ' ',
  "AAABBLHK" varchar(1) NOT NULL default ' ',
  "AAABBLHL" varchar(1) NOT NULL default ' ',
  "AAABBLHM" varchar(1) NOT NULL default ' ',
  "AAABBLHN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABBLTD","AAABBLTE","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMPWL" varchar(30) NOT NULL default ' ',
  "AAABBLIB" varchar(30) NOT NULL default ' ',
  "AAABBLIC" varchar(30) NOT NULL default ' ',
  "AAABBLID" varchar(30) NOT NULL default ' ',
  "AAABBLIE" varchar(30) NOT NULL default ' ',
  "AAABBLIF" varchar(30) NOT NULL default ' ',
  "AAABBLIG" varchar(30) NOT NULL default ' ',
  "AAABJKWK" varchar(30) NOT NULL default ' ',
  "AAABBLII" varchar(1) NOT NULL default ' ',
  "AAABBLIJ" varchar(1) NOT NULL default ' ',
  "AAABBLIK" varchar(3) NOT NULL default '000',
  "AAABBLIL" varchar(1) NOT NULL default ' ',
  "AAABBLIM" varchar(2) NOT NULL default ' ',
  "AAABBLIN" varchar(1) NOT NULL default ' ',
  "AAABBLIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBLTD","AAABBLTE","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABBLSK" varchar(1) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJVQM" varchar(12) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ","AAABBLSK","AAABMLZS"),
  KEY "AAABBLJO" ("AAABMRIL","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABBLSK" varchar(2) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABBLJZ" varchar(1) NOT NULL default ' ',
  "AAABBLKA" varchar(1) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBLTD","AAABBLTE","AAABBLSK","AAABLDEV","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLKZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(24) NOT NULL default ' ',
  "AAABBLLD" varchar(30) NOT NULL default ' ',
  "AAABBLLE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKIQ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABLRTJ" varchar(10) NOT NULL default ' ',
  "AAABBLNA" varchar(4) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABLRTJ","AAABBLNA","AAABBLTD","AAABBLTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABLRTJ" varchar(10) NOT NULL default ' ',
  "AAABBLNA" varchar(4) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(24) NOT NULL default ' ',
  "AAABBLME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABLRTJ","AAABBLNA","AAABBLTD","AAABBLTE","AAABMKXW","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABLRTJ" varchar(10) NOT NULL default ' ',
  "AAABBLNA" varchar(4) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKIQ","AAABLRTJ","AAABBLNA","AAABBLTD","AAABBLTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABHZZT" varchar(4) NOT NULL default '0000',
  "AAABBLNK" varchar(24) NOT NULL default ' ',
  "AAABBLNL" varchar(24) NOT NULL default ' ',
  "AAABBLNM" varchar(24) NOT NULL default ' ',
  "AAABBLNN" varchar(24) NOT NULL default ' ',
  "AAABBLNO" varchar(24) NOT NULL default ' ',
  "AAABBLNP" varchar(24) NOT NULL default ' ',
  "AAABBLNQ" varchar(24) NOT NULL default ' ',
  "AAABBLNR" varchar(24) NOT NULL default ' ',
  "AAABBLNS" varchar(24) NOT NULL default ' ',
  "AAABBLNT" varchar(24) NOT NULL default ' ',
  "AAABMMJT" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ","AAABHZZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABBLOI" varchar(4) NOT NULL default '0000',
  "AAABDSZT" varchar(1) NOT NULL default ' ',
  "AAABHNHN" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABBLOM" varchar(1) NOT NULL default '0',
  "AAABBLON" varchar(12) NOT NULL default ' ',
  "AAABBLOO" varchar(12) NOT NULL default ' ',
  "AAABEBTJ" varchar(12) NOT NULL default ' ',
  "AAABBLOQ" varchar(12) NOT NULL default ' ',
  "AAABBLOR" decimal(5,2) NOT NULL default '0.00',
  "AAABBLOS" varchar(30) NOT NULL default ' ',
  "AAABBLOT" varchar(30) NOT NULL default ' ',
  "AAABBLOU" varchar(30) NOT NULL default ' ',
  "AAABBLOV" varchar(30) NOT NULL default ' ',
  "AAABBLOW" varchar(30) NOT NULL default ' ',
  "AAABBLOX" varchar(30) NOT NULL default ' ',
  "AAABBLOY" varchar(30) NOT NULL default ' ',
  "AAABBLOZ" varchar(30) NOT NULL default ' ',
  "AAABBLPA" varchar(30) NOT NULL default ' ',
  "AAABBLPB" varchar(30) NOT NULL default ' ',
  "AAABBLPC" varchar(5) NOT NULL default ' ',
  "AAABBLPD" varchar(30) NOT NULL default ' ',
  "AAABBLPE" varchar(30) NOT NULL default ' ',
  "AAABBLPF" varchar(30) NOT NULL default ' ',
  "AAABBLPG" varchar(30) NOT NULL default ' ',
  "AAABBLPH" varchar(30) NOT NULL default ' ',
  "AAABBLPI" varchar(30) NOT NULL default ' ',
  "AAABBLPJ" varchar(30) NOT NULL default ' ',
  "AAABBLPK" varchar(30) NOT NULL default ' ',
  "AAABBLPL" varchar(30) NOT NULL default ' ',
  "AAABBLPM" varchar(30) NOT NULL default ' ',
  "AAABBLPN" varchar(1) NOT NULL default ' ',
  "AAABBLPO" varchar(30) NOT NULL default ' ',
  "AAABFKZG" varchar(10) NOT NULL default ' ',
  "AAABBLPQ" varchar(30) NOT NULL default ' ',
  "AAABBLPR" varchar(30) NOT NULL default ' ',
  "AAABBLPS" varchar(30) NOT NULL default ' ',
  "AAABKMWB" varchar(10) NOT NULL default ' ',
  "AAABBLPU" varchar(1) NOT NULL default ' ',
  "AAABFLBM" varchar(4) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABBLPY" varchar(7) NOT NULL default '0000000',
  "AAABBLPZ" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABBLQK" varchar(30) NOT NULL default ' ',
  "AAABBVGO" varchar(30) NOT NULL default ' ',
  "AAABBLQM" varchar(30) NOT NULL default ' ',
  "AAABBLTA" varchar(1) NOT NULL default '0',
  "AAABBLQO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLQU" varchar(1) NOT NULL default ' ',
  "AAABBLQV" varchar(1) NOT NULL default ' ',
  "AAABBLQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABBLSK" varchar(1) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABLEES" varchar(12) NOT NULL default ' ',
  "AAABBLYT" varchar(6) NOT NULL default '000000',
  "AAABBLRJ" varchar(3) NOT NULL default ' ',
  "AAABBLRK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABKOUC","AAABKOUD","AAABLQVJ","AAABBLSK","AAABLEER","AAABLEES","AAABBLYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABBLSK" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJVQM" varchar(12) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABBLSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBLTD","AAABBLTE","AAABBLSK","AAABMKXW"),
  KEY "AAABBLSM" ("AAABMRIL","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBLTD" varchar(1) NOT NULL default ' ',
  "AAABBLTE" varchar(1) NOT NULL default ' ',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  "AAABBLTA" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABBLTD","AAABBLTE","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABMLZS" varchar(6) NOT NULL default '000000',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABBLTM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" decimal(21,7) NOT NULL default '0.0000000',
  "AAABKOUC" varchar(10) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABBLUB" varchar(2) NOT NULL default ' ',
  "AAABLEQR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABKOUC","AAABKOUD","AAABBLUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBLWS" varchar(8) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABBLUG" varchar(3) NOT NULL default '000',
  "AAABCCUR" varchar(3) NOT NULL default '000',
  "AAABBMCQ" varchar(3) NOT NULL default '000',
  "AAABEJFS" varchar(4) NOT NULL default ' ',
  "AAABBLUK" varchar(2) NOT NULL default '00',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBLUO" varchar(1) NOT NULL default ' ',
  "AAABBLUP" varchar(1) NOT NULL default ' ',
  "AAABBLUQ" varchar(1) NOT NULL default ' ',
  "AAABBLUR" varchar(1) NOT NULL default ' ',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTS" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTU" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABBVTY" varchar(1) NOT NULL default ' ',
  "AAABBLVA" varchar(1) NOT NULL default ' ',
  "AAABBLVB" varchar(1) NOT NULL default ' ',
  "AAABBLVC" varchar(1) NOT NULL default ' ',
  "AAABBLVD" varchar(1) NOT NULL default ' ',
  "AAABBLVE" varchar(1) NOT NULL default ' ',
  "AAABBPJJ" varchar(1) NOT NULL default ' ',
  "AAABBLVG" varchar(1) NOT NULL default ' ',
  "AAABBLVH" varchar(1) NOT NULL default ' ',
  "AAABBLVI" varchar(1) NOT NULL default ' ',
  "AAABBLVJ" varchar(1) NOT NULL default ' ',
  "AAABBLVK" varchar(1) NOT NULL default ' ',
  "AAABBLVL" varchar(1) NOT NULL default ' ',
  "AAABBLVM" varchar(1) NOT NULL default ' ',
  "AAABBLVN" varchar(1) NOT NULL default ' ',
  "AAABBLVO" varchar(1) NOT NULL default ' ',
  "AAABBLVP" varchar(1) NOT NULL default ' ',
  "AAABBLVQ" varchar(1) NOT NULL default ' ',
  "AAABBLVR" varchar(1) NOT NULL default ' ',
  "AAABBLVS" varchar(1) NOT NULL default ' ',
  "AAABBLVT" varchar(1) NOT NULL default ' ',
  "AAABBLVU" varchar(1) NOT NULL default ' ',
  "AAABBLVV" varchar(1) NOT NULL default ' ',
  "AAABBLVW" varchar(1) NOT NULL default ' ',
  "AAABBLVX" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABBLWE" varchar(30) NOT NULL default ' ',
  "AAABEOWS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBLWS"),
  KEY "AAABBLWI" ("AAABMRIL","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBLWS" varchar(8) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBLWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABBLWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFPVV" varchar(4) NOT NULL default ' ',
  "AAABBLWW" varchar(4) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABFQWX" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFQWZ" varchar(7) NOT NULL default '0000000',
  "AAABBLXF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFPVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
