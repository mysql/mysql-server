# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl37.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABEREF" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREI" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREL" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREO" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERER" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREU" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREX" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERFA" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERFD" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABERJI" varchar(1) NOT NULL default '0',
  "AAABHWBK" varchar(2) NOT NULL default '00',
  "AAABIJYD" varchar(1) NOT NULL default '0',
  "AAABERIL" varchar(1) NOT NULL default '0',
  "AAABERIM" varchar(1) NOT NULL default '0',
  "AAABLUVS" varchar(3) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default '0',
  "AAABERIP" varchar(3) NOT NULL default '000',
  "AAABERIQ" varchar(1) NOT NULL default ' ',
  "AAABHVRP" varchar(1) NOT NULL default '0',
  "AAABERIS" varchar(2) NOT NULL default '00',
  "AAABERIT" varchar(3) NOT NULL default '000',
  "AAABERIU" varchar(3) NOT NULL default '000',
  "AAABERIV" varchar(3) NOT NULL default '000',
  "AAABERIW" varchar(3) NOT NULL default '000',
  "AAABERIX" varchar(1) NOT NULL default ' ',
  "AAABERIY" varchar(1) NOT NULL default ' ',
  "AAABERIZ" varchar(1) NOT NULL default ' ',
  "AAABERJA" varchar(1) NOT NULL default '0',
  "AAABERJB" varchar(2) NOT NULL default '00',
  "AAABERJC" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  "AAABERJE" varchar(1) NOT NULL default ' ',
  "AAABERJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABERJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERGF" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABERJM" varchar(3) NOT NULL default '000',
  "AAABERJN" varchar(3) NOT NULL default '000',
  "AAABERJO" varchar(1) NOT NULL default ' ',
  "AAABERJP" varchar(1) NOT NULL default ' ',
  "AAABERJQ" varchar(3) NOT NULL default '000',
  "AAABERJR" varchar(3) NOT NULL default '000',
  "AAABERJS" varchar(1) NOT NULL default ' ',
  "AAABERJT" varchar(1) NOT NULL default ' ',
  "AAABERJU" varchar(2) NOT NULL default '00',
  "AAABERJV" varchar(2) NOT NULL default '00',
  "AAABERJW" varchar(2) NOT NULL default '00',
  "AAABERJX" varchar(2) NOT NULL default '00',
  "AAABERJY" varchar(2) NOT NULL default '00',
  "AAABERJZ" varchar(2) NOT NULL default '00',
  "AAABERKA" varchar(2) NOT NULL default '00',
  "AAABERKB" varchar(2) NOT NULL default '00',
  "AAABERKC" varchar(2) NOT NULL default '00',
  "AAABERKD" varchar(2) NOT NULL default '00',
  "AAABERKE" varchar(2) NOT NULL default '00',
  "AAABERKF" varchar(2) NOT NULL default '00',
  "AAABERKG" varchar(2) NOT NULL default '00',
  "AAABERKH" varchar(2) NOT NULL default '00',
  "AAABHCKN" varchar(1) NOT NULL default '0',
  "AAABERKJ" varchar(1) NOT NULL default '0',
  "AAABERKK" varchar(1) NOT NULL default ' ',
  "AAABHJAV" varchar(128) NOT NULL default ' ',
  "AAABERKM" varchar(1) NOT NULL default '0',
  "AAABGEIN" varchar(2) NOT NULL default '00',
  "AAABERKO" varchar(2) NOT NULL default '00',
  "AAABHCEI" varchar(1) NOT NULL default '0',
  "AAABMKAI" varchar(1) NOT NULL default '0',
  "AAABERKR" decimal(6,2) NOT NULL default '0.00',
  "AAABERKS" decimal(6,2) NOT NULL default '0.00',
  "AAABERKT" decimal(6,2) NOT NULL default '0.00',
  "AAABERKU" decimal(6,2) NOT NULL default '0.00',
  "AAABERKV" varchar(2) NOT NULL default '00',
  "AAABERKW" varchar(2) NOT NULL default '00',
  "AAABERKX" varchar(2) NOT NULL default '00',
  "AAABERKY" varchar(2) NOT NULL default '00',
  "AAABERKZ" varchar(1) NOT NULL default ' ',
  "AAABERLA" varchar(1) NOT NULL default ' ',
  "AAABERLB" varchar(1) NOT NULL default ' ',
  "AAABERLC" varchar(1) NOT NULL default '0',
  "AAABERLD" varchar(1) NOT NULL default '0',
  "AAABERLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERIB" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABLOKS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABERJI" varchar(1) NOT NULL default '0',
  "AAABHWBK" varchar(2) NOT NULL default '00',
  "AAABIJYD" varchar(1) NOT NULL default '0',
  "AAABERIL" varchar(1) NOT NULL default '0',
  "AAABERIM" varchar(1) NOT NULL default '0',
  "AAABLUVS" varchar(3) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default '0',
  "AAABERIP" varchar(3) NOT NULL default '000',
  "AAABERIQ" varchar(1) NOT NULL default ' ',
  "AAABHVRP" varchar(1) NOT NULL default '0',
  "AAABERIS" varchar(2) NOT NULL default '00',
  "AAABERIT" varchar(3) NOT NULL default '000',
  "AAABERIU" varchar(3) NOT NULL default '000',
  "AAABERIV" varchar(3) NOT NULL default '000',
  "AAABERIW" varchar(3) NOT NULL default '000',
  "AAABERIX" varchar(1) NOT NULL default ' ',
  "AAABERIY" varchar(1) NOT NULL default ' ',
  "AAABERIZ" varchar(1) NOT NULL default ' ',
  "AAABERJA" varchar(1) NOT NULL default '0',
  "AAABERJB" varchar(2) NOT NULL default '00',
  "AAABERJC" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  "AAABERJE" varchar(1) NOT NULL default ' ',
  "AAABERJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABERJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABERJM" varchar(3) NOT NULL default '000',
  "AAABERJN" varchar(3) NOT NULL default '000',
  "AAABERJO" varchar(1) NOT NULL default ' ',
  "AAABERJP" varchar(1) NOT NULL default ' ',
  "AAABERJQ" varchar(3) NOT NULL default '000',
  "AAABERJR" varchar(3) NOT NULL default '000',
  "AAABERJS" varchar(1) NOT NULL default ' ',
  "AAABERJT" varchar(1) NOT NULL default ' ',
  "AAABERJU" varchar(2) NOT NULL default '00',
  "AAABERJV" varchar(2) NOT NULL default '00',
  "AAABERJW" varchar(2) NOT NULL default '00',
  "AAABERJX" varchar(2) NOT NULL default '00',
  "AAABERJY" varchar(2) NOT NULL default '00',
  "AAABERJZ" varchar(2) NOT NULL default '00',
  "AAABERKA" varchar(2) NOT NULL default '00',
  "AAABERKB" varchar(2) NOT NULL default '00',
  "AAABERKC" varchar(2) NOT NULL default '00',
  "AAABERKD" varchar(2) NOT NULL default '00',
  "AAABERKE" varchar(2) NOT NULL default '00',
  "AAABERKF" varchar(2) NOT NULL default '00',
  "AAABERKG" varchar(2) NOT NULL default '00',
  "AAABERKH" varchar(2) NOT NULL default '00',
  "AAABHCKN" varchar(1) NOT NULL default '0',
  "AAABERKJ" varchar(1) NOT NULL default '0',
  "AAABERKK" varchar(1) NOT NULL default ' ',
  "AAABHJAV" varchar(128) NOT NULL default ' ',
  "AAABERKM" varchar(1) NOT NULL default '0',
  "AAABGEIN" varchar(2) NOT NULL default '00',
  "AAABERKO" varchar(2) NOT NULL default '00',
  "AAABHCEI" varchar(1) NOT NULL default '0',
  "AAABMKAI" varchar(1) NOT NULL default '0',
  "AAABERKR" decimal(6,2) NOT NULL default '0.00',
  "AAABERKS" decimal(6,2) NOT NULL default '0.00',
  "AAABERKT" decimal(6,2) NOT NULL default '0.00',
  "AAABERKU" decimal(6,2) NOT NULL default '0.00',
  "AAABERKV" varchar(2) NOT NULL default '00',
  "AAABERKW" varchar(2) NOT NULL default '00',
  "AAABERKX" varchar(2) NOT NULL default '00',
  "AAABERKY" varchar(2) NOT NULL default '00',
  "AAABERKZ" varchar(1) NOT NULL default ' ',
  "AAABERLA" varchar(1) NOT NULL default ' ',
  "AAABERLB" varchar(1) NOT NULL default ' ',
  "AAABERLC" varchar(1) NOT NULL default '0',
  "AAABERLD" varchar(1) NOT NULL default '0',
  "AAABERLE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABERLM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERLQ" (
  "AAABERLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABERLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERLT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABERLY" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABERLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERLZ" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(40) NOT NULL default ' ',
  "AAABERMG" varchar(30) NOT NULL default ' ',
  "AAABERMD" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAABJEXI","AAABERMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERMH" (
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABERMO","AAABERMP","AAABERMQ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERMS" (
  "AAABJSNO" varchar(30) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABERMV" varchar(1) NOT NULL default ' ',
  "AAABLTXI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABJSNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERMY" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABGBEW" varchar(20) NOT NULL default ' ',
  "AAABERNC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERNE" (
  "AAABERNJ" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(60) NOT NULL default ' ',
  "AAABGBEW" varchar(30) NOT NULL default ' ',
  "AAABERNI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABERNJ","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYAG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERNQ" (
  "AAABEROR" varchar(1) NOT NULL default ' ',
  "AAABEROS" varchar(61) NOT NULL default ' ',
  "AAABEROT" varchar(1) NOT NULL default ' ',
  "AAABEROU" varchar(55) NOT NULL default ' ',
  "AAABJYTR" varchar(2) NOT NULL default '00',
  "AAABEROM" varchar(4) NOT NULL default ' ',
  "AAABERON" varchar(12) NOT NULL default ' ',
  "AAABEROO" varchar(4) NOT NULL default ' ',
  "AAABEROP" varchar(60) NOT NULL default ' ',
  "AAABEROQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEROR","AAABEROS","AAABEROT","AAABEROU","AAABJYTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEROG" (
  "AAABEROR" varchar(1) NOT NULL default ' ',
  "AAABEROS" varchar(61) NOT NULL default ' ',
  "AAABEROT" varchar(1) NOT NULL default ' ',
  "AAABEROU" varchar(55) NOT NULL default ' ',
  "AAABJYTR" varchar(2) NOT NULL default '00',
  "AAABEROM" varchar(4) NOT NULL default ' ',
  "AAABERON" varchar(12) NOT NULL default ' ',
  "AAABEROO" varchar(4) NOT NULL default ' ',
  "AAABEROP" varchar(60) NOT NULL default ' ',
  "AAABEROQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEROR","AAABEROS","AAABEROT","AAABEROU","AAABJYTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEROW" (
  "AAABHIKM" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABJGGL" varchar(46) NOT NULL default ' ',
  "AAABERPA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHIKM","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERPS" varchar(12) NOT NULL default ' ',
  "AAABLKNC" varchar(8) NOT NULL default '00000000',
  "AAABLKND" varchar(6) NOT NULL default '000000',
  "AAABERPV" varchar(6) NOT NULL default ' ',
  "AAABERPW" varchar(20) NOT NULL default ' ',
  "AAABERPX" tinyint(3) unsigned NOT NULL default '0',
  "AAABERPL" varchar(30) NOT NULL default ' ',
  "AAABERPM" varchar(60) NOT NULL default ' ',
  "AAABERPN" varchar(60) NOT NULL default ' ',
  "AAABERPO" varchar(30) NOT NULL default ' ',
  "AAABERPP" varchar(60) NOT NULL default ' ',
  "AAABERPQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERPS","AAABLKNC","AAABLKND","AAABERPV","AAABERPW","AAABERPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABJFDS" varchar(10) NOT NULL default ' ',
  "AAABJEXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMOAH","AAABJFDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERQW" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERQZ" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABIEMX" varchar(61) NOT NULL default ' ',
  "AAABERRI" varchar(2) NOT NULL default ' ',
  "AAABERRE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMNDU","AAABMFSV","AAABIEMX","AAABERRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERRJ" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABERRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERRN" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABERRU" varchar(11) NOT NULL default ' ',
  "AAABERRV" varchar(11) NOT NULL default ' ',
  "AAABERRW" varchar(11) NOT NULL default ' ',
  "AAABFWOU" varchar(16) NOT NULL default ' ',
  "AAABERRY" varchar(1) NOT NULL default ' ',
  "AAABHCXI" varchar(21) NOT NULL default ' ',
  "AAABERSA" varchar(11) NOT NULL default ' ',
  "AAABHEUY" varchar(14) NOT NULL default ' ',
  "AAABMNRG" varchar(3) NOT NULL default ' ',
  "AAABERSD" varchar(11) NOT NULL default ' ',
  "AAABERSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERTG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERTG" varchar(3) NOT NULL default '000',
  "AAABERTB" varchar(132) NOT NULL default ' ',
  "AAABERTC" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERTL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERTU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERYU","AAABERYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  "AAABERUO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERYU","AAABERYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERVO" varchar(4) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABESBT" varchar(4) NOT NULL default ' ',
  "AAABERVA" varchar(30) NOT NULL default ' ',
  "AAABERWB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERVO" varchar(4) NOT NULL default ' ',
  "AAABERVK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERWB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERWB" varchar(10) NOT NULL default ' ',
  "AAABERVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABERWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  "AAABERWO" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAABERWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABERXG" varchar(2) NOT NULL default ' ',
  "AAABERXD" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABERXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERXM" varchar(132) NOT NULL default ' ',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABERXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA"),
  UNIQUE KEY "AAABERXU" ("AAABMRIL","AAABESGI","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYV" varchar(10) NOT NULL default ' ',
  "AAABERYD" varchar(5) NOT NULL default ' ',
  "AAABERYE" varchar(3) NOT NULL default ' ',
  "AAABERYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERYU","AAABERYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYU" varchar(3) NOT NULL default ' ',
  "AAABERYV" varchar(10) NOT NULL default ' ',
  "AAABERYQ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERYU","AAABERYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERZR" varchar(5) NOT NULL default ' ',
  "AAABERZA" varchar(1) NOT NULL default ' ',
  "AAABERZB" varchar(1) NOT NULL default ' ',
  "AAABERZC" decimal(8,2) NOT NULL default '0.00',
  "AAABERZD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABERZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERZR" varchar(5) NOT NULL default ' ',
  "AAABERZM" varchar(132) NOT NULL default ' ',
  "AAABERZN" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABERZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESAI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESAI" varchar(25) NOT NULL default ' ',
  "AAABESAE" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABESAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESBB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESBB" varchar(3) NOT NULL default ' ',
  "AAABESAS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESBB" varchar(3) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESBB","AAABESHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESBT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESBT" varchar(4) NOT NULL default ' ',
  "AAABESBP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABESHA","AAABESBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESCC" varchar(4) NOT NULL default ' ',
  "AAABESCD" varchar(4) NOT NULL default ' ',
  "AAABESFQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESCC","AAABESCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESFQ" varchar(2) NOT NULL default ' ',
  "AAABESCH" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESFQ" varchar(2) NOT NULL default ' ',
  "AAABESCR" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABESFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESDO" varchar(6) NOT NULL default ' ',
  "AAABESEX" varchar(4) NOT NULL default ' ',
  "AAABESEG" varchar(4) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESDO" varchar(6) NOT NULL default ' ',
  "AAABESDK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMRHS","AAABESDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESEG" varchar(4) NOT NULL default ' ',
  "AAABESDT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESEG" varchar(4) NOT NULL default ' ',
  "AAABESEC" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMRHS","AAABESEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESEX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESEX" varchar(4) NOT NULL default ' ',
  "AAABESET" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMRHS","AAABESEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESFK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESFK" varchar(4) NOT NULL default ' ',
  "AAABESFH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABESFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESFT" varchar(4) NOT NULL default ' ',
  "AAABESFU" varchar(4) NOT NULL default ' ',
  "AAABESFQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESFT","AAABESFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABESFY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABESGF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESHB" varchar(2) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABESGO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABESHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABESHB" varchar(2) NOT NULL default ' ',
  "AAABESGX" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABESHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHQ" varchar(1) NOT NULL default ' ',
  "AAABESHF" varchar(1) NOT NULL default ' ',
  "AAABESHG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHQ" varchar(1) NOT NULL default ' ',
  "AAABESHN" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABESHV" tinyint(3) unsigned NOT NULL default '0',
  "AAABESQL" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESJA" varchar(1) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESJA" varchar(1) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJB" (
  "AAABESOE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABESOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJE" (
  "AAABESOE" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABESOE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJK" (
  "AAABESJR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABESJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJN" (
  "AAABESJR" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABESJR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJT" (
  "AAABESOD" varchar(2) NOT NULL default '00',
  "AAABESOE" varchar(2) NOT NULL default '00',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABESOD","AAABESOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESOC" varchar(5) NOT NULL default '00000',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESMT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESOC" varchar(5) NOT NULL default '00000',
  "AAABESLC" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKWVR" decimal(13,9) NOT NULL default '0.000000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESOC" varchar(5) NOT NULL default '00000',
  "AAABESLC" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKWVR" decimal(13,9) NOT NULL default '0.000000000',
  "AAABESPJ" varchar(10) NOT NULL default ' ',
  "AAABESPK" varchar(8) NOT NULL default '00000000',
  "AAABESPL" varchar(8) NOT NULL default '00000000',
  "AAABESPM" varchar(8) NOT NULL default '00000000',
  "AAABESPN" varchar(1) NOT NULL default ' ',
  "AAABESPO" varchar(6) NOT NULL default '000000',
  "AAABESPP" smallint(6) NOT NULL default '0',
  "AAABESPR" decimal(13,2) NOT NULL default '0.00',
  "AAABESPS" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESPT" varchar(1) NOT NULL default ' ',
  "AAABESPW" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESPX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESQB" varchar(11) NOT NULL default ' ',
  "AAABESOC" varchar(5) NOT NULL default '00000',
  "AAABKYXQ" varchar(20) NOT NULL default ' ',
  "AAABESMD" varchar(20) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABESMV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESQB","AAABESOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  "AAABESMT" varchar(2) NOT NULL default '00',
  "AAABESMU" varchar(8) NOT NULL default '00000000',
  "AAABESMV" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABESNB" varchar(5) NOT NULL default '00000',
  "AAABESNC" varchar(1) NOT NULL default ' ',
  "AAABETAY" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABESNG" varchar(1) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESNV" varchar(5) NOT NULL default '00000',
  "AAABGWQU" varchar(8) NOT NULL default '00000000',
  "AAABESNR" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESQB" varchar(11) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  "AAABESOC" varchar(5) NOT NULL default '00000',
  "AAABESOD" varchar(2) NOT NULL default '00',
  "AAABESOE" varchar(2) NOT NULL default '00',
  "AAABMKXF" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESQB" varchar(11) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABGWQU" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKWVR" decimal(13,9) NOT NULL default '0.000000000',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESQB","AAABHGYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABESQA" varchar(13) NOT NULL default ' ',
  "AAABESQB" varchar(11) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABGWQU" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABESPJ" varchar(10) NOT NULL default ' ',
  "AAABESPK" varchar(8) NOT NULL default '00000000',
  "AAABESPL" varchar(8) NOT NULL default '00000000',
  "AAABESPM" varchar(8) NOT NULL default '00000000',
  "AAABESPN" varchar(1) NOT NULL default ' ',
  "AAABESPO" varchar(6) NOT NULL default '000000',
  "AAABESPP" smallint(6) NOT NULL default '0',
  "AAABESPQ" varchar(2) NOT NULL default ' ',
  "AAABESPR" decimal(13,2) NOT NULL default '0.00',
  "AAABESPS" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESPT" varchar(1) NOT NULL default ' ',
  "AAABESPU" varchar(1) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABESPW" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESPX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABESQA","AAABESQB","AAABHGYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESQU" varchar(4) NOT NULL default '0000',
  "AAABESZL" varchar(2) NOT NULL default '00',
  "AAABESQH" varchar(3) NOT NULL default ' ',
  "AAABESQI" varchar(3) NOT NULL default ' ',
  "AAABESXO" varchar(1) NOT NULL default ' ',
  "AAABESQK" varchar(2) NOT NULL default '00',
  "AAABESQL" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABESQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESQU" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESQU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESRQ" varchar(8) NOT NULL default '00000000',
  "AAABESRR" varchar(8) NOT NULL default '00000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABESYT" varchar(5) NOT NULL default ' ',
  "AAABESYG" decimal(17,2) NOT NULL default '0.00',
  "AAABESYH" decimal(17,2) NOT NULL default '0.00',
  "AAABESYI" decimal(17,2) NOT NULL default '0.00',
  "AAABESYJ" decimal(17,2) NOT NULL default '0.00',
  "AAABESYK" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESRL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESRQ","AAABESRR","AAABESYQ","AAABHHZN","AAABESYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYP" varchar(8) NOT NULL default '00000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABESSE" decimal(17,2) NOT NULL default '0.00',
  "AAABESUY" varchar(5) NOT NULL default ' ',
  "AAABESSG" decimal(17,2) NOT NULL default '0.00',
  "AAABESSH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYP","AAABESYQ","AAABHHZN","AAABMEAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABESXU" varchar(8) NOT NULL default '00000000',
  "AAABHKGK" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABESXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYQ","AAABESXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYP" varchar(8) NOT NULL default '00000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABESYT" varchar(5) NOT NULL default ' ',
  "AAABESUK" varchar(3) NOT NULL default '000',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABESTS" varchar(6) NOT NULL default '000000',
  "AAABESYI" decimal(17,2) NOT NULL default '0.00',
  "AAABESYJ" decimal(17,2) NOT NULL default '0.00',
  "AAABIFOV" tinyint(3) unsigned NOT NULL default '0',
  "AAABJFUT" varchar(8) NOT NULL default '00000000',
  "AAABESTX" varchar(12) NOT NULL default ' ',
  "AAABMRFO" tinyint(3) unsigned NOT NULL default '0',
  "AAABESTZ" varchar(8) NOT NULL default '00000000',
  "AAABESUA" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYP","AAABESYQ","AAABHHZN","AAABESYT","AAABESUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABESUR" varchar(10) NOT NULL default '0000000000',
  "AAABESUS" varchar(8) NOT NULL default '00000000',
  "AAABESUT" varchar(8) NOT NULL default '00000000',
  "AAABESUU" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABESUX" decimal(17,2) NOT NULL default '0.00',
  "AAABESUY" varchar(5) NOT NULL default ' ',
  "AAABESUZ" decimal(17,2) NOT NULL default '0.00',
  "AAABESVA" varchar(5) NOT NULL default ' ',
  "AAABESVB" tinyint(3) unsigned NOT NULL default '0',
  "AAABESVC" tinyint(3) unsigned NOT NULL default '0',
  "AAABMRFO" tinyint(3) unsigned NOT NULL default '0',
  "AAABESVE" tinyint(3) unsigned NOT NULL default '0',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABKXFW" varchar(3) NOT NULL default '000',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABESVK" tinyint(3) unsigned NOT NULL default '0',
  "AAABESVL" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESVM" varchar(10) NOT NULL default ' ',
  "AAABESVN" decimal(9,5) NOT NULL default '0.00000',
  "AAABESVO" varchar(1) NOT NULL default '0',
  "AAABESVP" varchar(10) NOT NULL default ' ',
  "AAABESVQ" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESVR" varchar(13) NOT NULL default ' ',
  "AAABESVS" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXJ" varchar(3) NOT NULL default ' ',
  "AAABKZWO" varchar(3) NOT NULL default ' ',
  "AAABESWD" varchar(1) NOT NULL default ' ',
  "AAABESWE" varchar(1) NOT NULL default ' ',
  "AAABESWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYXJ","AAABKZWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABHQRM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABESXU" varchar(8) NOT NULL default '00000000',
  "AAABESXV" varchar(6) NOT NULL default '000000',
  "AAABHKGK" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABESXG" tinyint(3) unsigned NOT NULL default '0',
  "AAABGZBS" varchar(8) NOT NULL default '00000000',
  "AAABGZBT" varchar(6) NOT NULL default '000000',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABESXK" varchar(1) NOT NULL default ' ',
  "AAABESXL" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESXM" varchar(1) NOT NULL default ' ',
  "AAABESXN" varchar(1) NOT NULL default ' ',
  "AAABESXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYQ","AAABESXU","AAABESXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABESYP" varchar(8) NOT NULL default '00000000',
  "AAABESYQ" varchar(3) NOT NULL default '000',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABESYS" varchar(2) NOT NULL default '00',
  "AAABESYT" varchar(5) NOT NULL default ' ',
  "AAABESYG" decimal(17,2) NOT NULL default '0.00',
  "AAABESYH" decimal(17,2) NOT NULL default '0.00',
  "AAABESYI" decimal(17,2) NOT NULL default '0.00',
  "AAABESYJ" decimal(17,2) NOT NULL default '0.00',
  "AAABESYK" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKTVC","AAABESYP","AAABESYQ","AAABHHZN","AAABESYS","AAABESYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESYU" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABESYZ" varchar(3) NOT NULL default '000',
  "AAABETAV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLDAZ","AAABESYZ","AAABETAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESZL" varchar(2) NOT NULL default '00',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABESZF" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESZG" decimal(10,7) NOT NULL default '0.0000000',
  "AAABESZH" varchar(2) NOT NULL default '00',
  "AAABESZI" tinyint(3) unsigned NOT NULL default '0',
  "AAABESZJ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABESZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABESZT" varchar(2) NOT NULL default '00',
  "AAABESZQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABESZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABESZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABESZX" varchar(3) NOT NULL default '000',
  "AAABESZY" varchar(2) NOT NULL default '00',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAABETAA" varchar(3) NOT NULL default ' ',
  "AAABETAB" varchar(3) NOT NULL default ' ',
  "AAABETAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHHZN" varchar(3) NOT NULL default ' ',
  "AAABETAJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHHZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKTVC" varchar(10) NOT NULL default '0000000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABETAT" varchar(8) NOT NULL default '00000000',
  "AAABETAU" varchar(8) NOT NULL default '00000000',
  "AAABETAV" varchar(2) NOT NULL default '00',
  "AAABETAW" decimal(5,2) NOT NULL default '0.00',
  "AAABETAX" decimal(5,2) NOT NULL default '0.00',
  "AAABETAY" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABETBA" varchar(13) NOT NULL default ' ',
  "AAABETBB" varchar(10) NOT NULL default '0000000000',
  "AAABETBC" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKTVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETBS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABETBS" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABETBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETBT" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABETET" varchar(3) NOT NULL default ' ',
  "AAABETEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRFQ","AAABETET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETCB" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABETCH" varchar(4) NOT NULL default '0000',
  "AAABETET" varchar(3) NOT NULL default ' ',
  "AAABETCF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMLXR","AAABETCH","AAABETET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETCJ" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABETCL" varchar(4) NOT NULL default '0000',
  "AAABGCKI" varchar(4) NOT NULL default '0000',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABETCO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABETCR" varchar(4) NOT NULL default '0000',
  "AAABETCS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETCU" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABETCX" varchar(31) NOT NULL default ' ',
  "AAABETCY" varchar(31) NOT NULL default ' ',
  "AAABETCZ" varchar(31) NOT NULL default ' ',
  "AAABETDA" varchar(31) NOT NULL default ' ',
  "AAABETDB" varchar(31) NOT NULL default ' ',
  "AAABETDC" varchar(31) NOT NULL default ' ',
  "AAABETDD" varchar(31) NOT NULL default ' ',
  "AAABETDE" varchar(31) NOT NULL default ' ',
  "AAABETDF" varchar(31) NOT NULL default ' ',
  "AAABETDG" varchar(31) NOT NULL default ' ',
  "AAABETDH" varchar(31) NOT NULL default ' ',
  "AAABETDI" varchar(31) NOT NULL default ' ',
  "AAABETDJ" varchar(3) NOT NULL default '000',
  "AAABETDK" varchar(3) NOT NULL default '000',
  "AAABETDL" varchar(3) NOT NULL default '000',
  "AAABETDM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETDP" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETDV" (
  "AAABETET" varchar(3) NOT NULL default ' ',
  "AAABHPNG" varchar(1) NOT NULL default ' ',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABETDZ" varchar(2) NOT NULL default '00',
  "AAABLWOE" varchar(1) NOT NULL default '0',
  "AAABETEB" decimal(3,0) NOT NULL default '0',
  "AAABETEC" varchar(2) NOT NULL default ' ',
  "AAABETED" varchar(1) NOT NULL default ' ',
  "AAABETEE" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABETET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETEH" (
  "AAABETET" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABETET","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETEO" (
  "AAABETET" varchar(3) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABLZMT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABETET","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETEV" (
  "AAABETEZ" varchar(8) NOT NULL default ' ',
  "AAABETFA" varchar(1) NOT NULL default ' ',
  "AAABETEY" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABETEZ","AAABETFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETFB" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABETFE" varchar(30) NOT NULL default ' ',
  "AAABETFF" varchar(40) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABETFJ" varchar(6) NOT NULL default '000000',
  "AAABETFK" varchar(1) NOT NULL default ' ',
  "AAABLRQM" varchar(12) NOT NULL default ' ',
  "AAABLRQN" varchar(8) NOT NULL default '00000000',
  "AAABKRYU" varchar(6) NOT NULL default '000000',
  "AAABETFO" varchar(1) NOT NULL default ' ',
  "AAABETFP" varchar(1) NOT NULL default ' ',
  "AAABETFQ" varchar(1) NOT NULL default ' ',
  "AAABGBIE" varchar(16) NOT NULL default ' ',
  "AAABETFS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABETGN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABETGN","AAABKFCZ","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABETGN" varchar(4) NOT NULL default ' ',
  "AAABETGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABETGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETGO" (
  "AAABGBIE" varchar(16) NOT NULL default ' ',
  "AAABETGQ" varchar(1) NOT NULL default ' ',
  "AAABETGR" varchar(1) NOT NULL default ' ',
  "AAABETGS" varchar(1) NOT NULL default ' ',
  "AAABETGT" varchar(1) NOT NULL default ' ',
  "AAABETGU" varchar(16) NOT NULL default ' ',
  "AAABETGV" varchar(8) NOT NULL default ' ',
  "AAABETGW" varchar(1) NOT NULL default ' ',
  "AAABETGX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETGZ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGBIE" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABGBIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETHF" (
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABIMXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNJ"),
  KEY "AAABETHJ" ("AAABIMXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETHL" (
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABIMXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUNJ"),
  KEY "AAABETHP" ("AAABIMXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETHR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABILRJ" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXWV" varchar(2) NOT NULL default '00',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKDK" varchar(60) NOT NULL default ' ',
  "AAABKQYU" varchar(60) NOT NULL default ' ',
  "AAABETIJ" varchar(60) NOT NULL default ' ',
  "AAABLELF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABETIT" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSIH","AAABETIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETJC" varchar(1) NOT NULL default ' ',
  "AAABETJD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABETJI" varchar(3) NOT NULL default ' ',
  "AAABETJJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABETJY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABETJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETKY" varchar(4) NOT NULL default ' ',
  "AAABETKG" varchar(1) NOT NULL default ' ',
  "AAABETKD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETKY","AAABETKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETKY" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(1) NOT NULL default ' ',
  "AAABMCEY" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABKMUX" varchar(1) NOT NULL default ' ',
  "AAABLZMW" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(1) NOT NULL default ' ',
  "AAABFXHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKLQ" varchar(4) NOT NULL default ' ',
  "AAABETLC" varchar(1) NOT NULL default ' ',
  "AAABETLD" varchar(4) NOT NULL default '0000',
  "AAABETLE" varchar(2) NOT NULL default '00',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKLQ" varchar(4) NOT NULL default ' ',
  "AAABETMG" varchar(2) NOT NULL default '00',
  "AAABETMH" varchar(2) NOT NULL default '00',
  "AAABETLU" varchar(2) NOT NULL default ' ',
  "AAABJVLO" varchar(4) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKLQ","AAABETMG","AAABETMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABETMN" decimal(7,2) NOT NULL default '0.00',
  "AAABETMO" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLDBT","AAABMEFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABHKSM" varchar(4) NOT NULL default '0000',
  "AAABHKSN" varchar(4) NOT NULL default '0000',
  "AAABETMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMAMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(35) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABETNJ" varchar(34) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMREL" varchar(16) NOT NULL default ' ',
  "AAABLTOL" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWRR","AAABMGVL","AAABMGVM","AAABJWRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETNW" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETNY" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETOA" varchar(50) NOT NULL default ' ',
  "AAABETOB" varchar(10) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABETOD" varchar(3) NOT NULL default ' ',
  "AAABETOE" varchar(3) NOT NULL default ' ',
  "AAABINMN" varchar(1) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABETOH" int(11) NOT NULL default '0',
  "AAABETOI" varchar(5) NOT NULL default ' ',
  "AAABETOJ" int(11) NOT NULL default '0',
  "AAABETOK" int(11) NOT NULL default '0',
  "AAABETOL" smallint(6) NOT NULL default '0',
  "AAABETOM" int(11) NOT NULL default '0',
  "AAABETON" varchar(40) NOT NULL default ' ',
  "AAABETOO" varchar(30) NOT NULL default ' ',
  "AAABETOP" varchar(40) NOT NULL default ' ',
  "AAABETOQ" varchar(30) NOT NULL default ' ',
  "AAABETOR" varchar(40) NOT NULL default ' ',
  "AAABETOS" varchar(30) NOT NULL default ' ',
  "AAABETOT" varchar(1) NOT NULL default ' ',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" varchar(6) NOT NULL default '000000',
  "AAABETOW" varchar(3) NOT NULL default '000',
  "AAABETOX" int(11) NOT NULL default '0',
  "AAABETOY" varchar(10) NOT NULL default ' ',
  "AAABETOZ" varchar(10) NOT NULL default ' ',
  "AAABETPA" varchar(10) NOT NULL default ' ',
  "AAABETPB" varchar(30) NOT NULL default ' ',
  "AAABETPC" varchar(30) NOT NULL default ' ',
  "AAABETPD" varchar(30) NOT NULL default ' ',
  "AAABETPE" varchar(30) NOT NULL default ' ',
  "AAABETPF" varchar(30) NOT NULL default ' ',
  "AAABETPG" varchar(255) NOT NULL default ' ',
  "AAABETPH" varchar(255) NOT NULL default ' ',
  "AAABETPI" varchar(30) NOT NULL default ' ',
  "AAABETPJ" varchar(255) NOT NULL default ' ',
  "AAABETPK" varchar(30) NOT NULL default ' ',
  "AAABETPL" varchar(18) NOT NULL default ' ',
  "AAABETPM" varchar(20) NOT NULL default ' ',
  "AAABETQF" varchar(30) NOT NULL default ' ',
  "AAABETQG" varchar(30) NOT NULL default ' ',
  "AAABETQH" varchar(30) NOT NULL default ' ',
  "AAABETPQ" varchar(30) NOT NULL default ' ',
  "AAABETPR" varchar(30) NOT NULL default ' ',
  "AAABETPS" varchar(30) NOT NULL default ' ',
  "AAABETPT" varchar(40) NOT NULL default ' ',
  "AAABETPU" int(11) NOT NULL default '0',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABIACJ" varchar(12) NOT NULL default ' ',
  "AAABETPY" varchar(32) NOT NULL default ' ',
  "AAABETPZ" varchar(40) NOT NULL default ' ',
  "AAABETQA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETQC" (
  "AAABLGCM" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETQF" varchar(30) NOT NULL default ' ',
  "AAABETQG" varchar(30) NOT NULL default ' ',
  "AAABETQH" varchar(30) NOT NULL default ' ',
  "AAABETQI" smallint(6) NOT NULL default '0',
  "AAABETQJ" text,
  PRIMARY KEY ("AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAABETQP" varchar(80) NOT NULL default ' ',
  "AAABETQQ" varchar(132) NOT NULL default ' ',
  "AAABETQR" varchar(80) NOT NULL default ' ',
  "AAABETQS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABETRH" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABILWD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRA","AAABETRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAABEZHC" varchar(6) NOT NULL default ' ',
  "AAABIYYC" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBXV","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETRU" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABHAZA" varchar(16) NOT NULL default ' ',
  "AAABLEEW" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABHAZA","AAABLEEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETSD" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABLAXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETSI" (
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMEBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABETTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGG","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABETTC" varchar(4) NOT NULL default ' ',
  "AAABETTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGG","AAABLCPX","AAABETTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABETTM" varchar(4) NOT NULL default ' ',
  "AAABETTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGG","AAABLCPX","AAABETTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABETTQ" varchar(30) NOT NULL default ' ',
  "AAABIJAF" decimal(12,6) NOT NULL default '0.000000',
  "AAABETTS" varchar(1) NOT NULL default '0',
  "AAABETTT" varchar(2) NOT NULL default ' ',
  "AAABETTU" varchar(1) NOT NULL default ' ',
  "AAABKKBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZGG" varchar(15) NOT NULL default ' ',
  "AAABKWVK" varchar(8) NOT NULL default '00000000',
  "AAABKZRP" decimal(12,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABGZGG","AAABKWVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETUJ" varchar(1) NOT NULL default ' ',
  "AAABETUK" varchar(1) NOT NULL default ' ',
  "AAABETUL" varchar(1) NOT NULL default ' ',
  "AAABETUM" varchar(1) NOT NULL default ' ',
  "AAABETUN" varchar(1) NOT NULL default ' ',
  "AAABETUO" varchar(1) NOT NULL default ' ',
  "AAABKHFV" varchar(30) NOT NULL default ' ',
  "AAABETUQ" varchar(1) NOT NULL default ' ',
  "AAABETUR" varchar(1) NOT NULL default ' ',
  "AAABETUS" varchar(1) NOT NULL default ' ',
  "AAABETUT" varchar(1) NOT NULL default ' ',
  "AAABETUU" varchar(1) NOT NULL default ' ',
  "AAABETUV" varchar(1) NOT NULL default ' ',
  "AAABETUW" varchar(1) NOT NULL default ' ',
  "AAABETUX" varchar(1) NOT NULL default ' ',
  "AAABETUY" varchar(1) NOT NULL default ' ',
  "AAABETUZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBSA" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABKBSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETVW" varchar(1) NOT NULL default '0',
  "AAABETVX" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABETVW","AAABETVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABETVW" varchar(1) NOT NULL default '0',
  "AAABETVX" varchar(10) NOT NULL default '0000000000',
  "AAABETWS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABETVW","AAABETVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETWV" varchar(1) NOT NULL default '0',
  "AAABKBSA" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABETWV","AAABKBSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKBSA" varchar(10) NOT NULL default '0000000000',
  "AAABETWS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKBSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABETWV" varchar(1) NOT NULL default '0',
  "AAABKBSA" varchar(10) NOT NULL default '0000000000',
  "AAABETWS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABETWV","AAABKBSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCWA" varchar(4) NOT NULL default '0000',
  "AAABETXA" varchar(1) NOT NULL default ' ',
  "AAABETXO" varchar(2) NOT NULL default '00',
  "AAABETXC" varchar(2) NOT NULL default '00',
  "AAABLUTZ" varchar(1) NOT NULL default ' ',
  "AAABETXE" varchar(2) NOT NULL default '00',
  "AAABETXF" varchar(1) NOT NULL default ' ',
  "AAABETXG" varchar(1) NOT NULL default ' ',
  "AAABETXQ" varchar(1) NOT NULL default ' ',
  "AAABETXR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAZG" varchar(4) NOT NULL default '0000',
  "AAABETXO" varchar(2) NOT NULL default '00',
  "AAABETXP" varchar(1) NOT NULL default ' ',
  "AAABETXQ" varchar(1) NOT NULL default ' ',
  "AAABETXR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKAZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKAZG" varchar(4) NOT NULL default '0000',
  "AAABETXY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKAZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKCWA" varchar(4) NOT NULL default '0000',
  "AAABETYG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAZG" varchar(4) NOT NULL default '0000',
  "AAABETYQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKAZG","AAABETYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABFACD" varchar(100) NOT NULL default ' ',
  "AAABETZH" varchar(1) NOT NULL default ' ',
  "AAABETYW" varchar(1) NOT NULL default ' ',
  "AAABETZI" varchar(1) NOT NULL default ' ',
  "AAABETYY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABFACD" varchar(100) NOT NULL default ' ',
  "AAABETZF" varchar(1) NOT NULL default ' ',
  "AAABETZG" varchar(1) NOT NULL default ' ',
  "AAABETZH" varchar(1) NOT NULL default ' ',
  "AAABETZI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABETZX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABETZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABETZX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUAG" varchar(4) NOT NULL default '0000',
  "AAABEUAH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEUAG","AAABEUAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEVCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABEUAR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEVCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUCG" varchar(2) NOT NULL default '00',
  "AAABEUBJ" varchar(2) NOT NULL default '00',
  "AAABEUBK" varchar(2) NOT NULL default '00',
  "AAABKFWR" varchar(3) NOT NULL default '000',
  "AAABKFWS" varchar(3) NOT NULL default '000',
  "AAABEUBN" varchar(1) NOT NULL default ' ',
  "AAABEUBO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUCG" varchar(2) NOT NULL default '00',
  "AAABEUBJ" varchar(2) NOT NULL default '00',
  "AAABEUBK" varchar(2) NOT NULL default '00',
  "AAABKFWR" varchar(3) NOT NULL default '000',
  "AAABKFWS" varchar(3) NOT NULL default '000',
  "AAABEUBN" varchar(1) NOT NULL default ' ',
  "AAABEUBO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEUCG" varchar(2) NOT NULL default '00',
  "AAABEUCD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEUCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEUCG" varchar(2) NOT NULL default '00',
  "AAABEUCD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEUCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABKCWA" varchar(4) NOT NULL default '0000',
  "AAABIOYB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEVCX","AAABKBRY","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABKAZG" varchar(4) NOT NULL default '0000',
  "AAABIOYB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEVCX","AAABKBRY","AAABKAZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEVCX","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEVCX","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEVCX","AAABKBRY","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCWA" varchar(4) NOT NULL default '0000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKCWA","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBOI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABKBOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKBOI" varchar(10) NOT NULL default '0000000000',
  "AAABEUEO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKBOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHGC" varchar(2) NOT NULL default '00',
  "AAABKBRY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKHGC","AAABKBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBHD" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKBHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKBHD" varchar(16) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKBHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUGC" varchar(2) NOT NULL default '00',
  "AAABEUFP" varchar(2) NOT NULL default '00',
  "AAABEUFQ" varchar(2) NOT NULL default '00',
  "AAABKFWR" varchar(3) NOT NULL default '000',
  "AAABKFWS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEUGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEUGC" varchar(2) NOT NULL default '00',
  "AAABEUFZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEUGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCWA" varchar(4) NOT NULL default '0000',
  "AAABKBIV" varchar(2) NOT NULL default '00',
  "AAABEUGH" varchar(3) NOT NULL default ' ',
  "AAABEUGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCWA","AAABKBIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHGC" varchar(2) NOT NULL default '00',
  "AAABEUGQ" varchar(30) NOT NULL default ' ',
  "AAABEUGR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  "AAABKNGM" varchar(2) NOT NULL default ' ',
  "AAABFACD" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLIN","AAABKNGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABEUHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABJWBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABLDBT","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUIM" varchar(3) NOT NULL default '000',
  "AAABEUHW" varchar(1) NOT NULL default ' ',
  "AAABEUHX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEUIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABEUIM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLDBT","AAABEUIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUIM" varchar(3) NOT NULL default '000',
  "AAABEUIN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEUIM","AAABEUIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABLCXU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDSR" varchar(2) NOT NULL default '00',
  "AAABEUIZ" varchar(1) NOT NULL default ' ',
  "AAABEUJA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKDSR" varchar(2) NOT NULL default '00',
  "AAABEUJH" varchar(30) NOT NULL default ' ',
  "AAABEUJI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKDSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUJM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUMA" varchar(12) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUJS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUMA" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUMA","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUKA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUMX" varchar(12) NOT NULL default ' ',
  "AAABGPST" varchar(1) NOT NULL default ' ',
  "AAABLDLX" varchar(1) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUKI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUNF" varchar(12) NOT NULL default ' ',
  "AAABHNLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUKO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUNF" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUNF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUKW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEULW" varchar(12) NOT NULL default ' ',
  "AAABEUKZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEULW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEULC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEULK" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAABEULH" varchar(20) NOT NULL default ' ',
  "AAABEULI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEULK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEULM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEULR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABEUMA" varchar(12) NOT NULL default ' ',
  "AAABEUNF" varchar(12) NOT NULL default ' ',
  "AAABEULW" varchar(12) NOT NULL default ' ',
  "AAABEULX" decimal(13,0) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABEUMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUMB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABEUMX" varchar(12) NOT NULL default ' ',
  "AAABEUMF" varchar(12) NOT NULL default ' ',
  "AAABEUMG" varchar(12) NOT NULL default ' ',
  "AAABEUMH" varchar(12) NOT NULL default ' ',
  "AAABEUMI" varchar(12) NOT NULL default ' ',
  "AAABEUMJ" varchar(1) NOT NULL default ' ',
  "AAABEUMK" varchar(1) NOT NULL default ' ',
  "AAABEUML" varchar(1) NOT NULL default ' ',
  "AAABEUMM" varchar(1) NOT NULL default ' ',
  "AAABEUMN" varchar(6) NOT NULL default '000000',
  "AAABEUMO" varchar(1) NOT NULL default ' ',
  "AAABEUMP" varchar(8) NOT NULL default '00000000',
  "AAABEUMQ" varchar(1) NOT NULL default ' ',
  "AAABEUMR" varchar(6) NOT NULL default '000000',
  "AAABEUMS" varchar(1) NOT NULL default ' ',
  "AAABEUMT" varchar(8) NOT NULL default '00000000',
  "AAABEUMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABEUMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUMY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABEUNF" varchar(12) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABEUNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUNG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABJEVU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABJEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUNN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUVS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUNV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTRT" VARBINARY(255) NOT NULL,
  "AAABEUQB" varchar(12) NOT NULL default ' ',
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  "AAABEUPL" varchar(12) NOT NULL default ' ',
  "AAABEUPM" decimal(15,0) NOT NULL default '0',
  "AAABGWLH" varchar(12) NOT NULL default ' ',
  "AAABGWLJ" decimal(15,0) NOT NULL default '0',
  "AAABMPMU" varchar(12) NOT NULL default ' ',
  "AAABMPMW" decimal(15,0) NOT NULL default '0',
  "AAABLTQF" VARBINARY(255),
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABFTRT"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUOK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFTRT" VARBINARY(255) NOT NULL,
  "AAABEUQC" varchar(6) NOT NULL default '000000',
  "AAABEUPZ" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABFTRT"(255),"AAABEUQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUOS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUQB" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUQB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUPA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUQB" varchar(12) NOT NULL default ' ',
  "AAABFTRT" VARBINARY(255),
  PRIMARY KEY ("AAABLUXQ","AAABEUQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUPG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUQB" varchar(12) NOT NULL default ' ',
  "AAABFTRT" VARBINARY(255),
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  "AAABEUPL" varchar(12) NOT NULL default ' ',
  "AAABEUPM" decimal(15,0) NOT NULL default '0',
  "AAABGWLH" varchar(12) NOT NULL default ' ',
  "AAABGWLJ" decimal(15,0) NOT NULL default '0',
  "AAABMPMU" varchar(12) NOT NULL default ' ',
  "AAABMPMW" decimal(15,0) NOT NULL default '0',
  "AAABEUPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUPU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEUQB" varchar(12) NOT NULL default ' ',
  "AAABEUQC" varchar(6) NOT NULL default '000000',
  "AAABFTRT" VARBINARY(255),
  "AAABEUPZ" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEUQB","AAABEUQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUQD" (
  "AAABMDEP" varchar(60) NOT NULL default ' ',
  "AAABEUQF" varchar(8) NOT NULL default '00000000',
  "AAABEUQG" varchar(8) NOT NULL default '00000000',
  "AAABEUQH" smallint(6) NOT NULL default '0',
  "AAABEUQI" varchar(8) NOT NULL default '00000000',
  "AAABEUQJ" varchar(8) NOT NULL default '00000000',
  "AAABEUQK" smallint(6) NOT NULL default '0',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABKNUR" tinyint(3) unsigned NOT NULL default '0',
  "AAABMJVD" varchar(20) NOT NULL default ' ',
  "AAABMJVE" varchar(20) NOT NULL default ' ',
  "AAABMJVF" varchar(20) NOT NULL default ' ',
  "AAABJRBM" varchar(50) NOT NULL default ' ',
  "AAABJRAT" varchar(50) NOT NULL default ' ',
  "AAABEUQS" varchar(50) NOT NULL default ' ',
  "AAABEUQT" varchar(50) NOT NULL default ' ',
  "AAABEUQU" varchar(50) NOT NULL default ' ',
  "AAABEUQV" varchar(8) NOT NULL default '00000000',
  "AAABEUQW" varchar(12) NOT NULL default ' ',
  "AAABEUQX" varchar(8) NOT NULL default '00000000',
  "AAABMBYY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEURA" (
  "AAABMDEP" varchar(60) NOT NULL default ' ',
  "AAABHEIS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABHEIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEURF" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(20) NOT NULL default ' ',
  "AAABEURQ" varchar(32) NOT NULL default ' ',
  "AAABEURS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEURM" (
  "AAABHEHZ" varchar(4) NOT NULL default ' ',
  "AAABHEIC" varchar(60) NOT NULL default ' ',
  "AAABEURP" varchar(12) NOT NULL default ' ',
  "AAABEURQ" varchar(32) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABEURS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEHZ","AAABHEIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEURV" (
  "AAABEUSJ" varchar(30) NOT NULL default ' ',
  "AAABEURX" varchar(30) NOT NULL default ' ',
  "AAABEURY" varchar(30) NOT NULL default ' ',
  "AAABEURZ" varchar(30) NOT NULL default ' ',
  "AAABEUSA" varchar(30) NOT NULL default ' ',
  "AAABEUSB" varchar(30) NOT NULL default ' ',
  "AAABEUSC" varchar(3) NOT NULL default ' ',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(10) NOT NULL default ' ',
  "AAABIWJO" varchar(8) NOT NULL default '00000000',
  "AAABEUSG" varchar(6) NOT NULL default '000000',
  "AAABIJTA" varchar(30) NOT NULL default ' ',
  "AAABHMNR" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABEUSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  "AAABEUSU" varchar(1) NOT NULL default ' ',
  "AAABJDAS" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(1) NOT NULL default ' ',
  "AAABEUSQ" varchar(4) NOT NULL default ' ',
  "AAABLTYS" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTVR"(255),"AAABEUSU","AAABJDAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYIG" varchar(3) NOT NULL default ' ',
  "AAABEUSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYIG" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUTK" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUTP" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABHAZA" varchar(16) NOT NULL default ' ',
  "AAABGBHU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABHAZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUTX" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABEUUB" varchar(30) NOT NULL default ' ',
  "AAABEUUC" varchar(4) NOT NULL default ' ',
  "AAABEUUD" varchar(4) NOT NULL default ' ',
  "AAABEUUE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUUG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUUM" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABEUUQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUUT" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHAYY","AAABHAYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEUVI" varchar(10) NOT NULL default ' ',
  "AAABEUVF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABEUVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABEUWK" varchar(1) NOT NULL default '0',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABEUVR" decimal(13,2) NOT NULL default '0.00',
  "AAABEUVS" varchar(8) NOT NULL default '00000000',
  "AAABEUVT" varchar(10) NOT NULL default ' ',
  "AAABEUVU" varchar(10) NOT NULL default ' ',
  "AAABEUVV" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEUVX" decimal(13,2) NOT NULL default '0.00',
  "AAABEUVY" varchar(2) NOT NULL default ' ',
  "AAABEUWM" varchar(1) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABEUWC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABEUWE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMREX","AAABMDBA","AAABMDBB","AAABEUWK"),
  KEY "AAABEUWL" ("AAABEUWM","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(13) NOT NULL default ' ',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXW","AAABEUWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKHNS" varchar(1) NOT NULL default ' ',
  "AAABEUXB" varchar(3) NOT NULL default '000',
  "AAABEUXC" varchar(2) NOT NULL default '00',
  "AAABEUXD" varchar(3) NOT NULL default '000',
  "AAABEUXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKHOL","AAABKHNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEUXM" decimal(2,0) NOT NULL default '0',
  "AAABEUXN" decimal(2,0) NOT NULL default '0',
  "AAABEUXO" decimal(2,0) NOT NULL default '0',
  "AAABKELQ" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABEUXR" varchar(2) NOT NULL default ' ',
  "AAABEUXS" decimal(2,0) NOT NULL default '0',
  "AAABEUXT" varchar(1) NOT NULL default ' ',
  "AAABEUXU" varchar(2) NOT NULL default '00',
  "AAABEUXV" varchar(1) NOT NULL default ' ',
  "AAABEUXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABEUYC" decimal(2,0) NOT NULL default '0',
  "AAABEUYD" decimal(7,2) NOT NULL default '0.00',
  "AAABEUYE" decimal(7,2) NOT NULL default '0.00',
  "AAABEUYF" decimal(4,2) NOT NULL default '0.00',
  "AAABEUYG" varchar(8) NOT NULL default '00000000',
  "AAABEUYH" decimal(2,0) NOT NULL default '0',
  "AAABEUYI" decimal(7,2) NOT NULL default '0.00',
  "AAABEUYJ" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEUYL" decimal(9,2) NOT NULL default '0.00',
  "AAABEUYM" varchar(3) NOT NULL default '000',
  "AAABEUYN" varchar(2) NOT NULL default '00',
  "AAABEUYO" varchar(4) NOT NULL default '0000',
  "AAABEUYP" varchar(4) NOT NULL default '0000',
  "AAABEUYQ" decimal(2,0) NOT NULL default '0',
  "AAABEUYR" decimal(5,2) NOT NULL default '0.00',
  "AAABEUYS" decimal(5,2) NOT NULL default '0.00',
  "AAABEUYT" varchar(3) NOT NULL default '000',
  "AAABEUYU" varchar(3) NOT NULL default '000',
  "AAABEUYV" varchar(4) NOT NULL default '0000',
  "AAABEUYW" varchar(4) NOT NULL default '0000',
  "AAABEUYX" varchar(1) NOT NULL default ' ',
  "AAABEUYY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABEUZH" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKIFN","AAABLDBT","AAABMEFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEUZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEUZQ" varchar(3) NOT NULL default ' ',
  "AAABEUZR" varchar(3) NOT NULL default ' ',
  "AAABEUZS" varchar(1) NOT NULL default '0',
  "AAABEUZT" varchar(1) NOT NULL default ' ',
  "AAABKFEX" varchar(3) NOT NULL default ' ',
  "AAABEUZV" varchar(1) NOT NULL default ' ',
  "AAABEUZW" varchar(1) NOT NULL default ' ',
  "AAABEUZX" varchar(1) NOT NULL default ' ',
  "AAABIEWX" varchar(1) NOT NULL default ' ',
  "AAABIDVX" varchar(1) NOT NULL default ' ',
  "AAABEVAA" varchar(1) NOT NULL default ' ',
  "AAABEVAB" varchar(1) NOT NULL default ' ',
  "AAABEVAC" varchar(1) NOT NULL default ' ',
  "AAABEVAD" decimal(13,2) NOT NULL default '0.00',
  "AAABEVAE" decimal(13,2) NOT NULL default '0.00',
  "AAABEVAF" varchar(1) NOT NULL default ' ',
  "AAABEVAG" varchar(1) NOT NULL default '0',
  "AAABEVAH" varchar(1) NOT NULL default '0',
  "AAABEVAI" varchar(1) NOT NULL default '0',
  "AAABEVAJ" varchar(1) NOT NULL default ' ',
  "AAABEVAK" varchar(1) NOT NULL default ' ',
  "AAABEVAL" varchar(1) NOT NULL default ' ',
  "AAABEVAM" varchar(1) NOT NULL default ' ',
  "AAABEVAN" varchar(1) NOT NULL default ' ',
  "AAABKHHH" varchar(1) NOT NULL default ' ',
  "AAABKHHI" decimal(5,2) NOT NULL default '0.00',
  "AAABEVAQ" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABEVAS" decimal(9,6) NOT NULL default '0.000000',
  "AAABEVAT" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABKEFQ" varchar(1) NOT NULL default ' ',
  "AAABEVAW" varchar(2) NOT NULL default ' ',
  "AAABJRIT" varchar(4) NOT NULL default ' ',
  "AAABEVAY" varchar(1) NOT NULL default ' ',
  "AAABEVAZ" varchar(1) NOT NULL default ' ',
  "AAABEVBA" varchar(1) NOT NULL default ' ',
  "AAABEVBB" varchar(1) NOT NULL default ' ',
  "AAABEVBC" varchar(2) NOT NULL default ' ',
  "AAABEVBD" varchar(2) NOT NULL default ' ',
  "AAABEVBE" varchar(2) NOT NULL default ' ',
  "AAABEVBF" varchar(2) NOT NULL default ' ',
  "AAABEVBG" varchar(1) NOT NULL default ' ',
  "AAABEVBH" varchar(1) NOT NULL default ' ',
  "AAABEVBI" varchar(1) NOT NULL default ' ',
  "AAABEVBJ" varchar(1) NOT NULL default ' ',
  "AAABEVBK" decimal(9,6) NOT NULL default '0.000000',
  "AAABEVBL" varchar(1) NOT NULL default ' ',
  "AAABEVBM" varchar(1) NOT NULL default ' ',
  "AAABEVBN" varchar(2) NOT NULL default ' ',
  "AAABEVBO" varchar(1) NOT NULL default ' ',
  "AAABEVBP" varchar(2) NOT NULL default ' ',
  "AAABEVBQ" varchar(1) NOT NULL default ' ',
  "AAABEVBR" varchar(1) NOT NULL default ' ',
  "AAABEVBS" varchar(1) NOT NULL default ' ',
  "AAABEVBT" varchar(1) NOT NULL default '0',
  "AAABEVBU" varchar(1) NOT NULL default '0',
  "AAABEVBV" varchar(1) NOT NULL default '0',
  "AAABEVBW" varchar(1) NOT NULL default '0',
  "AAABEVBX" varchar(5) NOT NULL default ' ',
  "AAABEVBY" varchar(1) NOT NULL default ' ',
  "AAABEVBZ" varchar(1) NOT NULL default ' ',
  "AAABEVCA" varchar(20) NOT NULL default ' ',
  "AAABEVCB" varchar(1) NOT NULL default '0',
  "AAABEVCC" varchar(1) NOT NULL default '0',
  "AAABEVCD" varchar(30) NOT NULL default ' ',
  "AAABEVCE" varchar(1) NOT NULL default ' ',
  "AAABEVCF" varchar(1) NOT NULL default ' ',
  "AAABEVCG" varchar(1) NOT NULL default ' ',
  "AAABEVCH" varchar(1) NOT NULL default ' ',
  "AAABEVCI" varchar(3) NOT NULL default ' ',
  "AAABEVCJ" varchar(3) NOT NULL default ' ',
  "AAABEVCK" varchar(3) NOT NULL default ' ',
  "AAABEVCL" varchar(3) NOT NULL default ' ',
  "AAABJVYW" varchar(1) NOT NULL default ' ',
  "AAABEVCN" varchar(3) NOT NULL default ' ',
  "AAABEVCO" varchar(3) NOT NULL default ' ',
  "AAABEVCP" varchar(1) NOT NULL default '0',
  "AAABEVCQ" varchar(3) NOT NULL default ' ',
  "AAABEVCR" varchar(20) NOT NULL default ' ',
  "AAABEVCS" varchar(1) NOT NULL default ' ',
  "AAABKINQ" varchar(4) NOT NULL default ' ',
  "AAABFDWC" varchar(1) NOT NULL default ' ',
  "AAABFDWD" varchar(1) NOT NULL default ' ',
  "AAABEVCW" varchar(3) NOT NULL default ' ',
  "AAABEVCX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEVDD" varchar(1) NOT NULL default ' ',
  "AAABEVDE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCMT" varchar(1) NOT NULL default ' ',
  "AAABEVDK" varchar(1) NOT NULL default ' ',
  "AAABEVDL" varchar(2) NOT NULL default '00',
  "AAABEVDM" varchar(50) NOT NULL default ' ',
  "AAABEVDN" varchar(20) NOT NULL default ' ',
  "AAABEVDO" varchar(20) NOT NULL default ' ',
  "AAABEVDP" varchar(20) NOT NULL default ' ',
  "AAABEVDQ" varchar(20) NOT NULL default ' ',
  "AAABEVDR" varchar(1) NOT NULL default ' ',
  "AAABEVDS" varchar(5) NOT NULL default ' ',
  "AAABKEFQ" varchar(1) NOT NULL default ' ',
  "AAABEVDU" varchar(1) NOT NULL default ' ',
  "AAABEVDV" varchar(1) NOT NULL default ' ',
  "AAABEVDW" varchar(1) NOT NULL default ' ',
  "AAABEVDX" varchar(1) NOT NULL default ' ',
  "AAABEVDY" varchar(1) NOT NULL default ' ',
  "AAABEVDZ" varchar(1) NOT NULL default ' ',
  "AAABEVEA" varchar(1) NOT NULL default ' ',
  "AAABEVEB" varchar(1) NOT NULL default ' ',
  "AAABEVEC" varchar(60) NOT NULL default ' ',
  "AAABEVED" varchar(60) NOT NULL default ' ',
  "AAABEVEE" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABEVEG" varchar(2) NOT NULL default ' ',
  "AAABEVEH" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKINQ" varchar(4) NOT NULL default ' ',
  "AAABEVEK" varchar(1) NOT NULL default ' ',
  "AAABEVEL" varchar(1) NOT NULL default ' ',
  "AAABEVEM" varchar(1) NOT NULL default ' ',
  "AAABEVEN" varchar(1) NOT NULL default ' ',
  "AAABEVEO" varchar(1) NOT NULL default ' ',
  "AAABEVEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVES" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABEVEY" varchar(5) NOT NULL default ' ',
  "AAABGBHU" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABEVEY","AAABGBHU","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVFB" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVFG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVFL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVFT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABEVFX" varchar(30) NOT NULL default ' ',
  "AAABEVFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVGC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABLECD","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVGM" (
  "AAABIDWD" varchar(2) NOT NULL default ' ',
  "AAABLBXJ" varchar(6) NOT NULL default ' ',
  "AAABEVGP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABIDWD","AAABLBXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVGY" (
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABEVII" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEVIQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
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
  "AAABEVSW" varchar(1) NOT NULL default ' ',
  "AAABEXEX" varchar(1) NOT NULL default ' ',
  "AAABEVJX" varchar(1) NOT NULL default ' ',
  "AAABEWMV" varchar(60) NOT NULL default ' ',
  "AAABEVJZ" varchar(30) NOT NULL default ' ',
  "AAABEVKA" varchar(30) NOT NULL default ' ',
  "AAABEWMW" varchar(15) NOT NULL default ' ',
  "AAABEWMX" varchar(15) NOT NULL default ' ',
  "AAABEVKD" varchar(15) NOT NULL default ' ',
  "AAABEVKE" varchar(60) NOT NULL default ' ',
  "AAABEVKF" varchar(60) NOT NULL default ' ',
  "AAABEVKG" varchar(60) NOT NULL default ' ',
  "AAABEVKH" varchar(60) NOT NULL default ' ',
  "AAABEVKI" varchar(60) NOT NULL default ' ',
  "AAABEVKJ" varchar(60) NOT NULL default ' ',
  "AAABEVKK" varchar(60) NOT NULL default ' ',
  "AAABEVKL" varchar(60) NOT NULL default ' ',
  "AAABEVKM" varchar(60) NOT NULL default ' ',
  "AAABEVKN" varchar(15) NOT NULL default ' ',
  "AAABEVKO" varchar(15) NOT NULL default ' ',
  "AAABEVKP" varchar(15) NOT NULL default ' ',
  "AAABEVKQ" varchar(15) NOT NULL default ' ',
  "AAABEVKR" varchar(15) NOT NULL default ' ',
  "AAABEVKS" varchar(60) NOT NULL default ' ',
  "AAABEVKT" varchar(60) NOT NULL default ' ',
  "AAABEVKU" varchar(60) NOT NULL default ' ',
  "AAABEVKV" varchar(60) NOT NULL default ' ',
  "AAABEVKW" varchar(60) NOT NULL default ' ',
  "AAABEVKX" varchar(60) NOT NULL default ' ',
  "AAABEVKY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY","AAABGUBO","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGLWX" varchar(40) NOT NULL default ' ',
  "AAABEVLI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVLL" (
  "AAABEVLO" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEVLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEZMJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEVNZ" varchar(7) NOT NULL default ' ',
  "AAABEVOA" varchar(7) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEZXF","AAABEVNZ","AAABEVOA"),
  KEY "AAABEVOB" ("AAABMRIL","AAABHSAY","AAABEYWM","AAABGVMC","AAABGYJY","AAABEZXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVSW" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEVSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEWJO" varchar(2) NOT NULL default ' ',
  "AAABEVRJ" varchar(2) NOT NULL default ' ',
  "AAABEVSP" varchar(3) NOT NULL default ' ',
  "AAABEVRL" varchar(1) NOT NULL default ' ',
  "AAABEVRM" varchar(1) NOT NULL default ' ',
  "AAABEVRN" varchar(1) NOT NULL default ' ',
  "AAABEVRO" varchar(1) NOT NULL default ' ',
  "AAABEXEF" varchar(2) NOT NULL default ' ',
  "AAABEVPW" varchar(1) NOT NULL default ' ',
  "AAABEWUX" varchar(1) NOT NULL default ' ',
  "AAABEWHG" varchar(2) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  "AAABEVQB" varchar(1) NOT NULL default ' ',
  "AAABEVRR" varchar(1) NOT NULL default ' ',
  "AAABEVRS" varchar(1) NOT NULL default ' ',
  "AAABLIMM" varchar(2) NOT NULL default ' ',
  "AAABEVRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY"),
  KEY "AAABEVQM" ("AAABMRIL","AAABHSAY","AAABEYWM","AAABGVMC","AAABGYJY"),
  KEY "AAABEVQS" ("AAABMRIL","AAABHSAY","AAABGVMC"),
  KEY "AAABEVQW" ("AAABMRIL","AAABEVSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEWJO" varchar(2) NOT NULL default ' ',
  "AAABEVRJ" varchar(2) NOT NULL default ' ',
  "AAABEVSP" varchar(3) NOT NULL default ' ',
  "AAABEVRL" varchar(1) NOT NULL default ' ',
  "AAABEVRM" varchar(1) NOT NULL default ' ',
  "AAABEVRN" varchar(1) NOT NULL default ' ',
  "AAABEVRO" varchar(1) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  "AAABEVRR" varchar(1) NOT NULL default ' ',
  "AAABEVRS" varchar(1) NOT NULL default ' ',
  "AAABLIMM" varchar(2) NOT NULL default ' ',
  "AAABEVRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABLCPX"),
  KEY "AAABEVSD" ("AAABMRIL","AAABHSAY","AAABEYWM","AAABGVMC","AAABGYJY"),
  KEY "AAABEVSJ" ("AAABMRIL","AAABHSAY","AAABGVMC"),
  KEY "AAABEVSN" ("AAABMRIL","AAABEVSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVSW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEVSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVTQ" varchar(1) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABEVTN" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEVTQ","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEXFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEXFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEXFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEVXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEVXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVXF" varchar(5) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEVXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEVYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEVYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVYG" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEVYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEVZI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF","AAABEVZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVZI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEVZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEVZI" varchar(10) NOT NULL default ' ',
  "AAABEWAE" varchar(120) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEVZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEWAK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF","AAABEWAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWAK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEVZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWAK" varchar(10) NOT NULL default ' ',
  "AAABEWAE" varchar(120) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYTV","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXA" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZXB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXB" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZWN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZWN" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEWEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWEX" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEWFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEWFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWFY" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWHG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWHG" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEWHG" varchar(2) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEWHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWIO" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEWIO" varchar(2) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEWIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLAFA" varchar(1) NOT NULL default '0',
  "AAABEWIT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLAFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWJO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWJO" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEWKE" varchar(10) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEWKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXCI","AAABLCPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLIMM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLIMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLIMM" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABLIMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWLO" varchar(1) NOT NULL default ' ',
  "AAABEWLP" varchar(1) NOT NULL default ' ',
  "AAABEWLQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIDWD" varchar(2) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIDWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIDWD" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIDWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEWMK" varchar(1) NOT NULL default ' ',
  "AAABKGXT" varchar(1) NOT NULL default ' ',
  "AAABEWMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABEWMV" varchar(60) NOT NULL default ' ',
  "AAABEWMW" varchar(60) NOT NULL default ' ',
  "AAABEWMX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEWNE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVC","AAABEZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZVC","AAABEZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSAY","AAABEZVC","AAABEZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  "AAABMOHE" varchar(15) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZVC","AAABEZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABLBYJ" varchar(2) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVC","AAABLBYJ","AAABEZVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAABEWQI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVC","AAABEWQH","AAABEWQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABGVMC","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLBYI" varchar(2) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLBYI","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABKMQV" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(50) NOT NULL default ' ',
  "AAABHHTI" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABLESS","AAABKMQV","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABIMFT" varchar(30) NOT NULL default ' ',
  "AAABLAUO" varchar(4) NOT NULL default ' ',
  "AAABEWRU" varchar(3) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABIMFT","AAABLAUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABIMFT" varchar(30) NOT NULL default ' ',
  "AAABLAUO" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABIMFT","AAABLAUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABIMFT" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEWTM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABIMFT","AAABMREF","AAABEWTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABIMFT" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEWTM" varchar(30) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABIMFT","AAABMREF","AAABEWTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEXWS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWUO" varchar(8) NOT NULL default '00000000',
  "AAABEWUJ" varchar(1) NOT NULL default ' ',
  "AAABEWUK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWUO" varchar(8) NOT NULL default '00000000',
  "AAABEWUJ" varchar(1) NOT NULL default ' ',
  "AAABEWUK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY","AAABEWUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEWUX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMREX","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWVI" varchar(1) NOT NULL default ' ',
  "AAABEWVJ" varchar(1) NOT NULL default ' ',
  "AAABEWVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWVN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEWWR" varchar(10) NOT NULL default ' ',
  "AAABEWWP" varchar(15) NOT NULL default ' ',
  "AAABJLTX" varchar(90) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTOL" varchar(70) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABILGE" varchar(1) NOT NULL default ' ',
  "AAABFOUX" varchar(254) NOT NULL default ' ',
  "AAABFOUW" varchar(254) NOT NULL default ' ',
  "AAABEWWB" varchar(8) NOT NULL default '00000000',
  "AAABEWWC" varchar(8) NOT NULL default '00000000',
  "AAABEWWD" varchar(8) NOT NULL default '00000000',
  "AAABEWWE" varchar(8) NOT NULL default '00000000',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABEYWM","AAABLCPX","AAABMREX","AAABEWWR"),
  KEY "AAABEWWN" ("AAABMONG","AAABEWWP","AAABJLTX","AAABEWWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWXZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWXO" varchar(2) NOT NULL default '00',
  "AAABEWXZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWXO" varchar(2) NOT NULL default ' ',
  "AAABEWXZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEWXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEWXZ" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEWXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAABGUFU" varchar(1) NOT NULL default ' ',
  "AAABEWYE" varchar(10) NOT NULL default ' ',
  "AAABMKAH" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABEXGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAABEXGF" varchar(1) NOT NULL default '0',
  "AAABEWZC" varchar(30) NOT NULL default ' ',
  "AAABEWZD" varchar(30) NOT NULL default ' ',
  "AAABEWZE" varchar(30) NOT NULL default ' ',
  "AAABEWZF" varchar(1) NOT NULL default ' ',
  "AAABEWZG" varchar(6) NOT NULL default '000000',
  "AAABEWYR" varchar(30) NOT NULL default ' ',
  "AAABEWYS" varchar(30) NOT NULL default ' ',
  "AAABEWYT" varchar(30) NOT NULL default ' ',
  "AAABEWYU" varchar(30) NOT NULL default ' ',
  "AAABEWYV" varchar(1) NOT NULL default ' ',
  "AAABEWYW" varchar(6) NOT NULL default '000000',
  "AAABGMZS" varchar(3) NOT NULL default '000',
  "AAABEWYY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXGD","AAABEXGF","AAABEWZC","AAABEWZD","AAABEWZE","AAABEWZF","AAABEWZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXBV" varchar(8) NOT NULL default ' ',
  "AAABEWZK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXBV" varchar(8) NOT NULL default ' ',
  "AAABEWZQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXBV" varchar(8) NOT NULL default ' ',
  "AAABEWZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEWZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXCI","AAABMQCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGUBO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGVMC","AAABGUBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABKSZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGVMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXBH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXBH" varchar(15) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXBV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXBV" varchar(8) NOT NULL default ' ',
  "AAABKSZO" varchar(60) NOT NULL default ' ',
  "AAABIEYQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXCI" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZBQ" varchar(3) NOT NULL default ' ',
  "AAABEXCU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEZBQ","AAABEXCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZBQ" varchar(3) NOT NULL default ' ',
  "AAABEXCX" varchar(3) NOT NULL default '000',
  "AAABEXCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZBQ","AAABEXCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXEF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEXEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXEF" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEXEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEXEF" varchar(2) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEXEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEXEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXEX" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEXEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXFH" varchar(4) NOT NULL default ' ',
  "AAABEXFI" varchar(5) NOT NULL default ' ',
  "AAABEXFJ" varchar(7) NOT NULL default '0000000',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEXFE" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXFH","AAABEXFI","AAABEXFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXFP" varchar(30) NOT NULL default ' ',
  "AAABEXFN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEXFT" varchar(1) NOT NULL default ' ',
  "AAABEXFU" varchar(8) NOT NULL default ' ',
  "AAABEXFV" varchar(4) NOT NULL default ' ',
  "AAABEXFW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXGD" varchar(6) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABEXGF" varchar(1) NOT NULL default '0',
  "AAABEXGG" varchar(30) NOT NULL default ' ',
  "AAABEXGH" varchar(1) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABEXGJ" varchar(1) NOT NULL default ' ',
  "AAABEXGK" varchar(2) NOT NULL default ' ',
  "AAABGAUJ" varchar(2) NOT NULL default ' ',
  "AAABGAVG" varchar(10) NOT NULL default ' ',
  "AAABEXMG" varchar(1) NOT NULL default ' ',
  "AAABEXGO" varchar(1) NOT NULL default ' ',
  "AAABEXGP" varchar(1) NOT NULL default ' ',
  "AAABEXGQ" varchar(1) NOT NULL default ' ',
  "AAABEXGR" varchar(2) NOT NULL default ' ',
  "AAABEXGS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGXQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXNI" varchar(10) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXNI","AAABLRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXKF" varchar(2) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXKF","AAABMNML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRPK","AAABEYYQ","AAABEXQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEXKF" varchar(2) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABEXIH" varchar(4) NOT NULL default '0000',
  "AAABEXII" varchar(1) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABEXIK" varchar(5) NOT NULL default ' ',
  "AAABEXIL" varchar(5) NOT NULL default ' ',
  "AAABEXIM" varchar(2) NOT NULL default ' ',
  "AAABMCOH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABEXKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABFHBJ" varchar(1) NOT NULL default ' ',
  "AAABEXIW" varchar(1) NOT NULL default ' ',
  "AAABEXIX" varchar(1) NOT NULL default ' ',
  "AAABEXIY" varchar(1) NOT NULL default ' ',
  "AAABEXIZ" varchar(1) NOT NULL default ' ',
  "AAABEXJA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXWT" varchar(15) NOT NULL default ' ',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXKF" varchar(2) NOT NULL default ' ',
  "AAABFHBJ" varchar(1) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXKF" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFHBJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFHBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFHBJ" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFHBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEXLI" varchar(10) NOT NULL default ' ',
  "AAABEXLF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEXLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABEXLT" varchar(20) NOT NULL default ' ',
  "AAABEXLU" varchar(1) NOT NULL default ' ',
  "AAABEXLP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLDAZ","AAABEXLT","AAABEXLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABEXMA" varchar(3) NOT NULL default ' ',
  "AAABEXMB" varchar(1) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABEXME" varchar(1) NOT NULL default ' ',
  "AAABEXMF" varchar(1) NOT NULL default ' ',
  "AAABEXMG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXNI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXNI" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABLNEE" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTWG","AAABLNEE","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXON" varchar(4) NOT NULL default '0000',
  "AAABEXPO" varchar(8) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXON","AAABEXPO","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXPF" varchar(20) NOT NULL default ' ',
  "AAABEXPO" varchar(8) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXPF","AAABEXPO","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXPO" varchar(8) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXPO","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXPZ" varchar(3) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXPZ","AAABMPLS"),
  KEY "AAABEXPY" ("AAABEXPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXQH" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABEXQF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXQH","AAABLDCD","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEXQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEXQW" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEXQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAABGTIX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGXQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY","AAABGXQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABFVMB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKV" varchar(2) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  "AAABHMMT" varchar(5) NOT NULL default '00000',
  "AAABLQDN" varchar(1) NOT NULL default ' ',
  "AAABEYIJ" varchar(14) NOT NULL default ' ',
  "AAABEXTE" varchar(12) NOT NULL default ' ',
  "AAABEXTF" varchar(12) NOT NULL default ' ',
  "AAABEZGI" varchar(40) NOT NULL default ' ',
  "AAABEZGJ" varchar(40) NOT NULL default ' ',
  "AAABEYIY" varchar(60) NOT NULL default ' ',
  "AAABEXSU" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYKV","AAABMBXQ","AAABEYKQ","AAABHMMT","AAABLQDN","AAABEYIJ","AAABEXTE","AAABEXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABJLFF" varchar(2) NOT NULL default '00',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABJLFF","AAABEYFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXTR" (
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
CREATE TABLE "AAABEXUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABEYCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEYPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABEXVV" varchar(30) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQGL","AAABEXVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEXWP" varchar(30) NOT NULL default ' ',
  "AAABEXWQ" varchar(1) NOT NULL default ' ',
  "AAABEXWR" varchar(1) NOT NULL default ' ',
  "AAABEXWS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEXWP" varchar(30) NOT NULL default ' ',
  "AAABEXWQ" varchar(1) NOT NULL default ' ',
  "AAABEXWR" varchar(1) NOT NULL default ' ',
  "AAABEXWS" varchar(3) NOT NULL default '000',
  "AAABEXWT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLQGL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABJLFF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABMREF","AAABJLFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABEYQI" varchar(2) NOT NULL default '00',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABEYST" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYFR","AAABMCMG","AAABEYQI","AAABMQBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABEYQI" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABEYST" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYFR","AAABMCMG","AAABEYQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEXZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABEYQI" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABEYST" varchar(30) NOT NULL default ' ',
  "AAABIMAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYFR","AAABMCMG","AAABEYQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYAO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEYAO" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEYAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABMRGO" smallint(6) NOT NULL default '0',
  "AAABEYBJ" varchar(70) NOT NULL default ' ',
  "AAABEYBK" varchar(70) NOT NULL default ' ',
  "AAABEYBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABMREF","AAABMRGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABMRGO" varchar(4) NOT NULL default '0000',
  "AAABEYBJ" varchar(70) NOT NULL default ' ',
  "AAABEYBK" varchar(70) NOT NULL default ' ',
  "AAABEYBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABMREF","AAABMRGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYFR","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYCZ" varchar(1) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABEYCZ","AAABLMSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYCZ" varchar(1) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABEYCZ","AAABLMSB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYJK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYDY" varchar(10) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYDY","AAABEYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYJK" varchar(8) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABMCMG" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(234) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABMREF","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFR" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMBXQ","AAABEYFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYFW" varchar(30) NOT NULL default ' ',
  "AAABLRQP" varchar(4) NOT NULL default ' ',
  "AAABEYFY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABEYOW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYHO" varchar(1) NOT NULL default ' ',
  "AAABEYHP" varchar(8) NOT NULL default ' ',
  "AAABEYHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKV" varchar(2) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  "AAABHMMT" varchar(5) NOT NULL default '00000',
  "AAABLQDN" varchar(1) NOT NULL default ' ',
  "AAABEYIJ" varchar(14) NOT NULL default ' ',
  "AAABEYIY" varchar(60) NOT NULL default ' ',
  "AAABEYIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKV","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKV" varchar(2) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEZGI" varchar(40) NOT NULL default ' ',
  "AAABEZGJ" varchar(40) NOT NULL default ' ',
  "AAABEYIY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYKV","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABJTGL" varchar(4) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  "AAABEYJK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABJTGL","AAABEYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKA" varchar(10) NOT NULL default ' ',
  "AAABEYKB" varchar(2) NOT NULL default '00',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKA","AAABEYKB","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYKH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYKV" varchar(2) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYKQ" varchar(5) NOT NULL default '00000',
  "AAABHMMT" varchar(5) NOT NULL default '00000',
  "AAABLQDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYKV","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYLV" varchar(2) NOT NULL default ' ',
  "AAABEYLW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABEYST" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYPA","AAABLMSB","AAABEYNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  "AAABLMSB" varchar(30) NOT NULL default ' ',
  "AAABEYNJ" varchar(30) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABEYST" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABEYPA","AAABLMSB","AAABEYNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABJDRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABEYOW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABJDRC","AAABEYOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABJDRC" varchar(40) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABJDRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYOW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEYOW" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEYOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYPC" varchar(10) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYPK" varchar(30) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  "AAABEYPP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEYQI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABJTGL" varchar(4) NOT NULL default ' ',
  "AAABEYQZ" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLEKJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABJTGL","AAABEYQZ","AAABMREF","AAABLEKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYRS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYRS" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMBXQ" varchar(10) NOT NULL default ' ',
  "AAABJTGL" varchar(4) NOT NULL default ' ',
  "AAABEYSP" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEYST" varchar(30) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABEYSK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMBXQ","AAABJTGL","AAABEYSP","AAABMPMH","AAABMREL","AAABMREF","AAABEYST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEYSZ" varchar(24) NOT NULL default ' ',
  "AAABEYTA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEYSZ","AAABEYTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYTV","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABICUY" varchar(4) NOT NULL default ' ',
  "AAABEYTV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABICUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABLAQG" varchar(3) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYWL" varchar(8) NOT NULL default '00000000',
  "AAABEYWM" varchar(13) NOT NULL default ' ',
  "AAABGVMC" varchar(3) NOT NULL default ' ',
  "AAABGYJY" varchar(8) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABLAQG" varchar(3) NOT NULL default ' ',
  "AAABEYWI" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYWL","AAABEYWM","AAABGVMC","AAABGYJY","AAABEYXL","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYXL","AAABEYXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXL" varchar(2) NOT NULL default ' ',
  "AAABEYXM" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEYXL","AAABEYXM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEYXW" varchar(30) NOT NULL default ' ',
  "AAABEYXX" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(15) NOT NULL default ' ',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEYXW","AAABEYXX","AAABMMYV","AAABEZTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEYYF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMREX","AAABEZVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(70) NOT NULL default ' ',
  "AAABEYYQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABEYYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJASU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJASU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABHSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEYZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABEZAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABHSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABEZAJ" varchar(1) NOT NULL default ' ',
  "AAABEZAK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABHSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZBF" varchar(1) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABHSBA" varchar(1) NOT NULL default ' ',
  "AAABEZBQ" varchar(3) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABHSBA","AAABEZBQ","AAABICIU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABEZBX" varchar(1) NOT NULL default ' ',
  "AAABEZBY" varchar(1) NOT NULL default ' ',
  "AAABEZBZ" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZHM","AAABEZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABEZCK" varchar(1) NOT NULL default ' ',
  "AAABEZCL" varchar(1) NOT NULL default ' ',
  "AAABEZCM" varchar(1) NOT NULL default ' ',
  "AAABEZCN" varchar(1) NOT NULL default ' ',
  "AAABEZCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZHM","AAABEZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABEZCZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZHM","AAABEZHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZDL" varchar(1) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABMLNA" int(11) NOT NULL default '0',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABLAUL" varchar(1) NOT NULL default ' ',
  "AAABEZDQ" varchar(4) NOT NULL default ' ',
  "AAABEZDR" varchar(1) NOT NULL default ' ',
  "AAABEZDS" varchar(1) NOT NULL default ' ',
  "AAABLAUO" varchar(4) NOT NULL default ' ',
  "AAABLTXO" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABMQXO" varchar(1) NOT NULL default ' ',
  "AAABEZRD" varchar(3) NOT NULL default ' ',
  "AAABLAVS" varchar(30) NOT NULL default ' ',
  "AAABMKAH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZHL","AAABMREF","AAABEZHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  "AAABEZGI" varchar(40) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZHL","AAABMREF","AAABEZHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZGA" varchar(3) NOT NULL default '000',
  "AAABLMVO" varchar(30) NOT NULL default ' ',
  "AAABEZFE" varchar(12) NOT NULL default ' ',
  "AAABEZFF" varchar(12) NOT NULL default ' ',
  "AAABEZFG" int(11) NOT NULL default '0',
  "AAABEZFH" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAABJDQV" varchar(1) NOT NULL default ' ',
  "AAABJDQY" varchar(46) NOT NULL default ' ',
  "AAABJDQZ" varchar(46) NOT NULL default ' ',
  "AAABLTUD" int(11) NOT NULL default '0',
  "AAABEZFO" varchar(12) NOT NULL default ' ',
  "AAABEZFP" varchar(1) NOT NULL default ' ',
  "AAABJDQW" varchar(1) NOT NULL default ' ',
  "AAABEZFR" smallint(6) NOT NULL default '0',
  "AAABEZFS" varchar(1) NOT NULL default ' ',
  "AAABKGXT" varchar(30) NOT NULL default ' ',
  "AAABEZFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZHM","AAABEZHL","AAABMREF","AAABEZGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZGI" varchar(40) NOT NULL default ' ',
  "AAABEZGJ" varchar(40) NOT NULL default ' ',
  "AAABEZGK" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABEZHM","AAABEZHL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZHL" varchar(30) NOT NULL default ' ',
  "AAABEZHM" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFPQD" varchar(2) NOT NULL default '00',
  "AAABKRAD" varchar(1) NOT NULL default ' ',
  "AAABKRAE" varchar(1) NOT NULL default ' ',
  "AAABEZHA" varchar(2) NOT NULL default ' ',
  "AAABEZHB" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(1) NOT NULL default ' ',
  "AAABEZHD" varchar(1) NOT NULL default ' ',
  "AAABLAUT" varchar(40) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABEZHG" int(11) NOT NULL default '0',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABEZHI" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZHL","AAABEZHM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABEZLL" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZNS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZNS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZNS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABEZLL" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABEZNY" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZLZ" varchar(30) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  "AAABEZNA" varchar(1) NOT NULL default ' ',
  "AAABEZNB" varchar(1) NOT NULL default ' ',
  "AAABEZNC" varchar(1) NOT NULL default ' ',
  "AAABEZMF" varchar(1) NOT NULL default ' ',
  "AAABEZMG" varchar(30) NOT NULL default ' ',
  "AAABGMZS" varchar(3) NOT NULL default '000',
  "AAABEZND" varchar(3) NOT NULL default ' ',
  "AAABEZMJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABEZNY","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZMX" varchar(30) NOT NULL default ' ',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  "AAABEZNA" varchar(1) NOT NULL default ' ',
  "AAABEZNB" varchar(1) NOT NULL default ' ',
  "AAABEZNC" varchar(1) NOT NULL default ' ',
  "AAABEZND" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABEZNY" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZNS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE","AAABEZNY","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABMMVH","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZPO" varchar(4) NOT NULL default ' ',
  "AAABEZPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABMIRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEZRD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABLCPX","AAABEZRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEZRD" varchar(3) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHSAY","AAABLCPX","AAABEZRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZRI" varchar(1) NOT NULL default ' ',
  "AAABEZRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHSAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZRV" varchar(2) NOT NULL default '00',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABEZSF" varchar(2) NOT NULL default '00',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABEZSF","AAABHRWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABEZSN" varchar(30) NOT NULL default ' ',
  "AAABEZSO" varchar(30) NOT NULL default ' ',
  "AAABEZSP" varchar(30) NOT NULL default ' ',
  "AAABEZSQ" varchar(30) NOT NULL default ' ',
  "AAABEZSR" varchar(15) NOT NULL default ' ',
  "AAABEZSS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABMGHE","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  "AAABEZTG" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(4) NOT NULL default ' ',
  "AAABEZTD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZTU","AAABEZTG","AAABLTKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEZTU" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEZTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVG" varchar(2) NOT NULL default ' ',
  "AAABEZVH" varchar(2) NOT NULL default ' ',
  "AAABEZVI" varchar(6) NOT NULL default ' ',
  "AAABEZVJ" varchar(6) NOT NULL default ' ',
  "AAABEZXG" varchar(1) NOT NULL default ' ',
  "AAABEZVN" varchar(1) NOT NULL default ' ',
  "AAABEZVO" varchar(2) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVG","AAABEZVH","AAABEZVI","AAABEZVJ","AAABEZXG","AAABEZVN","AAABEZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZVG" varchar(2) NOT NULL default ' ',
  "AAABEZVH" varchar(2) NOT NULL default ' ',
  "AAABEZVI" varchar(6) NOT NULL default ' ',
  "AAABEZVJ" varchar(6) NOT NULL default ' ',
  "AAABEZVK" varchar(6) NOT NULL default ' ',
  "AAABEZVL" varchar(6) NOT NULL default ' ',
  "AAABEZXG" varchar(1) NOT NULL default ' ',
  "AAABEZVN" varchar(1) NOT NULL default ' ',
  "AAABEZVO" varchar(2) NOT NULL default ' ',
  "AAABEZVC" varchar(5) NOT NULL default ' ',
  "AAABEZVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZVG","AAABEZVH","AAABEZVI","AAABEZVJ","AAABEZVK","AAABEZVL","AAABEZXG","AAABEZVN","AAABEZVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEZWJ" varchar(4) NOT NULL default ' ',
  "AAABEZWK" varchar(4) NOT NULL default ' ',
  "AAABEZWL" varchar(4) NOT NULL default ' ',
  "AAABEZWM" varchar(4) NOT NULL default ' ',
  "AAABEZWN" varchar(2) NOT NULL default ' ',
  "AAABEZWO" varchar(5) NOT NULL default ' ',
  "AAABEZWP" varchar(5) NOT NULL default ' ',
  "AAABEZWQ" varchar(1) NOT NULL default ' ',
  "AAABFVMB" varchar(4) NOT NULL default ' ',
  "AAABEZWC" varchar(4) NOT NULL default ' ',
  "AAABEZWD" varchar(4) NOT NULL default ' ',
  "AAABEZWE" varchar(4) NOT NULL default ' ',
  "AAABEZWF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXF","AAABEZWJ","AAABEZWK","AAABEZWL","AAABEZWM","AAABEZWN","AAABEZWO","AAABEZWP","AAABEZWQ","AAABFVMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABEZXE" varchar(2) NOT NULL default ' ',
  "AAABEZXF" varchar(6) NOT NULL default ' ',
  "AAABEZXG" varchar(1) NOT NULL default ' ',
  "AAABEZXH" varchar(1) NOT NULL default ' ',
  "AAABEZXI" varchar(1) NOT NULL default ' ',
  "AAABEZXA" varchar(1) NOT NULL default ' ',
  "AAABEZXB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABEZXE","AAABEZXF","AAABEZXG","AAABEZXH","AAABEZXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSAY" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABEZXS" varchar(12) NOT NULL default ' ',
  "AAABEZXO" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSAY","AAABMMVH","AAABEZXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZXT" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABEZXZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABEZXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZYA" (
  "AAABHAYY" varchar(16) NOT NULL default ' ',
  "AAABHAYZ" varchar(16) NOT NULL default ' ',
  "AAABHAZA" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABHAYY","AAABHAYZ","AAABHAZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAZL" varchar(6) NOT NULL default '000000',
  "AAABKWJY" varchar(30) NOT NULL default ' ',
  "AAABLIMF" varchar(30) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  "AAABFNVZ" varchar(30) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABEZYP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHAZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMECK" (
  "AAABEZYX" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABEZYZ" varchar(20) NOT NULL default ' ',
  "AAABGZHX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZYX","AAABLESS","AAABEZYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFITF","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHYGA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFITF","AAABLUVS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEZZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABGNXB" varchar(4) NOT NULL default '0000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGPKT" varchar(3) NOT NULL default ' ',
  "AAABGPKU" varchar(3) NOT NULL default ' ',
  "AAABGPKV" varchar(3) NOT NULL default ' ',
  "AAABGPKW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFITF","AAABLUVS","AAABGNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAAF" (
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABFAAU" varchar(3) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABFAAK" varchar(1) NOT NULL default ' ',
  "AAABFAAL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFITF","AAABFAAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAAO" (
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABFAAU" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFITF","AAABFAAU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABGNXB" varchar(4) NOT NULL default '0000',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFITF","AAABLUVS","AAABGNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFABG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABGNXB" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFITF","AAABLUVS","AAABGNXB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFABS" (
  "AAABLSMW" varchar(3) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABFABW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSMW","AAABMPWP","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFACA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFACF" varchar(2) NOT NULL default ' ',
  "AAABFACD" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFACF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFACG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABFAHA" varchar(2) NOT NULL default '00',
  "AAABFACK" varchar(1) NOT NULL default ' ',
  "AAABIEWD" varchar(60) NOT NULL default ' ',
  "AAABFACM" varchar(60) NOT NULL default ' ',
  "AAABFACN" varchar(1) NOT NULL default ' ',
  "AAABKKSF" varchar(1) NOT NULL default '0',
  "AAABJWYW" varchar(2) NOT NULL default '00',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABKCES" varchar(4) NOT NULL default ' ',
  "AAABFFUS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFACV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFADA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABKHGE" varchar(3) NOT NULL default '000',
  "AAABFARI" varchar(2) NOT NULL default '00',
  "AAABFADF" decimal(7,2) NOT NULL default '0.00',
  "AAABFADG" decimal(7,2) NOT NULL default '0.00',
  "AAABFADH" decimal(7,2) NOT NULL default '0.00',
  "AAABFADI" varchar(1) NOT NULL default ' ',
  "AAABLCXU" varchar(4) NOT NULL default '0000',
  "AAABFADK" varchar(2) NOT NULL default ' ',
  "AAABFADL" varchar(1) NOT NULL default ' ',
  "AAABFADM" varchar(1) NOT NULL default ' ',
  "AAABFADN" varchar(1) NOT NULL default ' ',
  "AAABFADO" varchar(3) NOT NULL default ' ',
  "AAABFADP" varchar(2) NOT NULL default '00',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHNM","AAABKHGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFADU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFADZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAEE" varchar(4) NOT NULL default ' ',
  "AAABFAEC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFAEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFAEJ" varchar(1) NOT NULL default ' ',
  "AAABFAEK" varchar(1) NOT NULL default ' ',
  "AAABFAEL" varchar(1) NOT NULL default ' ',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  "AAABFAEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  "AAABFAEU" varchar(1) NOT NULL default ' ',
  "AAABFAEV" varchar(1) NOT NULL default ' ',
  "AAABFAEW" varchar(1) NOT NULL default ' ',
  "AAABFAEX" varchar(1) NOT NULL default ' ',
  "AAABFAEY" varchar(1) NOT NULL default ' ',
  "AAABFAEZ" varchar(1) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABFAFB" varchar(1) NOT NULL default ' ',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  "AAABFAFD" varchar(1) NOT NULL default ' ',
  "AAABJVYX" varchar(1) NOT NULL default ' ',
  "AAABJWKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  "AAABFAFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCXU" varchar(4) NOT NULL default '0000',
  "AAABFAFR" varchar(1) NOT NULL default ' ',
  "AAABFARM" varchar(1) NOT NULL default ' ',
  "AAABFAFT" varchar(1) NOT NULL default ' ',
  "AAABFARN" varchar(1) NOT NULL default ' ',
  "AAABFAFV" varchar(4) NOT NULL default '0000',
  "AAABFAFW" varchar(1) NOT NULL default ' ',
  "AAABFAFX" varchar(1) NOT NULL default ' ',
  "AAABFAFY" varchar(1) NOT NULL default ' ',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLCXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAGC" (
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXG","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABKDGK" varchar(1) NOT NULL default ' ',
  "AAABFAGL" varchar(1) NOT NULL default ' ',
  "AAABFAGM" varchar(4) NOT NULL default '0000',
  "AAABFAGN" varchar(1) NOT NULL default ' ',
  "AAABFAGO" varchar(1) NOT NULL default ' ',
  "AAABFAGP" varchar(1) NOT NULL default ' ',
  "AAABFAGQ" varchar(1) NOT NULL default ' ',
  "AAABFAGR" varchar(1) NOT NULL default ' ',
  "AAABFAGS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAHA" varchar(2) NOT NULL default '00',
  "AAABFAJD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFAHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHNH" varchar(1) NOT NULL default ' ',
  "AAABFAHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNH" varchar(1) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABFAJD" varchar(1) NOT NULL default '0',
  "AAABFFDF" varchar(1) NOT NULL default '0',
  "AAABLBFY" varchar(2) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABFAHZ" varchar(1) NOT NULL default ' ',
  "AAABFAIA" varchar(1) NOT NULL default ' ',
  "AAABFAIB" varchar(1) NOT NULL default ' ',
  "AAABFFDM" varchar(2) NOT NULL default '00',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABFAIE" varchar(1) NOT NULL default ' ',
  "AAABFAIF" varchar(1) NOT NULL default ' ',
  "AAABFAIG" varchar(2) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABFAIM" varchar(1) NOT NULL default '0',
  "AAABFAIN" varchar(4) NOT NULL default ' ',
  "AAABFAIO" varchar(1) NOT NULL default ' ',
  "AAABFAIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHNS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAJD" varchar(1) NOT NULL default '0',
  "AAABKHNS" varchar(1) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFAJD","AAABKHNS","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABKHMX" varchar(2) NOT NULL default '00',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNM","AAABKHMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABFARI" varchar(2) NOT NULL default '00',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNM","AAABFARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNS" varchar(1) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFALA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHLZ" varchar(3) NOT NULL default '000',
  "AAABFALD" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABKHLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFALG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHLZ" varchar(3) NOT NULL default '000',
  "AAABFALK" varchar(4) NOT NULL default ' ',
  "AAABFALL" varchar(30) NOT NULL default ' ',
  "AAABFALM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFALQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  "AAABFALT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFALW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDZR" varchar(4) NOT NULL default '0000',
  "AAABFALZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKHKO" varchar(1) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKHKO","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAHK" varchar(1) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAABFAMQ" varchar(2) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABFAMS" varchar(2) NOT NULL default ' ',
  "AAABFAMT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAHK","AAABFPZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKHOF","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFANE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKDZR" varchar(4) NOT NULL default '0000',
  "AAABFANI" varchar(30) NOT NULL default ' ',
  "AAABFANJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKDZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFANN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABFANS" varchar(7) NOT NULL default '0000000',
  "AAABJVSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFANW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABFAOL" varchar(2) NOT NULL default '00',
  "AAABFAOA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIDK","AAABFAOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABFAOL" varchar(2) NOT NULL default '00',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIDK","AAABFAOL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABFAPK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKIDK","AAABFAPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWMW" varchar(2) NOT NULL default '00',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFAOY" varchar(1) NOT NULL default ' ',
  "AAABKIKO" decimal(15,2) NOT NULL default '0.00',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABFAPK" varchar(2) NOT NULL default '00',
  "AAABJWMW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKIDK","AAABFAPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCGH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEHC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKEHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKFFD" varchar(10) NOT NULL default ' ',
  "AAABFAQQ" decimal(5,2) NOT NULL default '0.00',
  "AAABKELF" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKFFD","AAABFAQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEHC" varchar(3) NOT NULL default '000',
  "AAABFAQV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKEHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABFARI" varchar(2) NOT NULL default '00',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKHNM","AAABFARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFARJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFWR" varchar(3) NOT NULL default '000',
  "AAABFARM" varchar(1) NOT NULL default ' ',
  "AAABFARN" varchar(1) NOT NULL default ' ',
  "AAABFARO" varchar(1) NOT NULL default ' ',
  "AAABFARP" varchar(1) NOT NULL default ' ',
  "AAABFARQ" varchar(1) NOT NULL default ' ',
  "AAABFARR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFARU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFD" varchar(10) NOT NULL default ' ',
  "AAABFBDR" varchar(8) NOT NULL default '00000000',
  "AAABFBDN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKFFD","AAABFBDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFASC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABLCXU" varchar(4) NOT NULL default '0000',
  "AAABLADC" varchar(1) NOT NULL default ' ',
  "AAABFASH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHFZ","AAABLCXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFASL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFASU" varchar(1) NOT NULL default ' ',
  "AAABFASV" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  "AAABFASR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFASU","AAABFASV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFASW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAVK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFAVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFATB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFATI" varchar(10) NOT NULL default ' ',
  "AAABFATF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABFATI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFATJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABFATN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABLAXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFATR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKKPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFATZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAZW" varchar(2) NOT NULL default '00',
  "AAABKKLF" varchar(3) NOT NULL default '000',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABKKLE" varchar(3) NOT NULL default '000',
  "AAABFAUF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFAZW","AAABKKLF","AAABKKPV","AAABKKLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABKKLE" varchar(3) NOT NULL default '000',
  "AAABKKLF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKKPV","AAABKKLE","AAABKKLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFAVC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDCD","AAABLZBM","AAABFAVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFAVK" varchar(2) NOT NULL default '00',
  "AAABFAVH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFAVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKOL" varchar(3) NOT NULL default ' ',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABFAVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKOL","AAABKKPV","AAABFAVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKIFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAWA" (
  "AAABFAYZ" varchar(2) NOT NULL default '00',
  "AAABKKNE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFAYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABFAWH" varchar(2) NOT NULL default '00',
  "AAABFAYZ" varchar(2) NOT NULL default '00',
  "AAABFAWJ" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMDZR" varchar(3) NOT NULL default '000',
  "AAABLCZX" varchar(1) NOT NULL default ' ',
  "AAABLCVK" varchar(1) NOT NULL default '0',
  "AAABKKLQ" varchar(4) NOT NULL default ' ',
  "AAABFAWP" varchar(1) NOT NULL default ' ',
  "AAABKKNI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAWT" (
  "AAABFAZO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABFAZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFAZW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFAZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKLF" varchar(3) NOT NULL default '000',
  "AAABFAXE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKLE" varchar(3) NOT NULL default '000',
  "AAABFAZO" varchar(1) NOT NULL default '0',
  "AAABFAZW" varchar(2) NOT NULL default '00',
  "AAABFAXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKOL" varchar(3) NOT NULL default ' ',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABKKXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(7) NOT NULL default ' ',
  "AAABFAXZ" varchar(8) NOT NULL default ' ',
  "AAABFAYA" varchar(8) NOT NULL default ' ',
  "AAABFAYB" varchar(8) NOT NULL default ' ',
  "AAABFAYC" varchar(8) NOT NULL default ' ',
  "AAABFAYD" varchar(15) NOT NULL default ' ',
  "AAABFAYE" varchar(15) NOT NULL default ' ',
  "AAABFAYF" varchar(15) NOT NULL default ' ',
  "AAABHODG" varchar(15) NOT NULL default ' ',
  "AAABFAYH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKKPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAYT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFAYZ" varchar(2) NOT NULL default '00',
  "AAABFAYW" varchar(15) NOT NULL default ' ',
  "AAABFAYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFAYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKPV" varchar(2) NOT NULL default '00',
  "AAABFAZE" varchar(15) NOT NULL default ' ',
  "AAABFAZF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAZJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFAZO" varchar(1) NOT NULL default '0',
  "AAABFAZM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFAZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFAZW" varchar(2) NOT NULL default '00',
  "AAABFAZT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFAZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFAZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKLF" varchar(3) NOT NULL default '000',
  "AAABFBAB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKLE" varchar(3) NOT NULL default '000',
  "AAABFBAJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKOL" varchar(3) NOT NULL default ' ',
  "AAABKKON" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBBR" varchar(9) NOT NULL default '000000000',
  "AAABFBBS" varchar(15) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFBBI" varchar(10) NOT NULL default ' ',
  "AAABFBBJ" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABFBBL" varchar(2) NOT NULL default '00',
  "AAABFBBM" varchar(2) NOT NULL default '00',
  "AAABFBBN" varchar(1) NOT NULL default ' ',
  "AAABHYAO" varchar(2) NOT NULL default '00',
  "AAABFBBP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFBBR","AAABFBBS","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBCH" varchar(4) NOT NULL default ' ',
  "AAABKKOQ" varchar(4) NOT NULL default ' ',
  "AAABKKJH" varchar(4) NOT NULL default ' ',
  "AAABKKJG" varchar(1) NOT NULL default ' ',
  "AAABFBCA" varchar(10) NOT NULL default ' ',
  "AAABFBCB" varchar(10) NOT NULL default ' ',
  "AAABFBCC" varchar(10) NOT NULL default ' ',
  "AAABFBCD" varchar(10) NOT NULL default ' ',
  "AAABFBCE" varchar(10) NOT NULL default ' ',
  "AAABFBCF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFBCH","AAABKKOQ","AAABKKJH","AAABKKJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFBCO" varchar(10) NOT NULL default ' ',
  "AAABFBCP" varchar(10) NOT NULL default ' ',
  "AAABFBCQ" varchar(2) NOT NULL default ' ',
  "AAABFBCR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWHS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABFBDC" varchar(1) NOT NULL default ' ',
  "AAABFBDD" varchar(1) NOT NULL default ' ',
  "AAABFBDE" varchar(1) NOT NULL default ' ',
  "AAABFBDF" varchar(1) NOT NULL default ' ',
  "AAABFBDG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWHS" varchar(10) NOT NULL default ' ',
  "AAABFBDR" varchar(8) NOT NULL default '00000000',
  "AAABFBDN" varchar(8) NOT NULL default '00000000',
  "AAABFBDO" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJWHS","AAABFBDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBDS" (
  "AAABKBXS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKBXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABMEFI" varchar(5) NOT NULL default ' ',
  "AAABFBDZ" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABMEFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBED" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLCPX","AAABFFKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABFBFH" varchar(3) NOT NULL default ' ',
  "AAABFBEN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABFFKZ","AAABFBFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABFBEW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABFFKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBFA" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABFBFH" varchar(3) NOT NULL default ' ',
  "AAABFBFE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABFFKZ","AAABFBFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABFBFR" varchar(4) NOT NULL default '0000',
  "AAABFCLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABFFKZ","AAABFBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAUE" varchar(2) NOT NULL default '00',
  "AAABKARJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAUE","AAABKARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJWHS" varchar(10) NOT NULL default ' ',
  "AAABFBGD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBGH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKBXS" varchar(2) NOT NULL default ' ',
  "AAABFBGK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKBXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBGN" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABFFKZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBHK" varchar(2) NOT NULL default ' ',
  "AAABKKSP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFBHK","AAABKKSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFBHK" varchar(2) NOT NULL default ' ',
  "AAABKKSP" varchar(40) NOT NULL default ' ',
  "AAABFBHH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFBHK","AAABKKSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAUE" varchar(2) NOT NULL default '00',
  "AAABKARJ" varchar(1) NOT NULL default ' ',
  "AAABFBHR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKAUE","AAABKARJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBPF" varchar(6) NOT NULL default ' ',
  "AAABFBIO" varchar(4) NOT NULL default '0000',
  "AAABFBIP" varchar(3) NOT NULL default '000',
  "AAABJVGE" varchar(30) NOT NULL default ' ',
  "AAABFBIC" varchar(80) NOT NULL default ' ',
  "AAABFBID" varchar(1) NOT NULL default ' ',
  "AAABFBIE" varchar(1) NOT NULL default ' ',
  "AAABFBIF" varchar(1) NOT NULL default ' ',
  "AAABJVGF" varchar(1) NOT NULL default ' ',
  "AAABFBIH" varchar(30) NOT NULL default ' ',
  "AAABFBII" varchar(50) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFBPF","AAABFBIO","AAABFBIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBIQ" (
  "AAABJVGD" varchar(30) NOT NULL default ' ',
  "AAABFBIS" varchar(1) NOT NULL default ' ',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABFBIU" varchar(1) NOT NULL default ' ',
  "AAABFBIV" varchar(1) NOT NULL default ' ',
  "AAABJVGF" varchar(1) NOT NULL default ' ',
  "AAABFBIX" tinyint(3) unsigned NOT NULL default '0',
  "AAABFBIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJVGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBJA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJVGD" varchar(30) NOT NULL default ' ',
  "AAABFBJD" varchar(60) NOT NULL default ' ',
  "AAABJVGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJVGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABFBVG" varchar(6) NOT NULL default ' ',
  "AAABFBJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABFBVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABFBVG" varchar(6) NOT NULL default ' ',
  "AAABFBJU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER","AAABFBVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBPF" varchar(6) NOT NULL default ' ',
  "AAABFBKC" varchar(1) NOT NULL default ' ',
  "AAABFBKD" varchar(1) NOT NULL default ' ',
  "AAABFBKE" varchar(1) NOT NULL default ' ',
  "AAABFBKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFBPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFBPF" varchar(6) NOT NULL default ' ',
  "AAABFBKM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFBPF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBLK" varchar(5) NOT NULL default ' ',
  "AAABFBLL" varchar(4) NOT NULL default '0000',
  "AAABFBLF" varchar(5) NOT NULL default ' ',
  "AAABFBLG" varchar(4) NOT NULL default '0000',
  "AAABFBLH" varchar(8) NOT NULL default '00000000',
  "AAABFBLI" decimal(8,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABFBLK","AAABFBLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBLK" varchar(5) NOT NULL default ' ',
  "AAABFBLL" varchar(4) NOT NULL default '0000',
  "AAABFBLF" varchar(5) NOT NULL default ' ',
  "AAABFBLG" varchar(4) NOT NULL default '0000',
  "AAABFBLH" varchar(8) NOT NULL default '00000000',
  "AAABFBLI" decimal(8,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABFBLK","AAABFBLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVJG" varchar(5) NOT NULL default ' ',
  "AAABFBLP" varchar(1) NOT NULL default '0',
  "AAABFBLQ" smallint(6) NOT NULL default '0',
  "AAABFBLR" varchar(4) NOT NULL default '0000',
  "AAABFBLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABFBOJ" varchar(3) NOT NULL default '000',
  "AAABFBNV" decimal(7,2) NOT NULL default '0.00',
  "AAABFBNW" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBNX" decimal(15,2) NOT NULL default '0.00',
  "AAABFBNY" decimal(7,2) NOT NULL default '0.00',
  "AAABFBNZ" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBOA" decimal(15,2) NOT NULL default '0.00',
  "AAABFBOB" varchar(4) NOT NULL default ' ',
  "AAABFBOC" varchar(1) NOT NULL default ' ',
  "AAABFBOD" varchar(5) NOT NULL default ' ',
  "AAABFBOE" varchar(3) NOT NULL default ' ',
  "AAABFBOF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJVLB","AAABFBOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBMN" (
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABFBMP" varchar(30) NOT NULL default ' ',
  "AAABFBMQ" varchar(30) NOT NULL default ' ',
  "AAABFBMR" varchar(30) NOT NULL default ' ',
  "AAABFBMS" varchar(30) NOT NULL default ' ',
  "AAABFBMT" varchar(30) NOT NULL default ' ',
  "AAABFBMU" varchar(30) NOT NULL default ' ',
  "AAABFBMV" varchar(30) NOT NULL default ' ',
  "AAABFBMW" varchar(1) NOT NULL default ' ',
  "AAABFBMX" varchar(1) NOT NULL default ' ',
  "AAABFBMY" varchar(1) NOT NULL default ' ',
  "AAABFBMZ" varchar(30) NOT NULL default ' ',
  "AAABFBNA" varchar(3) NOT NULL default '000',
  "AAABFBNB" varchar(1) NOT NULL default ' ',
  "AAABFBNC" varchar(30) NOT NULL default ' ',
  "AAABFBND" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJVHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBNF" (
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABFBNJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJVHC","AAABFBNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBNK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABFBNN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJVHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVBB" varchar(10) NOT NULL default ' ',
  "AAABFBOJ" varchar(3) NOT NULL default '000',
  "AAABFBNV" decimal(7,2) NOT NULL default '0.00',
  "AAABFBNW" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBNX" decimal(15,2) NOT NULL default '0.00',
  "AAABFBNY" decimal(7,2) NOT NULL default '0.00',
  "AAABFBNZ" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBOA" decimal(15,2) NOT NULL default '0.00',
  "AAABFBOB" varchar(4) NOT NULL default ' ',
  "AAABFBOC" varchar(1) NOT NULL default ' ',
  "AAABFBOD" varchar(5) NOT NULL default ' ',
  "AAABFBOE" varchar(3) NOT NULL default ' ',
  "AAABFBOF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJVLB","AAABJVBB","AAABFBOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABJVBB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVHC","AAABJVBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABJVBB" varchar(10) NOT NULL default ' ',
  "AAABFBOW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJVHC","AAABJVBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABFBPF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVLB","AAABJYER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABFBPM" varchar(8) NOT NULL default '00000000',
  "AAABFBPN" varchar(8) NOT NULL default '00000000',
  "AAABFBPO" varchar(1) NOT NULL default ' ',
  "AAABFBPP" varchar(1) NOT NULL default ' ',
  "AAABFBPQ" varchar(1) NOT NULL default ' ',
  "AAABFBPR" varchar(1) NOT NULL default ' ',
  "AAABFBPS" varchar(1) NOT NULL default ' ',
  "AAABJVDJ" varchar(5) NOT NULL default ' ',
  "AAABFBPU" varchar(1) NOT NULL default ' ',
  "AAABFBPV" varchar(1) NOT NULL default ' ',
  "AAABFBPW" varchar(6) NOT NULL default ' ',
  "AAABFBPX" varchar(4) NOT NULL default ' ',
  "AAABJVDE" varchar(3) NOT NULL default ' ',
  "AAABFBPZ" varchar(1) NOT NULL default ' ',
  "AAABFBQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVCW" varchar(6) NOT NULL default ' ',
  "AAABJVCX" varchar(8) NOT NULL default '00000000',
  "AAABJVCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVCW" varchar(6) NOT NULL default ' ',
  "AAABJYEC" varchar(6) NOT NULL default ' ',
  "AAABJVCZ" varchar(6) NOT NULL default ' ',
  "AAABJYEB" varchar(6) NOT NULL default ' ',
  "AAABJVDF" decimal(19,6) NOT NULL default '0.000000',
  "AAABJVDG" decimal(19,6) NOT NULL default '0.000000',
  "AAABJVDH" decimal(19,6) NOT NULL default '0.000000',
  "AAABJVDL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVCW","AAABJYEC","AAABJVCZ","AAABJYEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVEG" varchar(6) NOT NULL default ' ',
  "AAABJVEV" varchar(4) NOT NULL default ' ',
  "AAABFBRI" decimal(5,2) NOT NULL default '0.00',
  "AAABJVEW" varchar(3) NOT NULL default ' ',
  "AAABJVEX" decimal(5,2) NOT NULL default '0.00',
  "AAABJVFJ" varchar(4) NOT NULL default ' ',
  "AAABFBRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVEG" varchar(6) NOT NULL default ' ',
  "AAABFBRV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER","AAABJVEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJYEB" varchar(6) NOT NULL default ' ',
  "AAABFBUR" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJYEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJYEB" varchar(6) NOT NULL default ' ',
  "AAABFBSN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER","AAABJYEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVDL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVDL" varchar(6) NOT NULL default ' ',
  "AAABFBTE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER","AAABJVDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJYEC" varchar(6) NOT NULL default ' ',
  "AAABFBTN" decimal(15,2) NOT NULL default '0.00',
  "AAABFBTO" decimal(15,2) NOT NULL default '0.00',
  "AAABFBWE" decimal(19,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJYEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJYEC" varchar(6) NOT NULL default ' ',
  "AAABFBTY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER","AAABJYEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFBUJ" varchar(6) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABFBUH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFBUJ","AAABLRLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVCZ" varchar(6) NOT NULL default ' ',
  "AAABJVDA" decimal(19,6) NOT NULL default '0.000000',
  "AAABJVDB" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBUR" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJXSA" varchar(6) NOT NULL default ' ',
  "AAABJVEG" varchar(6) NOT NULL default ' ',
  "AAABJVEY" decimal(7,2) NOT NULL default '0.00',
  "AAABJVEZ" decimal(7,2) NOT NULL default '0.00',
  "AAABJVFA" decimal(7,2) NOT NULL default '0.00',
  "AAABJVFB" decimal(7,2) NOT NULL default '0.00',
  "AAABFBVE" varchar(1) NOT NULL default ' ',
  "AAABFBVF" varchar(1) NOT NULL default ' ',
  "AAABFBVG" varchar(6) NOT NULL default ' ',
  "AAABFBVH" varchar(1) NOT NULL default ' ',
  "AAABJVES" varchar(1) NOT NULL default ' ',
  "AAABFBVJ" varchar(1) NOT NULL default ' ',
  "AAABFBVK" varchar(30) NOT NULL default ' ',
  "AAABFBVL" varchar(1) NOT NULL default ' ',
  "AAABFBVM" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBVN" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBVO" varchar(1) NOT NULL default ' ',
  "AAABFBVP" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJXSA","AAABJVEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABJVEG" varchar(6) NOT NULL default ' ',
  "AAABFBWP" varchar(2) NOT NULL default '00',
  "AAABFBVZ" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBWA" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBWB" tinyint(3) unsigned NOT NULL default '0',
  "AAABFBWC" decimal(17,4) NOT NULL default '0.0000',
  "AAABFBWD" decimal(17,4) NOT NULL default '0.0000',
  "AAABFBWE" decimal(19,6) NOT NULL default '0.000000',
  "AAABFBWF" decimal(5,2) NOT NULL default '0.00',
  "AAABJVEO" decimal(5,2) NOT NULL default '0.00',
  "AAABJVFG" decimal(15,2) NOT NULL default '0.00',
  "AAABJXSA" varchar(6) NOT NULL default ' ',
  "AAABFBWJ" varchar(1) NOT NULL default ' ',
  "AAABFBWK" varchar(1) NOT NULL default ' ',
  "AAABFBWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYER","AAABJVEG","AAABFBWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYER" varchar(6) NOT NULL default ' ',
  "AAABFBWU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  "AAABJVHD" varchar(1) NOT NULL default ' ',
  "AAABJVHE" varchar(1) NOT NULL default ' ',
  "AAABJVHF" varchar(1) NOT NULL default ' ',
  "AAABJVHG" varchar(1) NOT NULL default ' ',
  "AAABJVHH" varchar(1) NOT NULL default ' ',
  "AAABJVHI" decimal(5,2) NOT NULL default '0.00',
  "AAABJVHJ" varchar(1) NOT NULL default ' ',
  "AAABJVHK" varchar(1) NOT NULL default ' ',
  "AAABJVHL" varchar(1) NOT NULL default ' ',
  "AAABJVHM" varchar(3) NOT NULL default '000',
  "AAABJVHN" varchar(1) NOT NULL default '0',
  "AAABKKBO" varchar(1) NOT NULL default ' ',
  "AAABJVHP" varchar(1) NOT NULL default ' ',
  "AAABJVHQ" varchar(1) NOT NULL default ' ',
  "AAABJVJF" varchar(1) NOT NULL default '0',
  "AAABJVHS" varchar(1) NOT NULL default '0',
  "AAABJVHT" varchar(1) NOT NULL default '0',
  "AAABJVHU" varchar(1) NOT NULL default '0',
  "AAABJVHV" varchar(1) NOT NULL default '0',
  "AAABJVHW" varchar(1) NOT NULL default '0',
  "AAABJVHX" varchar(1) NOT NULL default '0',
  "AAABJVHY" varchar(1) NOT NULL default '0',
  "AAABJVHZ" varchar(1) NOT NULL default '0',
  "AAABJVIA" varchar(1) NOT NULL default '0',
  "AAABJVIB" varchar(1) NOT NULL default '0',
  "AAABJVIC" varchar(1) NOT NULL default '0',
  "AAABJVID" varchar(1) NOT NULL default '0',
  "AAABJVKG" varchar(1) NOT NULL default '0',
  "AAABJVIF" varchar(1) NOT NULL default ' ',
  "AAABJVIG" varchar(1) NOT NULL default '0',
  "AAABJVIH" varchar(1) NOT NULL default ' ',
  "AAABJVII" varchar(1) NOT NULL default '0',
  "AAABJVIJ" varchar(1) NOT NULL default '0',
  "AAABJVIK" varchar(1) NOT NULL default '0',
  "AAABJVIL" varchar(1) NOT NULL default ' ',
  "AAABJVIM" varchar(1) NOT NULL default '0',
  "AAABJVIN" varchar(1) NOT NULL default ' ',
  "AAABJVIO" varchar(1) NOT NULL default ' ',
  "AAABJVIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABFBYX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVLB","AAABFBYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVBB" varchar(10) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABFBZD" varchar(1) NOT NULL default '0',
  "AAABFBZE" varchar(1) NOT NULL default ' ',
  "AAABFBZF" varchar(3) NOT NULL default '000',
  "AAABFBZG" varchar(1) NOT NULL default ' ',
  "AAABFBZH" varchar(3) NOT NULL default '000',
  "AAABJZMO" varchar(10) NOT NULL default ' ',
  "AAABFEPA" varchar(1) NOT NULL default '0',
  "AAABFEPB" smallint(6) NOT NULL default '0',
  "AAABFEPC" varchar(4) NOT NULL default '0000',
  "AAABFBZM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVLB","AAABJVBB","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFBZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABFBZV" varchar(60) NOT NULL default ' ',
  "AAABFBZW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVJF" varchar(1) NOT NULL default '0',
  "AAABJVKG" varchar(1) NOT NULL default '0',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJYSZ" varchar(13) NOT NULL default ' ',
  "AAABJUVV" tinyint(3) unsigned NOT NULL default '0',
  "AAABJUVW" varchar(1) NOT NULL default ' ',
  "AAABJUVX" varchar(4) NOT NULL default ' ',
  "AAABJUVY" varchar(3) NOT NULL default ' ',
  "AAABJUVZ" varchar(4) NOT NULL default ' ',
  "AAABJUWA" varchar(1) NOT NULL default ' ',
  "AAABKVVC" varchar(1) NOT NULL default '0',
  "AAABFCAO" varchar(1) NOT NULL default '0',
  "AAABFCAP" varchar(1) NOT NULL default '0',
  "AAABFCAQ" varchar(1) NOT NULL default '0',
  "AAABFCAR" varchar(1) NOT NULL default '0',
  "AAABFCAS" varchar(1) NOT NULL default '0',
  "AAABFCAT" varchar(1) NOT NULL default '0',
  "AAABFCAU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVJF" varchar(1) NOT NULL default '0',
  "AAABJVJG" varchar(5) NOT NULL default ' ',
  "AAABJVJH" varchar(4) NOT NULL default '0000',
  "AAABJVJI" varchar(1) NOT NULL default ' ',
  "AAABJVJJ" decimal(8,3) NOT NULL default '0.000',
  "AAABJVJK" decimal(7,4) NOT NULL default '0.0000',
  "AAABJVJL" varchar(1) NOT NULL default ' ',
  "AAABJVJM" varchar(1) NOT NULL default ' ',
  "AAABJVJN" varchar(1) NOT NULL default ' ',
  "AAABJVJO" decimal(3,0) NOT NULL default '0',
  "AAABJVJP" varchar(1) NOT NULL default '0',
  "AAABJVJQ" varchar(1) NOT NULL default '0',
  "AAABJVJR" varchar(1) NOT NULL default '0',
  "AAABJVJS" varchar(1) NOT NULL default '0',
  "AAABJVJT" varchar(1) NOT NULL default '0',
  "AAABJVJU" varchar(1) NOT NULL default '0',
  "AAABJVJV" varchar(1) NOT NULL default '0',
  "AAABJVJW" varchar(1) NOT NULL default '0',
  "AAABJVJX" varchar(1) NOT NULL default '0',
  "AAABJVJY" varchar(1) NOT NULL default '0',
  "AAABJVJZ" varchar(1) NOT NULL default '0',
  "AAABJVKA" varchar(1) NOT NULL default '0',
  "AAABJVKB" varchar(1) NOT NULL default '0',
  "AAABJVKC" varchar(1) NOT NULL default '0',
  "AAABJVKD" varchar(1) NOT NULL default '0',
  "AAABJVKE" varchar(1) NOT NULL default '0',
  "AAABJVKF" varchar(1) NOT NULL default '0',
  "AAABJVKG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABJVJF" varchar(1) NOT NULL default '0',
  "AAABJVKG" varchar(1) NOT NULL default '0',
  "AAABFCCJ" varchar(1) NOT NULL default ' ',
  "AAABFCCK" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWYW" varchar(2) NOT NULL default '00',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJWYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWYW" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCCX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWYW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABFCDE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCDL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXCT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABFMOY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXCT","AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABFMOY" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCEB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXCT","AAABFMOY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWXY" varchar(2) NOT NULL default '00',
  "AAABJXAL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJWXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWXY" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCEQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWXT" varchar(1) NOT NULL default '0',
  "AAABJXAF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJWXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJWXT" varchar(1) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCFE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJWXT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJXAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAG" varchar(1) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCFR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXAG","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHGE" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABFMOY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHGE","AAABJXCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXDQ" varchar(3) NOT NULL default ' ',
  "AAABFCGG" varchar(1) NOT NULL default ' ',
  "AAABJXED" varchar(5) NOT NULL default ' ',
  "AAABKGBM" varchar(3) NOT NULL default ' ',
  "AAABJXDR" varchar(3) NOT NULL default ' ',
  "AAABFCGK" varchar(8) NOT NULL default '00000000',
  "AAABFCGL" varchar(2) NOT NULL default '00',
  "AAABFCGM" varchar(2) NOT NULL default '00',
  "AAABFCGN" varchar(2) NOT NULL default '00',
  "AAABFCGO" varchar(2) NOT NULL default '00',
  "AAABFCGP" decimal(3,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABJXDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCIF" varchar(20) NOT NULL default ' ',
  "AAABJDQB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCIF","AAABJDQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCKZ" varchar(2) NOT NULL default '00',
  "AAABFCHC" varchar(4) NOT NULL default '0000',
  "AAABFCHD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFCKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRHT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQEQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCIF" varchar(20) NOT NULL default ' ',
  "AAABHWLM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCIF" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCID" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCIF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJXAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAL" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCIQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXAL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXDJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXDJ" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCJD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXDJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJXAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXAF" varchar(1) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCJQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXAF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCJU" (
  "AAABFMOY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCJX" (
  "AAABFCKT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFCKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCKA" (
  "AAABFCKT" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFCKD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFCKT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCKG" (
  "AAABFCKT" varchar(4) NOT NULL default ' ',
  "AAABFCKP" varchar(4) NOT NULL default '0000',
  "AAABFCKQ" decimal(6,4) NOT NULL default '0.0000',
  "AAABFCKR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABFCKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCKT" varchar(4) NOT NULL default ' ',
  "AAABFCKP" varchar(4) NOT NULL default '0000',
  "AAABFCKQ" decimal(6,4) NOT NULL default '0.0000',
  "AAABFCKR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABFCKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCKZ" varchar(2) NOT NULL default '00',
  "AAABFCLA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABFCKZ","AAABFCLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXCT" varchar(2) NOT NULL default '00',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCNR" varchar(3) NOT NULL default ' ',
  "AAABFCLQ" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFCNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABIFNA" varchar(4) NOT NULL default '0000',
  "AAABFCLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCPX","AAABFFKZ","AAABIFNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFHL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCOI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCMT" varchar(1) NOT NULL default ' ',
  "AAABFGXT" varchar(3) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCMT","AAABFGXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCNR" varchar(3) NOT NULL default ' ',
  "AAABFCNO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFHL" varchar(4) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFCOI" varchar(4) NOT NULL default ' ',
  "AAABFCOF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFCOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCOM" tinyint(3) unsigned NOT NULL default '0',
  "AAABFCON" tinyint(3) unsigned NOT NULL default '0',
  "AAABFCOO" varchar(1) NOT NULL default ' ',
  "AAABJXHW" varchar(10) NOT NULL default ' ',
  "AAABFCOQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFDAI" varchar(1) NOT NULL default ' ',
  "AAABFDAC" varchar(250) NOT NULL default ' ',
  "AAABFDAD" varchar(250) NOT NULL default ' ',
  "AAABFDAE" varchar(250) NOT NULL default ' ',
  "AAABFDAF" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ","AAABFDAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABJXHZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ","AAABJXHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABJXHZ" varchar(4) NOT NULL default ' ',
  "AAABFCPQ" varchar(30) NOT NULL default ' ',
  "AAABFCPR" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDUQ","AAABJXHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABLTQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ","AAABLTQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCQG" varchar(1) NOT NULL default ' ',
  "AAABFCQH" varchar(1) NOT NULL default ' ',
  "AAABFCQI" varchar(1) NOT NULL default ' ',
  "AAABFCQJ" varchar(1) NOT NULL default ' ',
  "AAABFCQK" varchar(1) NOT NULL default ' ',
  "AAABJXMA" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCQT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXSA" varchar(6) NOT NULL default ' ',
  "AAABJVFI" varchar(1) NOT NULL default ' ',
  "AAABJVFH" varchar(1) NOT NULL default ' ',
  "AAABFCRC" varchar(1) NOT NULL default ' ',
  "AAABFCRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXSA" varchar(6) NOT NULL default ' ',
  "AAABFCRK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXSG" varchar(4) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXSG" varchar(4) NOT NULL default ' ',
  "AAABFCRY" varchar(30) NOT NULL default ' ',
  "AAABFCRZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJXSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABFCSG" varchar(1) NOT NULL default ' ',
  "AAABKHXY" varchar(1) NOT NULL default ' ',
  "AAABFCSI" varchar(1) NOT NULL default ' ',
  "AAABFCSJ" varchar(1) NOT NULL default ' ',
  "AAABFCSK" varchar(1) NOT NULL default ' ',
  "AAABFCSL" varchar(1) NOT NULL default ' ',
  "AAABFCSM" varchar(1) NOT NULL default ' ',
  "AAABFCSN" varchar(1) NOT NULL default ' ',
  "AAABFCSO" varchar(3) NOT NULL default ' ',
  "AAABFCTG" varchar(1) NOT NULL default ' ',
  "AAABFCVG" varchar(1) NOT NULL default ' ',
  "AAABFCSR" varchar(1) NOT NULL default ' ',
  "AAABFCSS" varchar(1) NOT NULL default ' ',
  "AAABFCST" varchar(1) NOT NULL default ' ',
  "AAABFCSU" varchar(1) NOT NULL default ' ',
  "AAABFCSV" varchar(1) NOT NULL default ' ',
  "AAABFCSW" varchar(1) NOT NULL default ' ',
  "AAABFCSX" varchar(1) NOT NULL default ' ',
  "AAABFCSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCUX" varchar(1) NOT NULL default ' ',
  "AAABFCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCTY" varchar(4) NOT NULL default ' ',
  "AAABKCPM" varchar(2) NOT NULL default ' ',
  "AAABJYFV" varchar(4) NOT NULL default '0000',
  "AAABFCUB" varchar(1) NOT NULL default ' ',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCUD" varchar(4) NOT NULL default '0000',
  "AAABFCWX" varchar(4) NOT NULL default ' ',
  "AAABFCTT" varchar(4) NOT NULL default ' ',
  "AAABFEPA" varchar(1) NOT NULL default ' ',
  "AAABFEPB" tinyint(3) unsigned NOT NULL default '0',
  "AAABFCTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFCTY","AAABKCPM","AAABJYFV","AAABFCUB","AAABLDUQ","AAABFCUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABFCUI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXIO" varchar(4) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXIO","AAABKIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABFCUX" varchar(1) NOT NULL default ' ',
  "AAABFCVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABKCPM" varchar(2) NOT NULL default ' ',
  "AAABFCVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL","AAABKCPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJXIO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJXIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXIO" varchar(4) NOT NULL default ' ',
  "AAABFCVT" varchar(30) NOT NULL default ' ',
  "AAABFCVU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJXIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCWX" varchar(4) NOT NULL default ' ',
  "AAABFCWG" varchar(4) NOT NULL default ' ',
  "AAABFCWU" int(11) NOT NULL default '0',
  "AAABFCWV" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFCWX","AAABFCWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABFCWL" varchar(30) NOT NULL default ' ',
  "AAABFCWM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFCWX" varchar(4) NOT NULL default ' ',
  "AAABFCWY" varchar(4) NOT NULL default ' ',
  "AAABFCWU" int(11) NOT NULL default '0',
  "AAABFCWV" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFCWX","AAABFCWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDKO" varchar(6) NOT NULL default ' ',
  "AAABFCXM" varchar(2) NOT NULL default ' ',
  "AAABFCXN" varchar(6) NOT NULL default ' ',
  "AAABFFKB" varchar(1) NOT NULL default ' ',
  "AAABFFKC" varchar(1) NOT NULL default ' ',
  "AAABFFKD" varchar(1) NOT NULL default ' ',
  "AAABFCXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABFDKO","AAABFCXM","AAABFCXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCXO" (
  "AAABJXUG" varchar(2) NOT NULL default ' ',
  "AAABFIFA" varchar(30) NOT NULL default ' ',
  "AAABFIFB" varchar(30) NOT NULL default ' ',
  "AAABFCXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJXUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCXU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJXUG" varchar(2) NOT NULL default ' ',
  "AAABFCXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJXUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCYA" (
  "AAABJXUG" varchar(2) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJXUG","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCYF" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABJXUG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABJXUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFCYY" varchar(1) NOT NULL default '0',
  "AAABFCYZ" varchar(3) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDUQ" varchar(4) NOT NULL default ' ',
  "AAABFCYY" varchar(1) NOT NULL default '0',
  "AAABFCYZ" varchar(3) NOT NULL default ' ',
  "AAABFCZA" varchar(5) NOT NULL default ' ',
  "AAABFCZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABFCZN" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKXRE","AAABFCZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDKO" varchar(6) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABFDKO","AAABKXRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFCZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABFDAI" varchar(1) NOT NULL default ' ',
  "AAABFDAC" varchar(250) NOT NULL default ' ',
  "AAABFDAD" varchar(250) NOT NULL default ' ',
  "AAABFDAE" varchar(250) NOT NULL default ' ',
  "AAABFDAF" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHFZ","AAABFDAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCPN" varchar(1) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCPN" varchar(1) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCPN","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFDBC" varchar(4) NOT NULL default '0000',
  "AAABFDBD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFDBC","AAABFDBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCPM" varchar(2) NOT NULL default ' ',
  "AAABJYFV" varchar(4) NOT NULL default '0000',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCPM","AAABJYFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDBM" (
  "AAABFDCE" varchar(2) NOT NULL default '00',
  "AAABFDBO" varchar(1) NOT NULL default ' ',
  "AAABFDBP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFDCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFDCE" varchar(2) NOT NULL default '00',
  "AAABFFIX" varchar(4) NOT NULL default ' ',
  "AAABJYFY" varchar(1) NOT NULL default ' ',
  "AAABFDBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFDCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDBZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFDCE" varchar(2) NOT NULL default '00',
  "AAABFDCC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFDCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYFP" varchar(4) NOT NULL default ' ',
  "AAABFDCI" varchar(1) NOT NULL default '0',
  "AAABFDCJ" varchar(1) NOT NULL default ' ',
  "AAABKKAM" varchar(10) NOT NULL default ' ',
  "AAABFDCL" varchar(1) NOT NULL default ' ',
  "AAABJYGA" varchar(4) NOT NULL default ' ',
  "AAABJYFY" varchar(1) NOT NULL default ' ',
  "AAABFDCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYFP" varchar(4) NOT NULL default ' ',
  "AAABFDCV" varchar(30) NOT NULL default ' ',
  "AAABFDCW" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJYFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDDA" (
  "AAABJYPW" varchar(4) NOT NULL default ' ',
  "AAABJYPX" varchar(30) NOT NULL default ' ',
  "AAABJYPY" varchar(20) NOT NULL default ' ',
  "AAABFDDE" varchar(60) NOT NULL default ' ',
  "AAABFDDF" varchar(30) NOT NULL default ' ',
  "AAABFDDG" varchar(5) NOT NULL default ' ',
  "AAABFDFP" varchar(30) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABFDDJ" varchar(40) NOT NULL default ' ',
  "AAABFDDK" varchar(30) NOT NULL default ' ',
  "AAABFDDL" varchar(30) NOT NULL default ' ',
  "AAABFDDM" varchar(30) NOT NULL default ' ',
  "AAABFDDN" varchar(120) NOT NULL default ' ',
  "AAABFDDO" varchar(30) NOT NULL default ' ',
  "AAABFDDP" varchar(30) NOT NULL default ' ',
  "AAABFDDQ" varchar(30) NOT NULL default ' ',
  "AAABFDDR" varchar(30) NOT NULL default ' ',
  "AAABFEBC" varchar(10) NOT NULL default ' ',
  "AAABFDDT" varchar(16) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABFDED" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYPW","AAABJYPX","AAABJYPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDEH" (
  "AAABJYPW" varchar(4) NOT NULL default ' ',
  "AAABFDEM" varchar(30) NOT NULL default ' ',
  "AAABIJQE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYPW","AAABFDEM","AAABIJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDEO" (
  "AAABJYPW" varchar(4) NOT NULL default ' ',
  "AAABJYPX" varchar(30) NOT NULL default ' ',
  "AAABJYPY" varchar(20) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFDEV" varchar(1) NOT NULL default ' ',
  "AAABFDEW" varchar(1) NOT NULL default ' ',
  "AAABFDEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYPW","AAABJYPX","AAABJYPY","AAABLGCM","AAABFDFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDFD" (
  "AAABJYPW" varchar(4) NOT NULL default ' ',
  "AAABFDFP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYPW","AAABFDFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDFI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYPW" varchar(4) NOT NULL default ' ',
  "AAABFDFP" varchar(30) NOT NULL default ' ',
  "AAABFDFM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJYPW","AAABFDFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFDHO" varchar(1) NOT NULL default ' ',
  "AAABFDHP" varchar(1) NOT NULL default ' ',
  "AAABFDHQ" varchar(1) NOT NULL default ' ',
  "AAABFDHR" varchar(1) NOT NULL default ' ',
  "AAABFDHS" varchar(1) NOT NULL default ' ',
  "AAABFDHT" varchar(1) NOT NULL default ' ',
  "AAABFDHU" varchar(1) NOT NULL default ' ',
  "AAABFDHV" varchar(1) NOT NULL default ' ',
  "AAABFDHW" varchar(1) NOT NULL default ' ',
  "AAABFDHX" varchar(1) NOT NULL default ' ',
  "AAABFDGC" varchar(1) NOT NULL default ' ',
  "AAABFDGD" varchar(1) NOT NULL default ' ',
  "AAABFDGE" varchar(1) NOT NULL default ' ',
  "AAABFDGF" varchar(1) NOT NULL default ' ',
  "AAABFDGG" varchar(1) NOT NULL default ' ',
  "AAABFDGH" varchar(13) NOT NULL default ' ',
  "AAABFDGI" varchar(22) NOT NULL default ' ',
  "AAABFDGJ" varchar(8) NOT NULL default ' ',
  "AAABFDGK" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABFDHO","AAABFDHP","AAABFDHQ","AAABFDHR","AAABFDHS","AAABFDHT","AAABFDHU","AAABFDHV","AAABFDHW","AAABFDHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFDHO" varchar(1) NOT NULL default ' ',
  "AAABFDHP" varchar(1) NOT NULL default ' ',
  "AAABFDHQ" varchar(1) NOT NULL default ' ',
  "AAABFDHR" varchar(1) NOT NULL default ' ',
  "AAABFDHS" varchar(1) NOT NULL default ' ',
  "AAABFDHT" varchar(1) NOT NULL default ' ',
  "AAABFDHU" varchar(1) NOT NULL default ' ',
  "AAABFDHV" varchar(1) NOT NULL default ' ',
  "AAABFDHW" varchar(1) NOT NULL default ' ',
  "AAABFDHX" varchar(1) NOT NULL default ' ',
  "AAABFDHJ" varchar(1) NOT NULL default ' ',
  "AAABFDHK" varchar(1) NOT NULL default ' ',
  "AAABFDHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFDHO","AAABFDHP","AAABFDHQ","AAABFDHR","AAABFDHS","AAABFDHT","AAABFDHU","AAABFDHV","AAABFDHW","AAABFDHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFCG" varchar(4) NOT NULL default ' ',
  "AAABFDIC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFCH" varchar(4) NOT NULL default ' ',
  "AAABFDIK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFCI" varchar(8) NOT NULL default ' ',
  "AAABFDIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFDJI" varchar(4) NOT NULL default '0000',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABFDJD" decimal(13,2) NOT NULL default '0.00',
  "AAABFDJE" decimal(13,2) NOT NULL default '0.00',
  "AAABFDJF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFDJI","AAABLAAI","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDJL" (
  "AAABIAEU" varchar(6) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIAEU","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABFDJZ" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFDJZ","AAABMREX","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDKO" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABGJXW" varchar(3) NOT NULL default '000',
  "AAABGJXX" varchar(1) NOT NULL default ' ',
  "AAABFDKJ" varchar(4) NOT NULL default ' ',
  "AAABFDKK" varchar(10) NOT NULL default ' ',
  "AAABFDKL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABFDKO","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABJYQI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAABJYQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABJYQI" varchar(4) NOT NULL default ' ',
  "AAABFDLC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCC","AAABJYQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAABLCEX" varchar(3) NOT NULL default ' ',
  "AAABMMXA" varchar(10) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABLCFJ" varchar(1) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLCFQ" varchar(12) NOT NULL default ' ',
  "AAABLCFR" varchar(15) NOT NULL default ' ',
  "AAABLCFS" varchar(30) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABFDMC" varchar(10) NOT NULL default ' ',
  "AAABLJCX" varchar(10) NOT NULL default ' ',
  "AAABLCFO" varchar(8) NOT NULL default '00000000',
  "AAABLCFP" varchar(2) NOT NULL default '00',
  "AAABLCFZ" varchar(8) NOT NULL default '00000000',
  "AAABLCFV" varchar(1) NOT NULL default ' ',
  "AAABLCFX" decimal(13,2) NOT NULL default '0.00',
  "AAABLCGC" decimal(3,0) NOT NULL default '0',
  "AAABJKQZ" varchar(1) NOT NULL default ' ',
  "AAABLCGB" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABLCGE" varchar(1) NOT NULL default ' ',
  "AAABMCVC" varchar(10) NOT NULL default ' ',
  "AAABFDMP" varchar(8) NOT NULL default '00000000',
  "AAABMCPY" varchar(2) NOT NULL default ' ',
  "AAABMCUR" varchar(2) NOT NULL default ' ',
  "AAABFDMS" varchar(3) NOT NULL default ' ',
  "AAABLCGG" varchar(10) NOT NULL default ' ',
  "AAABLCGH" varchar(2) NOT NULL default ' ',
  "AAABLCGK" varchar(2) NOT NULL default ' ',
  "AAABMCZD" varchar(2) NOT NULL default ' ',
  "AAABLOHP" varchar(2) NOT NULL default ' ',
  "AAABFDMY" varchar(1) NOT NULL default ' ',
  "AAABFDMZ" varchar(3) NOT NULL default ' ',
  "AAABFDNA" varchar(1) NOT NULL default ' ',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAABFDNC" varchar(4) NOT NULL default ' ',
  "AAABFDND" varchar(31) NOT NULL default ' ',
  "AAABFDNE" varchar(130) NOT NULL default ' ',
  "AAABFDNF" varchar(1) NOT NULL default ' ',
  "AAABFDNG" varchar(4) NOT NULL default ' ',
  "AAABMDAC" varchar(2) NOT NULL default ' ',
  "AAABFDNI" varchar(2) NOT NULL default ' ',
  "AAABLCFW" varchar(1) NOT NULL default ' ',
  "AAABFDNK" varchar(8) NOT NULL default '00000000',
  "AAABFDQR" varchar(1) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABFDQT" varchar(6) NOT NULL default '000000',
  "AAABFDQU" varchar(1) NOT NULL default ' ',
  "AAABFDNP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  "AAABFDNX" varchar(25) NOT NULL default ' ',
  "AAABLCDA" varchar(7) NOT NULL default '0000000',
  "AAABLCDB" varchar(5) NOT NULL default '00000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABLCDD" varchar(4) NOT NULL default ' ',
  "AAABLCDE" varchar(1) NOT NULL default '0',
  "AAABLVHZ" varchar(14) NOT NULL default ' ',
  "AAABJWRX" varchar(1) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCUY" varchar(11) NOT NULL default ' ',
  "AAABLCDI" varchar(10) NOT NULL default ' ',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABFDOM" varchar(10) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAABLRSQ" varchar(1) NOT NULL default ' ',
  "AAABFDOP" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCJJ" varchar(16) NOT NULL default ' ',
  "AAABLCJK" varchar(11) NOT NULL default ' ',
  "AAABLCJL" varchar(1) NOT NULL default ' ',
  "AAABLCED" varchar(1) NOT NULL default ' ',
  "AAABLVHY" varchar(15) NOT NULL default ' ',
  "AAABLXIZ" varchar(16) NOT NULL default ' ',
  "AAABLVHW" varchar(16) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABLVHX" varchar(30) NOT NULL default ' ',
  "AAABLVHT" varchar(30) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABLCEL" varchar(1) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABLCDG" varchar(10) NOT NULL default ' ',
  "AAABMCWL" varchar(20) NOT NULL default ' ',
  "AAABLCEO" varchar(1) NOT NULL default ' ',
  "AAABFDPH" varchar(2) NOT NULL default ' ',
  "AAABFDPI" varchar(25) NOT NULL default ' ',
  "AAABFDPJ" varchar(8) NOT NULL default '00000000',
  "AAABFDPK" varchar(1) NOT NULL default ' ',
  "AAABFDPL" varchar(11) NOT NULL default ' ',
  "AAABFDPM" varchar(8) NOT NULL default '00000000',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABFDPO" varchar(4) NOT NULL default ' ',
  "AAABLVHN" varchar(35) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABFDPR" varchar(1) NOT NULL default ' ',
  "AAABFDPS" varchar(1) NOT NULL default ' ',
  "AAABFDPT" varchar(2) NOT NULL default ' ',
  "AAABMCOO" varchar(1) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABFDPW" varchar(1) NOT NULL default ' ',
  "AAABJSBH" varchar(4) NOT NULL default ' ',
  "AAABHBAO" varchar(4) NOT NULL default ' ',
  "AAABLXWU" varchar(10) NOT NULL default ' ',
  "AAABFDQA" varchar(1) NOT NULL default ' ',
  "AAABHNBK" varchar(4) NOT NULL default ' ',
  "AAABJRLR" varchar(2) NOT NULL default ' ',
  "AAABJRLS" varchar(2) NOT NULL default ' ',
  "AAABFDQE" varchar(1) NOT NULL default ' ',
  "AAABFDQF" varchar(3) NOT NULL default ' ',
  "AAABJRLU" varchar(18) NOT NULL default ' ',
  "AAABJRLV" varchar(18) NOT NULL default ' ',
  "AAABFDQI" varchar(1) NOT NULL default ' ',
  "AAABFDQJ" varchar(1) NOT NULL default '0',
  "AAABILOK" varchar(30) NOT NULL default ' ',
  "AAABKYAG" varchar(2) NOT NULL default ' ',
  "AAABKPRG" varchar(10) NOT NULL default ' ',
  "AAABFDQN" varchar(132) NOT NULL default ' ',
  "AAABJRLX" varchar(10) NOT NULL default ' ',
  "AAABJRLY" varchar(30) NOT NULL default ' ',
  "AAABJRLZ" varchar(30) NOT NULL default ' ',
  "AAABFDQR" varchar(1) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABFDQT" varchar(6) NOT NULL default '000000',
  "AAABFDQU" varchar(1) NOT NULL default ' ',
  "AAABFDQV" varchar(8) NOT NULL default '00000000',
  "AAABFDQW" varchar(1) NOT NULL default ' ',
  "AAABFDQX" varchar(10) NOT NULL default ' ',
  "AAABFDQY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABFDRF" int(11) NOT NULL default '0',
  "AAABFDRG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKHFZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDCG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDCH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFGG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDSE" (
  "AAABFFDQ" varchar(4) NOT NULL default ' ',
  "AAABFDSG" varchar(30) NOT NULL default ' ',
  "AAABFDSH" varchar(30) NOT NULL default ' ',
  "AAABFDSI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFFDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABFDTL" varchar(4) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDTD" (
  "AAABFDTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFDTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDTG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFDTL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABFDTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKDCG" varchar(4) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDTU" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABKGHC" varchar(3) NOT NULL default ' ',
  "AAABJYUQ" varchar(3) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABFHRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABKGHC","AAABJYUQ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDUH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYRX" varchar(10) NOT NULL default ' ',
  "AAABFDUN" varchar(4) NOT NULL default '0000',
  "AAABFDUO" varchar(4) NOT NULL default '0000',
  "AAABFDUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABMGHG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABMGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABMGHG" varchar(40) NOT NULL default ' ',
  "AAABFDVE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPJD","AAABMGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDVJ" (
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABJZCQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWSS","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFDVS" varchar(3) NOT NULL default ' ',
  "AAABFDVT" varchar(3) NOT NULL default ' ',
  "AAABFDVU" varchar(3) NOT NULL default ' ',
  "AAABFDVV" varchar(1) NOT NULL default ' ',
  "AAABFDVW" varchar(1) NOT NULL default ' ',
  "AAABFDVX" varchar(1) NOT NULL default ' ',
  "AAABFDVY" varchar(1) NOT NULL default ' ',
  "AAABFDVZ" varchar(1) NOT NULL default ' ',
  "AAABFDWA" varchar(1) NOT NULL default ' ',
  "AAABKINQ" varchar(4) NOT NULL default ' ',
  "AAABFDWC" varchar(1) NOT NULL default ' ',
  "AAABFDWD" varchar(1) NOT NULL default ' ',
  "AAABFDWE" varchar(1) NOT NULL default ' ',
  "AAABFDWF" varchar(1) NOT NULL default ' ',
  "AAABFDWG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDWJ" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDWP" varchar(15) NOT NULL default ' ',
  "AAABFDWM" varchar(15) NOT NULL default ' ',
  "AAABFDWN" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLPJD","AAABFDWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGTW" varchar(1) NOT NULL default ' ',
  "AAABFDWT" varchar(5) NOT NULL default ' ',
  "AAABFDWU" varchar(1) NOT NULL default ' ',
  "AAABFDWV" varchar(1) NOT NULL default ' ',
  "AAABFDWW" varchar(1) NOT NULL default ' ',
  "AAABFDWX" varchar(1) NOT NULL default ' ',
  "AAABFDWY" varchar(1) NOT NULL default ' ',
  "AAABFDWZ" varchar(1) NOT NULL default ' ',
  "AAABFDXA" varchar(1) NOT NULL default ' ',
  "AAABFDXB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFDXL" varchar(2) NOT NULL default ' ',
  "AAABFDXH" int(11) NOT NULL default '0',
  "AAABFDXI" varchar(1) NOT NULL default ' ',
  "AAABFDXJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFDXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDXM" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDYF" varchar(4) NOT NULL default '0000',
  "AAABFDXP" varchar(4) NOT NULL default '0000',
  "AAABFDXQ" varchar(1) NOT NULL default ' ',
  "AAABFDXR" varchar(1) NOT NULL default ' ',
  "AAABFDXS" varchar(1) NOT NULL default ' ',
  "AAABFIFA" varchar(30) NOT NULL default ' ',
  "AAABFDXU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABFDYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDXX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFDYF" varchar(4) NOT NULL default '0000',
  "AAABFDYB" varchar(60) NOT NULL default ' ',
  "AAABFDYC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPJD","AAABFDYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDYG" (
  "AAABFDYS" varchar(20) NOT NULL default ' ',
  "AAABFDYI" varchar(10) NOT NULL default '0000000000',
  "AAABFDYJ" varchar(20) NOT NULL default ' ',
  "AAABFDYK" varchar(30) NOT NULL default ' ',
  "AAABFDYL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABFDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDYN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFDYS" varchar(20) NOT NULL default ' ',
  "AAABFDYQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFDYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDYT" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABFHMI" varchar(4) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABLTLR","AAABFHMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFDZH" varchar(1) NOT NULL default '0',
  "AAABFDZI" varchar(4) NOT NULL default ' ',
  "AAABIFOK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFDZH","AAABFDZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDZJ" (
  "AAABFIFA" varchar(30) NOT NULL default ' ',
  "AAABFDZO" varchar(30) NOT NULL default ' ',
  "AAABFDZM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIFA","AAABFDZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFDZP" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABGGJC" varchar(1) NOT NULL default ' ',
  "AAABFDZS" varchar(10) NOT NULL default ' ',
  "AAABFDZT" varchar(4) NOT NULL default '0000',
  "AAABFDZU" varchar(1) NOT NULL default ' ',
  "AAABFDZV" varchar(1) NOT NULL default ' ',
  "AAABFDZW" varchar(1) NOT NULL default ' ',
  "AAABFDZX" varchar(1) NOT NULL default ' ',
  "AAABFDZY" varchar(30) NOT NULL default ' ',
  "AAABFIFA" varchar(30) NOT NULL default ' ',
  "AAABFEAA" varchar(1) NOT NULL default ' ',
  "AAABFEAB" varchar(1) NOT NULL default ' ',
  "AAABFEAC" varchar(1) NOT NULL default ' ',
  "AAABFEAD" varchar(2) NOT NULL default ' ',
  "AAABFEAE" varchar(20) NOT NULL default ' ',
  "AAABFEAF" varchar(30) NOT NULL default ' ',
  "AAABFEAG" varchar(2) NOT NULL default '00',
  "AAABFEAH" varchar(40) NOT NULL default ' ',
  "AAABFEAI" varchar(4) NOT NULL default '0000',
  "AAABFEAJ" varchar(30) NOT NULL default ' ',
  "AAABFEAK" varchar(30) NOT NULL default ' ',
  "AAABFEAL" varchar(4) NOT NULL default ' ',
  "AAABFEAM" varchar(4) NOT NULL default ' ',
  "AAABFEAN" varchar(1) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABFEAP" varchar(1) NOT NULL default ' ',
  "AAABFEAQ" varchar(1) NOT NULL default ' ',
  "AAABFEAR" varchar(1) NOT NULL default ' ',
  "AAABFEAS" varchar(10) NOT NULL default ' ',
  "AAABFEAT" varchar(1) NOT NULL default ' ',
  "AAABFEAU" varchar(15) NOT NULL default ' ',
  "AAABFEAV" varchar(3) NOT NULL default ' ',
  "AAABFEAW" varchar(40) NOT NULL default ' ',
  "AAABFEAX" varchar(4) NOT NULL default ' ',
  "AAABFEAY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEBA" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABFEBC" varchar(10) NOT NULL default ' ',
  "AAABFEBD" varchar(20) NOT NULL default ' ',
  "AAABFEBE" varchar(20) NOT NULL default ' ',
  "AAABFEBF" varchar(1) NOT NULL default ' ',
  "AAABFEBG" varchar(1) NOT NULL default ' ',
  "AAABFEBH" varchar(1) NOT NULL default ' ',
  "AAABFEBI" varchar(4) NOT NULL default ' ',
  "AAABFEBJ" varchar(1) NOT NULL default ' ',
  "AAABFEBK" varchar(1) NOT NULL default ' ',
  "AAABLQRI" varchar(10) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEBO" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEBR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEBU" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABJYUQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABJYUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEBZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABJYUQ" varchar(3) NOT NULL default ' ',
  "AAABFECD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY","AAABJYUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFECH" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABKGHC" varchar(3) NOT NULL default ' ',
  "AAABLEJX" varchar(1) NOT NULL default ' ',
  "AAABFEIS" varchar(1) NOT NULL default ' ',
  "AAABFEIT" varchar(1) NOT NULL default ' ',
  "AAABFEIU" varchar(1) NOT NULL default ' ',
  "AAABFEIV" varchar(1) NOT NULL default ' ',
  "AAABFECP" varchar(1) NOT NULL default ' ',
  "AAABFECQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABKGHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFECT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABKGHC" varchar(3) NOT NULL default ' ',
  "AAABFECX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY","AAABKGHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEDB" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABMOIA" varchar(10) NOT NULL default ' ',
  "AAABFHMV" varchar(4) NOT NULL default ' ',
  "AAABLILS" varchar(4) NOT NULL default ' ',
  "AAABFHMZ" varchar(4) NOT NULL default ' ',
  "AAABFHNA" tinyint(3) unsigned NOT NULL default '0',
  "AAABFHNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEDL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABMOIA" varchar(10) NOT NULL default ' ',
  "AAABFHNH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFEEP" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABFEDX" varchar(1) NOT NULL default ' ',
  "AAABFEDY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFEEP","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFEEP" varchar(4) NOT NULL default ' ',
  "AAABFEEF" varchar(1) NOT NULL default ' ',
  "AAABFEEG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFEEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFEEP" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEEN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFEEP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEER" (
  "AAABJYXQ" varchar(4) NOT NULL default ' ',
  "AAABFEET" varchar(4) NOT NULL default '0000',
  "AAABFEEU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEEW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYXQ" varchar(4) NOT NULL default ' ',
  "AAABFEEZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJYXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEFC" (
  "AAABJYYO" varchar(4) NOT NULL default ' ',
  "AAABFEFE" varchar(4) NOT NULL default '0000',
  "AAABFEFF" varchar(1) NOT NULL default ' ',
  "AAABFEFG" varchar(1) NOT NULL default ' ',
  "AAABFEFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEFJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYYO" varchar(4) NOT NULL default ' ',
  "AAABFEFM" varchar(60) NOT NULL default ' ',
  "AAABFEFN" varchar(60) NOT NULL default ' ',
  "AAABFEFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJYYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEFR" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABJZAL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABJZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABJZAK" varchar(4) NOT NULL default ' ',
  "AAABFEGL" varchar(4) NOT NULL default ' ',
  "AAABFEGM" varchar(1) NOT NULL default ' ',
  "AAABFEGN" varchar(1) NOT NULL default ' ',
  "AAABFEGO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABJZAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZAL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZAL" varchar(4) NOT NULL default ' ',
  "AAABFFCO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZAK" varchar(4) NOT NULL default ' ',
  "AAABFEHI" varchar(1) NOT NULL default ' ',
  "AAABFEHJ" varchar(1) NOT NULL default ' ',
  "AAABFEHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZAK" varchar(4) NOT NULL default ' ',
  "AAABJZBD" varchar(2) NOT NULL default '00',
  "AAABJZAW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZAK","AAABJZBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZAK" varchar(4) NOT NULL default ' ',
  "AAABJZBD" varchar(2) NOT NULL default '00',
  "AAABFEIA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZAK","AAABJZBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZAK" varchar(4) NOT NULL default ' ',
  "AAABFEIJ" varchar(60) NOT NULL default ' ',
  "AAABFEIK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEIO" (
  "AAABKIFH" varchar(2) NOT NULL default ' ',
  "AAABKIFG" varchar(3) NOT NULL default ' ',
  "AAABLEJX" varchar(1) NOT NULL default ' ',
  "AAABFEIS" varchar(1) NOT NULL default ' ',
  "AAABFEIT" varchar(1) NOT NULL default ' ',
  "AAABFEIU" varchar(1) NOT NULL default ' ',
  "AAABFEIV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIFH","AAABKIFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEIY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKIFH" varchar(2) NOT NULL default ' ',
  "AAABKIFG" varchar(3) NOT NULL default ' ',
  "AAABFEJC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKIFH","AAABKIFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEJG" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABFEJJ" varchar(2) NOT NULL default ' ',
  "AAABFEJK" varchar(4) NOT NULL default '0000',
  "AAABFEJL" varchar(1) NOT NULL default ' ',
  "AAABFEJM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEJP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABFEJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPJD","AAABIFOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEJX" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABFHUH" tinyint(3) unsigned NOT NULL default '0',
  "AAABFHTS" varchar(1) NOT NULL default ' ',
  "AAABFHTT" varchar(1) NOT NULL default ' ',
  "AAABFHTU" varchar(1) NOT NULL default ' ',
  "AAABFHTV" varchar(1) NOT NULL default ' ',
  "AAABFHTW" varchar(1) NOT NULL default ' ',
  "AAABFEKG" varchar(30) NOT NULL default ' ',
  "AAABFEKH" varchar(1) NOT NULL default ' ',
  "AAABFEKI" varchar(1) NOT NULL default ' ',
  "AAABFEKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEKM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEKU" (
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABJZBY" varchar(4) NOT NULL default ' ',
  "AAABFHUH" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABKWJY","AAABLTLR","AAABJZBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFELC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABJZBY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY","AAABLTLR","AAABJZBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFELM" (
  "AAABFELX" varchar(4) NOT NULL default ' ',
  "AAABFELO" tinyint(3) unsigned NOT NULL default '0',
  "AAABFELP" varchar(4) NOT NULL default ' ',
  "AAABFELQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFELX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFELS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFELX" varchar(4) NOT NULL default ' ',
  "AAABFELV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFELX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFELY" (
  "AAABFENF" varchar(2) NOT NULL default ' ',
  "AAABFEND" tinyint(3) unsigned NOT NULL default '0',
  "AAABFEMB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFENF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEMD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFENF" varchar(2) NOT NULL default ' ',
  "AAABFEMG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFENF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEMJ" (
  "AAABJZCQ" varchar(4) NOT NULL default ' ',
  "AAABLJJO" varchar(70) NOT NULL default ' ',
  "AAABFEMM" varchar(30) NOT NULL default ' ',
  "AAABFEMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZCQ","AAABLJJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEMQ" (
  "AAABJZCQ" varchar(4) NOT NULL default ' ',
  "AAABFEMS" tinyint(3) unsigned NOT NULL default '0',
  "AAABFEMT" varchar(30) NOT NULL default ' ',
  "AAABFENE" varchar(30) NOT NULL default ' ',
  "AAABFEMV" varchar(1) NOT NULL default ' ',
  "AAABFEMW" varchar(1) NOT NULL default ' ',
  "AAABFEMX" varchar(1) NOT NULL default ' ',
  "AAABFEMY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFENA" (
  "AAABJZCQ" varchar(4) NOT NULL default ' ',
  "AAABFENH" varchar(4) NOT NULL default ' ',
  "AAABFEND" tinyint(3) unsigned NOT NULL default '0',
  "AAABFENE" varchar(30) NOT NULL default ' ',
  "AAABFENF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZCQ","AAABFENH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFENI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZCQ" varchar(4) NOT NULL default ' ',
  "AAABFENL" varchar(30) NOT NULL default ' ',
  "AAABFENM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJZCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFENP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKDCH" varchar(4) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKDCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFENX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNJ" varchar(4) NOT NULL default ' ',
  "AAABKHNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFGG" varchar(2) NOT NULL default ' ',
  "AAABKHNG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFESZ" varchar(4) NOT NULL default ' ',
  "AAABFEQG" varchar(4) NOT NULL default '0000',
  "AAABJZNI" varchar(70) NOT NULL default ' ',
  "AAABFEOS" varchar(1) NOT NULL default ' ',
  "AAABFEOT" varchar(1) NOT NULL default ' ',
  "AAABJZNL" varchar(70) NOT NULL default ' ',
  "AAABFEOV" varchar(1) NOT NULL default ' ',
  "AAABFEOW" varchar(1) NOT NULL default ' ',
  "AAABFEOX" varchar(1) NOT NULL default ' ',
  "AAABFEVU" varchar(4) NOT NULL default ' ',
  "AAABFEOZ" varchar(1) NOT NULL default ' ',
  "AAABFEPA" varchar(1) NOT NULL default '0',
  "AAABFEPB" smallint(6) NOT NULL default '0',
  "AAABFEPC" varchar(4) NOT NULL default '0000',
  "AAABFEPD" varchar(1) NOT NULL default '0',
  "AAABFEPE" smallint(6) NOT NULL default '0',
  "AAABFEPF" varchar(4) NOT NULL default '0000',
  "AAABFEPG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFESZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFESZ" varchar(4) NOT NULL default ' ',
  "AAABFEQC" varchar(15) NOT NULL default ' ',
  "AAABFEQD" varchar(30) NOT NULL default ' ',
  "AAABFEQE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFESZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEPT" (
  "AAABFEQG" varchar(4) NOT NULL default '0000',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABFEPW" varchar(30) NOT NULL default ' ',
  "AAABFEVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEPZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEQG" varchar(4) NOT NULL default '0000',
  "AAABFEQC" varchar(15) NOT NULL default ' ',
  "AAABFEQD" varchar(30) NOT NULL default ' ',
  "AAABFEQE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFEQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABJZMO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJFK","AAABJZMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABJZMO" varchar(10) NOT NULL default ' ',
  "AAABFEQT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJFK","AAABJZMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABJZMN" varchar(4) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABKHVZ" varchar(8) NOT NULL default '00000000',
  "AAABJZMB" varchar(8) NOT NULL default '00000000',
  "AAABJZMC" varchar(1) NOT NULL default ' ',
  "AAABJZMD" varchar(1) NOT NULL default ' ',
  "AAABJZME" decimal(19,6) NOT NULL default '0.000000',
  "AAABJZMF" varchar(4) NOT NULL default ' ',
  "AAABJZNI" varchar(70) NOT NULL default ' ',
  "AAABJZNL" varchar(70) NOT NULL default ' ',
  "AAABJZMI" varchar(4) NOT NULL default ' ',
  "AAABJZNO" varchar(70) NOT NULL default ' ',
  "AAABJZNR" varchar(70) NOT NULL default ' ',
  "AAABJZML" varchar(1) NOT NULL default '0',
  "AAABJZMM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFGAI","AAABJZMN","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFERU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABFEZQ" varchar(1) NOT NULL default ' ',
  "AAABFEZR" varchar(1) NOT NULL default ' ',
  "AAABFERZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFESC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABFESG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFGAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFESK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABFESO" varchar(4) NOT NULL default '0000',
  "AAABJZMN" varchar(4) NOT NULL default ' ',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGAI","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFESU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABKJBP" varchar(4) NOT NULL default ' ',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABFESZ" varchar(4) NOT NULL default ' ',
  "AAABJZNI" varchar(70) NOT NULL default ' ',
  "AAABJZNL" varchar(70) NOT NULL default ' ',
  "AAABFEVU" varchar(4) NOT NULL default ' ',
  "AAABJZNO" varchar(70) NOT NULL default ' ',
  "AAABJZNR" varchar(70) NOT NULL default ' ',
  "AAABFETF" varchar(1) NOT NULL default ' ',
  "AAABJZME" decimal(19,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFETJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABFETN" varchar(20) NOT NULL default ' ',
  "AAABFETO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFETS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZMN" varchar(4) NOT NULL default ' ',
  "AAABFEUV" varchar(4) NOT NULL default '0000',
  "AAABFETW" varchar(1) NOT NULL default ' ',
  "AAABFETX" varchar(1) NOT NULL default ' ',
  "AAABFETY" varchar(1) NOT NULL default ' ',
  "AAABFETZ" varchar(1) NOT NULL default ' ',
  "AAABFEUA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZMN" varchar(4) NOT NULL default ' ',
  "AAABFEUH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEUL" (
  "AAABFEUV" varchar(4) NOT NULL default '0000',
  "AAABFEUN" varchar(1) NOT NULL default ' ',
  "AAABFEUO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEUQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEUV" varchar(4) NOT NULL default '0000',
  "AAABFEUT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFEUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFEVU" varchar(4) NOT NULL default ' ',
  "AAABFEWH" varchar(4) NOT NULL default '0000',
  "AAABJZNO" varchar(70) NOT NULL default ' ',
  "AAABFEVB" varchar(1) NOT NULL default ' ',
  "AAABFEVC" varchar(1) NOT NULL default ' ',
  "AAABJZNR" varchar(70) NOT NULL default ' ',
  "AAABFEVE" varchar(1) NOT NULL default ' ',
  "AAABFEVF" varchar(1) NOT NULL default ' ',
  "AAABFEVG" varchar(1) NOT NULL default '0',
  "AAABFEVH" varchar(1) NOT NULL default '0',
  "AAABFEVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEVU" varchar(4) NOT NULL default ' ',
  "AAABFEWD" varchar(15) NOT NULL default ' ',
  "AAABFEWE" varchar(30) NOT NULL default ' ',
  "AAABFEWF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEVV" (
  "AAABFEWH" varchar(4) NOT NULL default '0000',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABFEVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFEWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEWA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFEWH" varchar(4) NOT NULL default '0000',
  "AAABFEWD" varchar(15) NOT NULL default ' ',
  "AAABFEWE" varchar(30) NOT NULL default ' ',
  "AAABFEWF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFEWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKBP" varchar(4) NOT NULL default ' ',
  "AAABFEWL" tinyint(3) unsigned NOT NULL default '0',
  "AAABKKBQ" smallint(6) NOT NULL default '0',
  "AAABKKBR" smallint(6) NOT NULL default '0',
  "AAABKKBS" smallint(6) NOT NULL default '0',
  "AAABFEWP" tinyint(3) unsigned NOT NULL default '0',
  "AAABKKBT" smallint(6) NOT NULL default '0',
  "AAABKKBU" varchar(2) NOT NULL default '00',
  "AAABKKBV" varchar(2) NOT NULL default ' ',
  "AAABFEWT" varchar(1) NOT NULL default ' ',
  "AAABKKBX" varchar(1) NOT NULL default ' ',
  "AAABFEWV" varchar(1) NOT NULL default '0',
  "AAABFEWW" varchar(1) NOT NULL default '0',
  "AAABFEWX" varchar(1) NOT NULL default ' ',
  "AAABFEWY" varchar(1) NOT NULL default ' ',
  "AAABFEWZ" smallint(6) NOT NULL default '0',
  "AAABFEXA" smallint(6) NOT NULL default '0',
  "AAABFEXB" varchar(1) NOT NULL default ' ',
  "AAABFEXC" varchar(1) NOT NULL default '0',
  "AAABFEXD" varchar(1) NOT NULL default '0',
  "AAABFEXE" varchar(1) NOT NULL default ' ',
  "AAABFEXF" varchar(1) NOT NULL default ' ',
  "AAABFEXG" smallint(6) NOT NULL default '0',
  "AAABFEXH" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKKBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKBP" varchar(4) NOT NULL default ' ',
  "AAABFEXO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKBY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKBY" varchar(10) NOT NULL default ' ',
  "AAABFEYG" varchar(2) NOT NULL default '00',
  "AAABFEYB" varchar(2) NOT NULL default '00',
  "AAABFEYC" varchar(2) NOT NULL default '00',
  "AAABFEYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKBY","AAABFEYG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKBY" varchar(10) NOT NULL default ' ',
  "AAABFEYL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZJM" varchar(3) NOT NULL default ' ',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABFEYT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZJM","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABLAAD" varchar(1) NOT NULL default ' ',
  "AAABFEZB" varchar(6) NOT NULL default ' ',
  "AAABFEZC" varchar(6) NOT NULL default ' ',
  "AAABFEZD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABFEZK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEZO" (
  "AAABKJBP" varchar(4) NOT NULL default ' ',
  "AAABFEZQ" varchar(1) NOT NULL default ' ',
  "AAABFEZR" varchar(1) NOT NULL default ' ',
  "AAABJVHC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKJBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFEZU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJBP" varchar(4) NOT NULL default ' ',
  "AAABFEZX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKJBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFAA" (
  "AAABJZJM" varchar(3) NOT NULL default ' ',
  "AAABIFOK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFAE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZJM" varchar(3) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJZJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  "AAABFFEM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  "AAABFFEM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHPW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHPW" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFBJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHPW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHPW" varchar(4) NOT NULL default ' ',
  "AAABKHQQ" varchar(3) NOT NULL default '000',
  "AAABKHQG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKHPW","AAABKHQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHPW" varchar(4) NOT NULL default ' ',
  "AAABKHQQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFCA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHPW","AAABKHQQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHRH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKHRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHRH" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFCO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHRH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFCS" (
  "AAABKGER" varchar(1) NOT NULL default ' ',
  "AAABFFCU" varchar(1) NOT NULL default ' ',
  "AAABFFCV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFCX" (
  "AAABKGER" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFDA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGER","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFDD" (
  "AAABFFDM" varchar(2) NOT NULL default '00',
  "AAABFFDF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABFFDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFDH" (
  "AAABFFDM" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFDK" varchar(30) NOT NULL default ' ',
  "AAABFFDL" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABFFDM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFDO" (
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABFFDQ" varchar(4) NOT NULL default ' ',
  "AAABIDBV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLDAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZWL" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABKCRJ" varchar(1) NOT NULL default ' ',
  "AAABFFDY" varchar(1) NOT NULL default ' ',
  "AAABFFDZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZWL","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKIX" varchar(10) NOT NULL default ' ',
  "AAABFFEM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGTX" varchar(4) NOT NULL default ' ',
  "AAABFFET" varchar(1) NOT NULL default ' ',
  "AAABKGTC" varchar(1) NOT NULL default ' ',
  "AAABKGTD" varchar(1) NOT NULL default ' ',
  "AAABKGTE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGTX" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFFFD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGTX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGTX" varchar(4) NOT NULL default ' ',
  "AAABKGUY" varchar(3) NOT NULL default '000',
  "AAABKGTY" varchar(3) NOT NULL default '000',
  "AAABKGTZ" varchar(3) NOT NULL default '000',
  "AAABKGUA" varchar(3) NOT NULL default '000',
  "AAABKGUB" varchar(1) NOT NULL default ' ',
  "AAABKGUC" varchar(3) NOT NULL default '000',
  "AAABKGUD" varchar(3) NOT NULL default '000',
  "AAABKGUE" varchar(3) NOT NULL default '000',
  "AAABKGUF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGTX","AAABKGUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZWL" varchar(4) NOT NULL default ' ',
  "AAABJZQP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZWL","AAABJZQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABFFGG" varchar(6) NOT NULL default ' ',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABFFGI" varchar(2) NOT NULL default ' ',
  "AAABFFGJ" varchar(255) NOT NULL default ' ',
  "AAABFFGK" varchar(4) NOT NULL default '0000',
  "AAABFFGL" varchar(4) NOT NULL default '0000',
  "AAABFFGM" varchar(6) NOT NULL default ' ',
  "AAABFFGN" varchar(6) NOT NULL default ' ',
  "AAABFFGO" varchar(4) NOT NULL default ' ',
  "AAABFFGP" varchar(1) NOT NULL default '0',
  "AAABLRXL" varchar(1) NOT NULL default '0',
  "AAABFFGR" varchar(2) NOT NULL default '00',
  "AAABKCES" varchar(4) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABKKAO" varchar(20) NOT NULL default ' ',
  "AAABFFGV" varchar(4) NOT NULL default ' ',
  "AAABFFGW" varchar(1) NOT NULL default ' ',
  "AAABFFGX" varchar(1) NOT NULL default ' ',
  "AAABFFUS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZWL" varchar(4) NOT NULL default ' ',
  "AAABFFHH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZWL","AAABFFHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFHU" (
  "AAABFFIG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABFFIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFHX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFFIG" varchar(2) NOT NULL default '00',
  "AAABFFIA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFFIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFIR" varchar(4) NOT NULL default ' ',
  "AAABFFIG" varchar(2) NOT NULL default '00',
  "AAABFFIH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFFIR" varchar(4) NOT NULL default ' ',
  "AAABFFIO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFFIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZZN" varchar(2) NOT NULL default '00',
  "AAABJZZL" varchar(1) NOT NULL default ' ',
  "AAABJZZM" varchar(1) NOT NULL default ' ',
  "AAABFFIX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZZN" varchar(2) NOT NULL default '00',
  "AAABFFJE" varchar(30) NOT NULL default ' ',
  "AAABFFJF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZZO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJZZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZZO" varchar(2) NOT NULL default '00',
  "AAABFFJS" varchar(30) NOT NULL default ' ',
  "AAABFFJT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZWL" varchar(4) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFKB" varchar(1) NOT NULL default ' ',
  "AAABFFKC" varchar(1) NOT NULL default ' ',
  "AAABFFKD" varchar(1) NOT NULL default ' ',
  "AAABKGFG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZWL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBXS" varchar(2) NOT NULL default ' ',
  "AAABKEWB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKBXS","AAABKEWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBXS" varchar(2) NOT NULL default ' ',
  "AAABKEWB" varchar(20) NOT NULL default ' ',
  "AAABFFKZ" varchar(3) NOT NULL default '000',
  "AAABFFLA" varchar(30) NOT NULL default ' ',
  "AAABIFNA" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKBXS","AAABKEWB","AAABFFKZ","AAABFFLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKBXS" varchar(2) NOT NULL default ' ',
  "AAABKEWB" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFFLG" varchar(30) NOT NULL default ' ',
  "AAABFFLH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKBXS","AAABKEWB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGTW" varchar(1) NOT NULL default ' ',
  "AAABFFLP" varchar(1) NOT NULL default ' ',
  "AAABFFLQ" varchar(1) NOT NULL default ' ',
  "AAABFFLR" varchar(1) NOT NULL default ' ',
  "AAABFFLS" varchar(1) NOT NULL default ' ',
  "AAABFFLT" varchar(1) NOT NULL default ' ',
  "AAABFFLU" varchar(1) NOT NULL default ' ',
  "AAABFFLV" varchar(1) NOT NULL default ' ',
  "AAABFFLW" varchar(1) NOT NULL default ' ',
  "AAABFFLX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFMF" (
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFMI" varchar(4) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABFPOY" varchar(20) NOT NULL default ' ',
  "AAABFFML" varchar(20) NOT NULL default ' ',
  "AAABFFRO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKAGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABFFMS" varchar(1) NOT NULL default ' ',
  "AAABFFMT" varchar(1) NOT NULL default ' ',
  "AAABFFMU" varchar(5) NOT NULL default ' ',
  "AAABFFMV" varchar(1) NOT NULL default ' ',
  "AAABFFMW" varchar(1) NOT NULL default ' ',
  "AAABFFRV" varchar(6) NOT NULL default ' ',
  "AAABFFRW" varchar(4) NOT NULL default ' ',
  "AAABFFRX" varchar(6) NOT NULL default ' ',
  "AAABFFRY" varchar(4) NOT NULL default ' ',
  "AAABFFRZ" varchar(6) NOT NULL default ' ',
  "AAABFFSA" varchar(4) NOT NULL default ' ',
  "AAABFFND" varchar(1) NOT NULL default ' ',
  "AAABFFNE" varchar(2) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAABFFNG" varchar(1) NOT NULL default ' ',
  "AAABFFNH" varchar(1) NOT NULL default ' ',
  "AAABFFNI" varchar(1) NOT NULL default ' ',
  "AAABFFNJ" varchar(1) NOT NULL default ' ',
  "AAABFFNK" varchar(12) NOT NULL default ' ',
  "AAABFFNL" varchar(1) NOT NULL default ' ',
  "AAABFFNM" varchar(1) NOT NULL default ' ',
  "AAABFFNN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABFFQH" varchar(30) NOT NULL default ' ',
  "AAABKAFN" varchar(4) NOT NULL default ' ',
  "AAABFFNV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAGD","AAABFFQH","AAABKAFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFOA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABFFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKAGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABFFOO" varchar(4) NOT NULL default '0000',
  "AAABFFOK" varchar(30) NOT NULL default ' ',
  "AAABFFOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAGD","AAABFFOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFN" varchar(4) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFN" varchar(4) NOT NULL default ' ',
  "AAABFFPI" varchar(4) NOT NULL default '0000',
  "AAABFFOZ" varchar(2) NOT NULL default ' ',
  "AAABFFPA" varchar(1) NOT NULL default ' ',
  "AAABFFPB" varchar(6) NOT NULL default ' ',
  "AAABFFPC" varchar(4) NOT NULL default ' ',
  "AAABFFPD" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(15) NOT NULL default ' ',
  "AAABFFPF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKAFN","AAABFFPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAFN" varchar(4) NOT NULL default ' ',
  "AAABFFPN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKAFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABFFQH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAGD","AAABFFQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAGD" varchar(4) NOT NULL default ' ',
  "AAABFFQH" varchar(30) NOT NULL default ' ',
  "AAABFFQD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKAGD","AAABFFQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(15) NOT NULL default ' ',
  "AAABFGSI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(15) NOT NULL default ' ',
  "AAABFFQS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABFFRL" varchar(2) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL","AAABFFRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFRM" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFRO" varchar(30) NOT NULL default ' ',
  "AAABFFRP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABFFRV" varchar(6) NOT NULL default ' ',
  "AAABFFRW" varchar(4) NOT NULL default ' ',
  "AAABFFRX" varchar(6) NOT NULL default ' ',
  "AAABFFRY" varchar(4) NOT NULL default ' ',
  "AAABFFRZ" varchar(6) NOT NULL default ' ',
  "AAABFFSA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFSD" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFSF" varchar(30) NOT NULL default ' ',
  "AAABFFSG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFTJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABFFTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFTJ" varchar(2) NOT NULL default ' ',
  "AAABFFSZ" varchar(2) NOT NULL default '00',
  "AAABIEMX" varchar(30) NOT NULL default ' ',
  "AAABLKWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABFFTJ","AAABFFSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFTJ" varchar(2) NOT NULL default ' ',
  "AAABFFTF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPJD","AAABFFTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFFTN" varchar(30) NOT NULL default ' ',
  "AAABFFTO" varchar(30) NOT NULL default ' ',
  "AAABFFTP" varchar(30) NOT NULL default ' ',
  "AAABFFTQ" varchar(30) NOT NULL default ' ',
  "AAABFFTR" varchar(30) NOT NULL default ' ',
  "AAABFFTS" varchar(30) NOT NULL default ' ',
  "AAABFFTT" varchar(30) NOT NULL default ' ',
  "AAABFFTU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFTX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFFUD" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  "AAABFFUB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFFUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABFFUN" varchar(6) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFUJ" varchar(2) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSAK","AAABFFUN","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFFUS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFWN" varchar(4) NOT NULL default ' ',
  "AAABKGMN" varchar(10) NOT NULL default ' ',
  "AAABFHPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFWN","AAABKGMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAABKGPS" varchar(4) NOT NULL default ' ',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABIYQF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABKGPS","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKKAJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKKAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABIFGH" varchar(1) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABFFWA" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFWN" varchar(4) NOT NULL default ' ',
  "AAABFFWO" varchar(15) NOT NULL default ' ',
  "AAABKGMN" varchar(10) NOT NULL default ' ',
  "AAABFFWK" varchar(1) NOT NULL default ' ',
  "AAABFFWL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFWN","AAABFFWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABKKAO" varchar(20) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAABFFWW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABKKAN","AAABKKAO","AAABLRLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAABFFXI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABKKAN","AAABLRLE","AAABLRLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABFFXY" varchar(2) NOT NULL default ' ',
  "AAABLRLE" varchar(8) NOT NULL default '00000000',
  "AAABLRLF" varchar(8) NOT NULL default '00000000',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABKKAO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABFFXY","AAABLRLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABFFYG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMPWP","AAABLDCD","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFME" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFZN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFME" varchar(8) NOT NULL default ' ',
  "AAABFFZA" varchar(50) NOT NULL default ' ',
  "AAABFFZB" varchar(30) NOT NULL default ' ',
  "AAABFFZC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFFZN" varchar(12) NOT NULL default ' ',
  "AAABFFZK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFFZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGCJ" varchar(12) NOT NULL default ' ',
  "AAABFFZR" varchar(4) NOT NULL default ' ',
  "AAABFGBU" varchar(12) NOT NULL default ' ',
  "AAABFFZT" varchar(12) NOT NULL default ' ',
  "AAABFFZU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFFZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGAY" varchar(12) NOT NULL default ' ',
  "AAABFGAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGAY" varchar(12) NOT NULL default ' ',
  "AAABFGBM" varchar(2) NOT NULL default '00',
  "AAABJUVZ" varchar(4) NOT NULL default ' ',
  "AAABFGAI" varchar(4) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKKBF" varchar(3) NOT NULL default '000',
  "AAABKKBG" varchar(1) NOT NULL default '0',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGAY","AAABFGBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGAY" varchar(12) NOT NULL default ' ',
  "AAABFGAV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFGAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGBU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGBU" varchar(12) NOT NULL default ' ',
  "AAABFGBM" varchar(2) NOT NULL default '00',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGBU","AAABFGBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGBU" varchar(12) NOT NULL default ' ',
  "AAABFGBR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGBV" (
  "AAABFGCB" varchar(4) NOT NULL default ' ',
  "AAABFGCN" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABFGBZ" varchar(30) NOT NULL default ' ',
  "AAABFGCA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGCJ" varchar(12) NOT NULL default ' ',
  "AAABFGCG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFGCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGCK" (
  "AAABFGCZ" varchar(12) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABFGCN" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABFGCP" varchar(30) NOT NULL default ' ',
  "AAABFGCQ" varchar(30) NOT NULL default ' ',
  "AAABFGCR" varchar(60) NOT NULL default ' ',
  "AAABFGCS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGCU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGCZ" varchar(12) NOT NULL default ' ',
  "AAABFHNH" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGDA" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFGDG" varchar(1) NOT NULL default ' ',
  "AAABFGDH" varchar(1) NOT NULL default ' ',
  "AAABFGDI" varchar(32) NOT NULL default ' ',
  "AAABFGDJ" varchar(32) NOT NULL default ' ',
  "AAABFGDK" varchar(32) NOT NULL default ' ',
  "AAABFGDL" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCES" varchar(4) NOT NULL default ' ',
  "AAABFGDR" decimal(9,6) NOT NULL default '0.000000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABFGDT" varchar(1) NOT NULL default ' ',
  "AAABFGDU" varchar(1) NOT NULL default ' ',
  "AAABFGDV" varchar(1) NOT NULL default ' ',
  "AAABFGDW" varchar(1) NOT NULL default ' ',
  "AAABFGDX" varchar(4) NOT NULL default ' ',
  "AAABFGDY" varchar(1) NOT NULL default ' ',
  "AAABFGDZ" varchar(1) NOT NULL default ' ',
  "AAABFGYN" varchar(1) NOT NULL default ' ',
  "AAABFGYL" varchar(1) NOT NULL default ' ',
  "AAABFGEC" varchar(1) NOT NULL default ' ',
  "AAABFGED" varchar(1) NOT NULL default ' ',
  "AAABFGEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCES" varchar(4) NOT NULL default ' ',
  "AAABFGEL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
