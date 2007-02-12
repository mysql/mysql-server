# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl39.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABFYCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABFYCS" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIWAM" varchar(1) NOT NULL default ' ',
  "AAABFYCW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLVCV","AAABFYTT","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLVCV","AAABFYTT","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMBFP" varchar(1) NOT NULL default ' ',
  "AAABFYDT" varchar(4) NOT NULL default ' ',
  "AAABFYDU" varchar(4) NOT NULL default ' ',
  "AAABFYDV" varchar(4) NOT NULL default ' ',
  "AAABFYDW" varchar(4) NOT NULL default ' ',
  "AAABFYDX" varchar(1) NOT NULL default ' ',
  "AAABFYDY" varchar(1) NOT NULL default ' ',
  "AAABFYDZ" varchar(1) NOT NULL default ' ',
  "AAABFYEA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC","AAABLXSU","AAABMBFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYEL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCV","AAABFYEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYEY" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYEW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYEY","AAABLVCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYFH" (
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFYFR" varchar(25) NOT NULL default ' ',
  "AAABFYFS" varchar(25) NOT NULL default ' ',
  "AAABFYFT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYFN" (
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABFYFR" varchar(25) NOT NULL default ' ',
  "AAABFYFS" varchar(25) NOT NULL default ' ',
  "AAABFYFT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYGD" (
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMMVO" varchar(5) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default '000',
  "AAABFYGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLVCV","AAABFYTT","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYGY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFYHC" varchar(1) NOT NULL default ' ',
  "AAABFYHD" varchar(1) NOT NULL default ' ',
  "AAABFYHE" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLVCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYHI" (
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCW","AAABMREF","AAABLVCY"),
  KEY "AAABFYHR" ("AAABLVCV"),
  KEY "AAABFYHT" ("AAABLVCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYHV" (
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCW","AAABMREF","AAABLVCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYIC" (
  "AAABFYIH" varchar(2) NOT NULL default ' ',
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYIH","AAABLVCW","AAABLVCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYIK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYIN" varchar(1) NOT NULL default ' ',
  "AAABFYIO" varchar(1) NOT NULL default ' ',
  "AAABFYIP" varchar(2) NOT NULL default ' ',
  "AAABFYIQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUXQ","AAABLVCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYIT" (
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCV","AAABFYJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYIY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYJV" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLVCV","AAABFYJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYJG" (
  "AAABFYJM" varchar(10) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABLVCY" varchar(30) NOT NULL default ' ',
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABFYJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYJM","AAABLVCV","AAABLVCY","AAABLVCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYJQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYJT" varchar(1) NOT NULL default ' ',
  "AAABFYJU" varchar(6) NOT NULL default '000000',
  "AAABFYJV" varchar(1) NOT NULL default ' ',
  "AAABLRWU" int(11) NOT NULL default '0',
  "AAABLORS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLVCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYKA" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYKH" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABFYLA" varchar(10) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABFYKL" varchar(1) NOT NULL default ' ',
  "AAABFYKM" varchar(1) NOT NULL default ' ',
  "AAABFYKN" varchar(3) NOT NULL default ' ',
  "AAABFYKO" varchar(8) NOT NULL default '00000000',
  "AAABFYKP" varchar(6) NOT NULL default '000000',
  "AAABFYKQ" varchar(10) NOT NULL default '0000000000',
  "AAABFYKR" varchar(8) NOT NULL default '00000000',
  "AAABFYKS" varchar(6) NOT NULL default '000000',
  "AAABFYKT" varchar(1) NOT NULL default ' ',
  "AAABFYKU" varchar(10) NOT NULL default '0000000000',
  "AAABFYKV" varchar(8) NOT NULL default '00000000',
  "AAABFYKW" varchar(6) NOT NULL default '000000',
  "AAABFYKX" varchar(8) NOT NULL default '00000000',
  "AAABFYKY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMONG","AAABFYLA","AAABFYLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYLE" varchar(1) NOT NULL default ' ',
  "AAABFYLF" varchar(1) NOT NULL default ' ',
  "AAABFYLG" varchar(1) NOT NULL default ' ',
  "AAABFYLH" varchar(1) NOT NULL default ' ',
  "AAABFYLI" varchar(1) NOT NULL default ' ',
  "AAABFYLJ" varchar(1) NOT NULL default ' ',
  "AAABFYLK" varchar(1) NOT NULL default ' ',
  "AAABFYLL" varchar(1) NOT NULL default ' ',
  "AAABFYLM" varchar(1) NOT NULL default ' ',
  "AAABFYLN" varchar(1) NOT NULL default ' ',
  "AAABMMPK" varchar(1) NOT NULL default ' ',
  "AAABFYLP" varchar(1) NOT NULL default ' ',
  "AAABFYLQ" varchar(1) NOT NULL default ' ',
  "AAABFYLR" varchar(1) NOT NULL default ' ',
  "AAABFYLS" varchar(1) NOT NULL default ' ',
  "AAABFYLT" varchar(1) NOT NULL default ' ',
  "AAABFYLU" varchar(1) NOT NULL default ' ',
  "AAABFYLV" varchar(1) NOT NULL default ' ',
  "AAABFYLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABKLWC" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(1) NOT NULL default ' ',
  "AAABFYME" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFYWO","AAABLVCV","AAABKLWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABKLWC" varchar(3) NOT NULL default '000',
  "AAABMOHX" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFYWO","AAABLVCV","AAABKLWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYMX" varchar(3) NOT NULL default ' ',
  "AAABMLEY" varchar(3) NOT NULL default ' ',
  "AAABFYMZ" varchar(1) NOT NULL default ' ',
  "AAABFYNA" varchar(1) NOT NULL default ' ',
  "AAABFYNB" varchar(1) NOT NULL default ' ',
  "AAABFYNC" varchar(1) NOT NULL default ' ',
  "AAABFYND" varchar(8) NOT NULL default ' ',
  "AAABFYNE" varchar(8) NOT NULL default ' ',
  "AAABFYNF" varchar(8) NOT NULL default ' ',
  "AAABFYNG" varchar(8) NOT NULL default ' ',
  "AAABFYNH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYPD" varchar(30) NOT NULL default ' ',
  "AAABFYNM" varchar(2) NOT NULL default ' ',
  "AAABFYNN" varchar(30) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYOH" varchar(8) NOT NULL default ' ',
  "AAABFYNW" smallint(6) NOT NULL default '0',
  "AAABFYNX" smallint(6) NOT NULL default '0',
  "AAABFYNY" smallint(6) NOT NULL default '0',
  "AAABFYNZ" smallint(6) NOT NULL default '0',
  "AAABFYOA" smallint(6) NOT NULL default '0',
  "AAABFYOB" varchar(18) NOT NULL default ' ',
  "AAABFYOC" varchar(18) NOT NULL default ' ',
  "AAABFYOD" varchar(1) NOT NULL default ' ',
  "AAABFYOE" varchar(1) NOT NULL default ' ',
  "AAABFYOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYPL" varchar(22) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABFYON" varchar(4) NOT NULL default '0000',
  "AAABFYOO" varchar(4) NOT NULL default '0000',
  "AAABJLKW" varchar(2) NOT NULL default '00',
  "AAABFYOQ" varchar(1) NOT NULL default ' ',
  "AAABFYOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYPL" varchar(22) NOT NULL default ' ',
  "AAABFYPD" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(24) NOT NULL default ' ',
  "AAABFYPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYPL","AAABFYPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFYPL" varchar(22) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFYPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYPX" varchar(4) NOT NULL default ' ',
  "AAABHLNU" varchar(4) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABLJFK" varchar(1) NOT NULL default ' ',
  "AAABHLSE" varchar(1) NOT NULL default ' ',
  "AAABHLNI" varchar(1) NOT NULL default ' ',
  "AAABFYPU" varchar(1) NOT NULL default ' ',
  "AAABFYPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYPX","AAABHLNU","AAABLXYW","AAABLJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYQB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default '000',
  "AAABGGKN" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLSIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABJURT" varchar(4) NOT NULL default ' ',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABFYQM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABJURT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFYQW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABFYQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(2) NOT NULL default ' ',
  "AAABJURT" varchar(3) NOT NULL default ' ',
  "AAABFYRB" varchar(4) NOT NULL default ' ',
  "AAABFYRC" varchar(2) NOT NULL default ' ',
  "AAABFYRD" varchar(3) NOT NULL default ' ',
  "AAABFYRE" varchar(4) NOT NULL default ' ',
  "AAABFYRF" varchar(2) NOT NULL default ' ',
  "AAABFYRG" varchar(3) NOT NULL default ' ',
  "AAABFYRH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUWW" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFYRN" varchar(34) NOT NULL default ' ',
  "AAABFYRO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUWW","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYSE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFYSE" varchar(8) NOT NULL default ' ',
  "AAABLPUZ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFYSE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYSF" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABFYSH" varchar(1) NOT NULL default ' ',
  "AAABFYSI" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABIHSD" varchar(18) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMKXW","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABFYTC" varchar(1) NOT NULL default ' ',
  "AAABFYTD" varchar(3) NOT NULL default '000',
  "AAABFYTE" varchar(1) NOT NULL default ' ',
  "AAABFYTF" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(1) NOT NULL default ' ',
  "AAABFYTH" varchar(2) NOT NULL default '00',
  "AAABFYTI" varchar(2) NOT NULL default '00',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABFYTK" varchar(1) NOT NULL default ' ',
  "AAABFYTL" varchar(4) NOT NULL default ' ',
  "AAABFYTM" varchar(4) NOT NULL default ' ',
  "AAABFYTN" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFYTP" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLVCV","AAABFYTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYTW" varchar(1) NOT NULL default ' ',
  "AAABFYTX" varchar(1) NOT NULL default ' ',
  "AAABFYTY" varchar(1) NOT NULL default '0',
  "AAABFYTZ" varchar(1) NOT NULL default '0',
  "AAABFYUA" varchar(1) NOT NULL default ' ',
  "AAABFYUB" varchar(1) NOT NULL default ' ',
  "AAABFYUC" varchar(1) NOT NULL default ' ',
  "AAABFYUD" varchar(1) NOT NULL default ' ',
  "AAABFYUE" varchar(1) NOT NULL default ' ',
  "AAABFYUF" varchar(1) NOT NULL default ' ',
  "AAABFYUG" varchar(1) NOT NULL default ' ',
  "AAABFYUH" varchar(1) NOT NULL default ' ',
  "AAABFYUI" varchar(1) NOT NULL default ' ',
  "AAABFYUJ" varchar(1) NOT NULL default ' ',
  "AAABFYUK" varchar(1) NOT NULL default ' ',
  "AAABFYUL" varchar(1) NOT NULL default ' ',
  "AAABFYUM" varchar(1) NOT NULL default ' ',
  "AAABFYUN" varchar(1) NOT NULL default ' ',
  "AAABLOUA" varchar(1) NOT NULL default ' ',
  "AAABFYUP" varchar(1) NOT NULL default ' ',
  "AAABFYUQ" varchar(1) NOT NULL default ' ',
  "AAABFYUR" varchar(1) NOT NULL default ' ',
  "AAABFYUS" varchar(1) NOT NULL default ' ',
  "AAABFYUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYVV" varchar(10) NOT NULL default ' ',
  "AAABFYVP" varchar(10) NOT NULL default ' ',
  "AAABFYVQ" varchar(10) NOT NULL default ' ',
  "AAABFYVR" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYVV" varchar(10) NOT NULL default ' ',
  "AAABFYVP" varchar(30) NOT NULL default ' ',
  "AAABFYVQ" varchar(30) NOT NULL default ' ',
  "AAABFYVR" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCV","AAABFYVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYWQ" varchar(3) NOT NULL default '000',
  "AAABFYWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYWO","AAABLVCV","AAABFYWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYWO" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYWQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYWO","AAABLVCV","AAABFYWQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYXD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFYXD" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFYXD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHXQO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHXQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHXQO" varchar(3) NOT NULL default ' ',
  "AAABFYXO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHXQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHXQO" varchar(3) NOT NULL default ' ',
  "AAABFYYT" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHXQO","AAABFYYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHXQO" varchar(3) NOT NULL default ' ',
  "AAABFYYT" varchar(16) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(10) NOT NULL default ' ',
  "AAABFZAB" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHXQO","AAABFYYT","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHXQO" varchar(3) NOT NULL default ' ',
  "AAABFYYT" varchar(16) NOT NULL default ' ',
  "AAABFYYP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHXQO","AAABFYYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(10) NOT NULL default ' ',
  "AAABFZAB" varchar(55) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(10) NOT NULL default ' ',
  "AAABFZAB" varchar(55) NOT NULL default ' ',
  "AAABFYZR" varchar(3) NOT NULL default '000',
  "AAABFYZG" varchar(30) NOT NULL default ' ',
  "AAABFYZH" varchar(2) NOT NULL default ' ',
  "AAABFYZI" varchar(30) NOT NULL default ' ',
  "AAABFYZJ" varchar(4) NOT NULL default ' ',
  "AAABFYZK" varchar(6) NOT NULL default '000000',
  "AAABFYZL" varchar(1) NOT NULL default ' ',
  "AAABFYZM" varchar(1) NOT NULL default ' ',
  "AAABFYZN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABFZAB","AAABFYZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(10) NOT NULL default ' ',
  "AAABFZAB" varchar(55) NOT NULL default ' ',
  "AAABFYZX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMODI","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZAC" (
  "AAABFZAP" varchar(1) NOT NULL default ' ',
  "AAABFZAE" varchar(30) NOT NULL default ' ',
  "AAABFZAF" varchar(40) NOT NULL default ' ',
  "AAABFZAG" varchar(40) NOT NULL default ' ',
  "AAABFZAH" varchar(40) NOT NULL default ' ',
  "AAABFZAI" varchar(30) NOT NULL default ' ',
  "AAABFZAJ" varchar(30) NOT NULL default ' ',
  "AAABFZAK" varchar(40) NOT NULL default ' ',
  "AAABFZAL" varchar(40) NOT NULL default ' ',
  "AAABFZAM" varchar(40) NOT NULL default ' ',
  "AAABFZAN" varchar(40) NOT NULL default ' ',
  "AAABFZAO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZAQ" (
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAABFZAV" varchar(5) NOT NULL default '00000',
  "AAABFZAX" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSLF","AAABFZAV"),
  UNIQUE KEY "AAABFZAW" ("AAABFZAX","AAABJSLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOLZ" varchar(2) NOT NULL default ' ',
  "AAABFZBC" varchar(1) NOT NULL default ' ',
  "AAABFZBD" varchar(1) NOT NULL default ' ',
  "AAABFZBE" varchar(1) NOT NULL default ' ',
  "AAABFZBF" varchar(1) NOT NULL default ' ',
  "AAABFZBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZBJ" (
  "AAABFZBP" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(4) NOT NULL default ' ',
  "AAABKMLX" varchar(12) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZBP","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZBR" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFZCC" varchar(2) NOT NULL default ' ',
  "AAABFZBV" varchar(45) NOT NULL default ' ',
  "AAABFZBW" varchar(6) NOT NULL default '000000',
  "AAABFZBX" varchar(4) NOT NULL default ' ',
  "AAABMRHK" varchar(6) NOT NULL default '000000',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABMOHM","AAABFZCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZCD" (
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
  KEY "AAABFZDT" ("AAABJAAI"),
  KEY "AAABFZDV" ("AAABMRHK","AAABHZRB"),
  KEY "AAABFZDY" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABFZEB" ("AAABHZOJ"),
  KEY "AAABFZED" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZEG" (
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
  KEY "AAABFZFC" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZFF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABFZFT" ("AAABMRHK","AAABHZRB"),
  KEY "AAABFZFW" ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZFY" (
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
  KEY "AAABFZHO" ("AAABMRHK","AAABHZRB"),
  KEY "AAABFZHR" ("AAABHZRB","AAABLFYJ"),
  KEY "AAABFZHU" ("AAABHZOJ"),
  KEY "AAABFZHW" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZHZ" (
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
  KEY "AAABFZIV" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZIY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABFZJM" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZJP" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZJZ" (
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABLRJL" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZKF" varchar(1) NOT NULL default ' ',
  "AAABFZKG" varchar(16) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZKL" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZKT" (
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFZKX" varchar(30) NOT NULL default ' ',
  "AAABFZKY" varchar(30) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZLC" (
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZSF","AAABFZRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZLI" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABFZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZLO" (
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABLRJL" varchar(30) NOT NULL default ' ',
  "AAABIYUV" varchar(1) NOT NULL default ' ',
  "AAABHJSK" varchar(30) NOT NULL default ' ',
  "AAABFZLT" varchar(30) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZLW" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  "AAABFZMA" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZMD" (
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZNJ","AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZMJ" (
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZNJ","AAABLEHK","AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZMR" (
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABFZTW" varchar(2) NOT NULL default ' ',
  "AAABFZNA" varchar(16) NOT NULL default ' ',
  "AAABFZMV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZMX" (
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZNA" varchar(16) NOT NULL default ' ',
  "AAABFZNB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZNJ","AAABLEHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZNE" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFZNJ" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABFZNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZNK" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZQE" varchar(1) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZNX" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZOJ" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABFZUW" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZRK" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZOX" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABFZVE" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZSA" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZPL" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZPQ" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLEHK","AAABFZSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZPY" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZQE" varchar(1) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLEHK","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZQN" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLEHK","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZRB" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABFZUW" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZRK" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLEHK","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZRR" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLEHK" varchar(2) NOT NULL default ' ',
  "AAABFZSF" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAABFZRX" varchar(1) NOT NULL default ' ',
  "AAABFZVE" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZSA" varchar(1) NOT NULL default ' ',
  "AAABFZSB" varchar(1) NOT NULL default ' ',
  "AAABFZSC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLEHK","AAABFZSF","AAABGBHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZSH" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZSM" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZTP" varchar(30) NOT NULL default ' ',
  "AAABFZTQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZSU" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZTP" varchar(30) NOT NULL default ' ',
  "AAABFZTQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZTC" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZTP" varchar(30) NOT NULL default ' ',
  "AAABFZTQ" varchar(30) NOT NULL default ' ',
  "AAABFZTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZTL" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZTS" varchar(16) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABFZTP" varchar(30) NOT NULL default ' ',
  "AAABFZTQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABFZTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZTT" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABFZTW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZTY" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABLOZE" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZUD" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZUJ" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZUL" varchar(4) NOT NULL default ' ',
  "AAABFZUX" varchar(30) NOT NULL default ' ',
  "AAABFZUY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZUP" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZUR" varchar(30) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZUU" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZUW" varchar(30) NOT NULL default ' ',
  "AAABFZUX" varchar(30) NOT NULL default ' ',
  "AAABFZUY" varchar(30) NOT NULL default ' ',
  "AAABFZUZ" varchar(1) NOT NULL default ' ',
  "AAABFZVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZVC" (
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABFZVE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZVG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFZVL" varchar(16) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABFZVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  "AAABGGHA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFZVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFZVY" decimal(13,2) NOT NULL default '0.00',
  "AAABFZVZ" varchar(1) NOT NULL default ' ',
  "AAABFZWA" varchar(1) NOT NULL default ' ',
  "AAABFZWB" decimal(4,2) NOT NULL default '0.00',
  "AAABFZWC" varchar(1) NOT NULL default ' ',
  "AAABFZWD" varchar(1) NOT NULL default ' ',
  "AAABFZWE" varchar(1) NOT NULL default ' ',
  "AAABFZWF" varchar(1) NOT NULL default ' ',
  "AAABKMKG" decimal(13,2) NOT NULL default '0.00',
  "AAABFZWH" varchar(1) NOT NULL default ' ',
  "AAABFZWI" varchar(1) NOT NULL default ' ',
  "AAABFZWJ" decimal(4,2) NOT NULL default '0.00',
  "AAABFZWK" varchar(1) NOT NULL default ' ',
  "AAABFZWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFZWY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZW" varchar(4) NOT NULL default ' ',
  "AAABLPGA" varchar(10) NOT NULL default ' ',
  "AAABLOZZ" varchar(4) NOT NULL default ' ',
  "AAABLPAA" varchar(2) NOT NULL default ' ',
  "AAABLPAB" varchar(2) NOT NULL default ' ',
  "AAABLPAC" varchar(10) NOT NULL default ' ',
  "AAABLPAE" varchar(3) NOT NULL default '000',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABLPCX" varchar(1) NOT NULL default ' ',
  "AAABLOZV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZW" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHRCM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXUT" varchar(8) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFZYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLJDA" varchar(2) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABHYJM" varchar(2) NOT NULL default ' ',
  "AAABHYJN" varchar(2) NOT NULL default ' ',
  "AAABHOOP" varchar(6) NOT NULL default '000000',
  "AAABHNCJ" varchar(1) NOT NULL default ' ',
  "AAABFZYV" varchar(1) NOT NULL default ' ',
  "AAABFZYW" varchar(4) NOT NULL default ' ',
  "AAABFZYX" varchar(2) NOT NULL default ' ',
  "AAABFZYY" varchar(6) NOT NULL default ' ',
  "AAABFZYZ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABHNDA" varchar(1) NOT NULL default ' ',
  "AAABLJHC" varchar(4) NOT NULL default ' ',
  "AAABHNDE" varchar(1) NOT NULL default ' ',
  "AAABHNCO" varchar(4) NOT NULL default ' ',
  "AAABHNCP" varchar(4) NOT NULL default ' ',
  "AAABLUKA" varchar(2) NOT NULL default ' ',
  "AAABFZZI" varchar(1) NOT NULL default ' ',
  "AAABGJUJ" varchar(1) NOT NULL default ' ',
  "AAABFZZK" varchar(1) NOT NULL default ' ',
  "AAABHLNN" varchar(1) NOT NULL default ' ',
  "AAABFZZM" varchar(1) NOT NULL default ' ',
  "AAABFZZN" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCZS" varchar(1) NOT NULL default ' ',
  "AAABHNDV" varchar(1) NOT NULL default ' ',
  "AAABFZZR" varchar(1) NOT NULL default ' ',
  "AAABFZZS" varchar(1) NOT NULL default ' ',
  "AAABFZZT" varchar(1) NOT NULL default ' ',
  "AAABFZZU" varchar(3) NOT NULL default ' ',
  "AAABFZZV" varchar(3) NOT NULL default ' ',
  "AAABHRHF" varchar(3) NOT NULL default ' ',
  "AAABFZZX" varchar(3) NOT NULL default ' ',
  "AAABFZZY" varchar(1) NOT NULL default ' ',
  "AAABFZZZ" varchar(1) NOT NULL default ' ',
  "AAABGAAA" varchar(1) NOT NULL default ' ',
  "AAABGAAB" varchar(1) NOT NULL default ' ',
  "AAABGAAC" varchar(1) NOT NULL default ' ',
  "AAABGAAD" varchar(1) NOT NULL default ' ',
  "AAABGAAE" varchar(6) NOT NULL default '000000',
  "AAABLYCG" varchar(2) NOT NULL default ' ',
  "AAABGAAG" varchar(2) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABGAAI" varchar(4) NOT NULL default ' ',
  "AAABGAAJ" varchar(1) NOT NULL default ' ',
  "AAABGAAK" varchar(1) NOT NULL default ' ',
  "AAABGAAL" varchar(1) NOT NULL default ' ',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABGAAP" varchar(1) NOT NULL default ' ',
  "AAABGAAQ" varchar(4) NOT NULL default ' ',
  "AAABGAAR" varchar(1) NOT NULL default ' ',
  "AAABGAAS" varchar(1) NOT NULL default ' ',
  "AAABHNCQ" varchar(6) NOT NULL default ' ',
  "AAABGCQH" varchar(4) NOT NULL default ' ',
  "AAABGCQI" varchar(4) NOT NULL default ' ',
  "AAABGAAW" varchar(1) NOT NULL default ' ',
  "AAABMCVX" varchar(4) NOT NULL default ' ',
  "AAABGAAY" varchar(1) NOT NULL default ' ',
  "AAABGAAZ" varchar(1) NOT NULL default ' ',
  "AAABGABA" varchar(1) NOT NULL default ' ',
  "AAABGABB" varchar(2) NOT NULL default ' ',
  "AAABGABC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGABF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGACI","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGABO" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGABR" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGABW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGACI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGACB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGACI" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGACI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGACJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGACR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGADI" varchar(3) NOT NULL default ' ',
  "AAABGACU" varchar(1) NOT NULL default ' ',
  "AAABGACV" varchar(1) NOT NULL default ' ',
  "AAABGACW" varchar(3) NOT NULL default ' ',
  "AAABGACX" varchar(1) NOT NULL default ' ',
  "AAABGACY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGADI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGADB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGADI" varchar(3) NOT NULL default ' ',
  "AAABHSKC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGADI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGADJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIIT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGADO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIIT" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGADW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAEC" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABKRPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAEP" varchar(2) NOT NULL default ' ',
  "AAABGAEQ" varchar(2) NOT NULL default ' ',
  "AAABGAEH" varchar(1) NOT NULL default ' ',
  "AAABLSLM" varchar(1) NOT NULL default ' ',
  "AAABGAEJ" varchar(1) NOT NULL default ' ',
  "AAABGFYY" varchar(20) NOT NULL default ' ',
  "AAABIXBL" varchar(1) NOT NULL default ' ',
  "AAABGAEM" varchar(3) NOT NULL default '000',
  "AAABGAEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAEP","AAABGAEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAET" varchar(1) NOT NULL default ' ',
  "AAABGAEU" varchar(1) NOT NULL default ' ',
  "AAABGAEV" varchar(4) NOT NULL default ' ',
  "AAABGAEW" varchar(1) NOT NULL default ' ',
  "AAABGAEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDNL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAIJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGAIJ" varchar(4) NOT NULL default ' ',
  "AAABGAGT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGAIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAIL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABGAGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAGO" varchar(1) NOT NULL default ' ',
  "AAABGAJM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABGAGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGAIL" varchar(2) NOT NULL default ' ',
  "AAABGAGT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGAIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAHN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABGAHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAHN" varchar(1) NOT NULL default ' ',
  "AAABGAJM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABGAHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAIJ" varchar(4) NOT NULL default ' ',
  "AAABGAIL" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGAHT" varchar(6) NOT NULL default ' ',
  "AAABGAHU" varchar(6) NOT NULL default ' ',
  "AAABGAHV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAIJ","AAABGAIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAIJ" varchar(4) NOT NULL default ' ',
  "AAABLUKA" varchar(2) NOT NULL default ' ',
  "AAABGAIL" varchar(2) NOT NULL default ' ',
  "AAABHOAX" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGAIG" varchar(6) NOT NULL default ' ',
  "AAABGAIH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAIJ","AAABLUKA","AAABGAIL","AAABHOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUKA" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABGAJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABGAJQ" varchar(1) NOT NULL default ' ',
  "AAABGAJM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMP","AAABGAJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABGAJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABGAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABKRHS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRHS" varchar(4) NOT NULL default ' ',
  "AAABGAKI" varchar(1) NOT NULL default ' ',
  "AAABGAKJ" varchar(1) NOT NULL default ' ',
  "AAABGAKK" varchar(1) NOT NULL default ' ',
  "AAABGAKL" varchar(1) NOT NULL default ' ',
  "AAABGAKM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJFN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGALC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJFN" varchar(3) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGALK" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGALR" varchar(30) NOT NULL default ' ',
  "AAABGALO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLECD","AAABMREL","AAABGALR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGALS" (
  "AAABGAMQ" varchar(11) NOT NULL default ' ',
  "AAABGAMR" varchar(3) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAABGALZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGAMQ","AAABGAMR","AAABGYQM","AAABGRZB","AAABMPWP","AAABMCPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAMG" (
  "AAABGAMQ" varchar(11) NOT NULL default ' ',
  "AAABGAMR" varchar(3) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAABGAML" varchar(1) NOT NULL default ' ',
  "AAABGAMM" varchar(1) NOT NULL default ' ',
  "AAABGAMN" varchar(1) NOT NULL default ' ',
  "AAABGAMO" varchar(1) NOT NULL default ' ',
  "AAABGAMP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGAMQ","AAABGAMR","AAABGYQM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCQZ" varchar(4) NOT NULL default ' ',
  "AAABGAMX" varchar(4) NOT NULL default ' ',
  "AAABLKIU" varchar(2) NOT NULL default ' ',
  "AAABGAMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGANC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGANW" varchar(4) NOT NULL default ' ',
  "AAABGANF" decimal(5,3) NOT NULL default '0.000',
  "AAABGANG" decimal(15,2) NOT NULL default '0.00',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABGANI" decimal(7,3) NOT NULL default '0.000',
  "AAABGANJ" varchar(1) NOT NULL default ' ',
  "AAABGANK" varchar(1) NOT NULL default ' ',
  "AAABGANL" varchar(1) NOT NULL default ' ',
  "AAABGANM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGANW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGANP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGANW" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGANW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGANX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJAS" varchar(4) NOT NULL default ' ',
  "AAABGAOA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJAS" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGAPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAPD" varchar(4) NOT NULL default ' ',
  "AAABLJAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAPD","AAABLJAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGCQZ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGCQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAPN" (
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGARW" smallint(6) NOT NULL default '0',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  "AAABGAPU" varchar(1) NOT NULL default ' ',
  "AAABGAPV" varchar(1) NOT NULL default ' ',
  "AAABGAPW" varchar(1) NOT NULL default ' ',
  "AAABGAPX" varchar(30) NOT NULL default ' ',
  "AAABGAPY" varchar(30) NOT NULL default ' ',
  "AAABGAPZ" varchar(30) NOT NULL default ' ',
  "AAABGAQA" varchar(1) NOT NULL default ' ',
  "AAABGAQB" varchar(1) NOT NULL default ' ',
  "AAABGAQC" varchar(1) NOT NULL default ' ',
  "AAABGAQD" varchar(1) NOT NULL default ' ',
  "AAABGAQE" varchar(1) NOT NULL default ' ',
  "AAABGAQF" varchar(1) NOT NULL default ' ',
  "AAABGAQG" varchar(1) NOT NULL default ' ',
  "AAABGAQH" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABGAQJ" varchar(1) NOT NULL default ' ',
  "AAABGAQK" varchar(1) NOT NULL default ' ',
  "AAABLENR" varchar(2) NOT NULL default ' ',
  "AAABGAQM" varchar(2) NOT NULL default ' ',
  "AAABGAQN" varchar(2) NOT NULL default ' ',
  "AAABGAQO" varchar(2) NOT NULL default ' ',
  "AAABGAQP" varchar(12) NOT NULL default ' ',
  "AAABGAQQ" varchar(12) NOT NULL default ' ',
  "AAABGAQR" varchar(12) NOT NULL default ' ',
  "AAABGAQS" varchar(12) NOT NULL default ' ',
  "AAABGAQT" varchar(30) NOT NULL default ' ',
  "AAABGAQU" varchar(30) NOT NULL default ' ',
  "AAABGAQV" varchar(20) NOT NULL default ' ',
  "AAABGAQW" varchar(20) NOT NULL default ' ',
  "AAABGAQX" varchar(8) NOT NULL default '00000000',
  "AAABGAQY" varchar(8) NOT NULL default '00000000',
  "AAABGAQZ" varchar(6) NOT NULL default '000000',
  "AAABGARA" varchar(6) NOT NULL default '000000',
  "AAABGARB" varchar(1) NOT NULL default ' ',
  "AAABGARC" varchar(10) NOT NULL default ' ',
  "AAABGARD" varchar(10) NOT NULL default ' ',
  "AAABGARE" varchar(1) NOT NULL default ' ',
  "AAABGARF" varchar(1) NOT NULL default ' ',
  "AAABGARG" varchar(1) NOT NULL default ' ',
  "AAABGARH" varchar(1) NOT NULL default ' ',
  "AAABGARI" varchar(1) NOT NULL default ' ',
  "AAABGARJ" varchar(1) NOT NULL default ' ',
  "AAABGARK" varchar(1) NOT NULL default ' ',
  "AAABGARL" varchar(1) NOT NULL default ' ',
  "AAABGARM" varchar(1) NOT NULL default ' ',
  "AAABGARN" varchar(1) NOT NULL default ' ',
  "AAABGARO" varchar(1) NOT NULL default ' ',
  "AAABGARP" varchar(1) NOT NULL default ' ',
  "AAABGARQ" varchar(1) NOT NULL default ' ',
  "AAABGARR" varchar(1) NOT NULL default ' ',
  "AAABGARS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS","AAABMIRX","AAABMIRY","AAABGARW","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGARY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPRA" varchar(4) NOT NULL default ' ',
  "AAABGASB" varchar(1) NOT NULL default ' ',
  "AAABGASC" varchar(1) NOT NULL default ' ',
  "AAABGASD" varchar(1) NOT NULL default ' ',
  "AAABGASE" varchar(1) NOT NULL default ' ',
  "AAABGASF" varchar(1) NOT NULL default ' ',
  "AAABGASG" varchar(1) NOT NULL default ' ',
  "AAABGASH" varchar(1) NOT NULL default ' ',
  "AAABGASI" varchar(1) NOT NULL default ' ',
  "AAABGASJ" varchar(1) NOT NULL default ' ',
  "AAABGASK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGASN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPRA" varchar(4) NOT NULL default ' ',
  "AAABGASR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGASV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGATX" varchar(10) NOT NULL default ' ',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABKRIX" varchar(20) NOT NULL default ' ',
  "AAABKPNT" varchar(40) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABGATM" varchar(2) NOT NULL default ' ',
  "AAABGATN" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABKRYH" varchar(10) NOT NULL default ' ',
  "AAABLUFO" varchar(10) NOT NULL default ' ',
  "AAABLIPI" varchar(5) NOT NULL default '00000',
  "AAABGATU" varchar(4) NOT NULL default ' ',
  "AAABGATV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGATX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGATY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAV" varchar(5) NOT NULL default ' ',
  "AAABKWRQ" varchar(2) NOT NULL default ' ',
  "AAABGAUJ" varchar(2) NOT NULL default ' ',
  "AAABGAUK" varchar(2) NOT NULL default '00',
  "AAABGAUU" varchar(5) NOT NULL default ' ',
  "AAABGAVG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAV","AAABKWRQ","AAABGAUJ","AAABGAUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAV" varchar(5) NOT NULL default ' ',
  "AAABKWRQ" varchar(2) NOT NULL default ' ',
  "AAABGAUU" varchar(5) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAV","AAABKWRQ","AAABGAUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAV" varchar(5) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABIDOF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLOAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAV" varchar(5) NOT NULL default ' ',
  "AAABGAVJ" varchar(5) NOT NULL default ' ',
  "AAABGAVG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAV","AAABGAVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAV" varchar(5) NOT NULL default ' ',
  "AAABGAVR" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAV","AAABGAVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABGAVX" varchar(1) NOT NULL default ' ',
  "AAABGAVY" varchar(1) NOT NULL default ' ',
  "AAABGAVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABMPTW","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABLJHQ" varchar(1) NOT NULL default ' ',
  "AAABGYCL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZZR","AAABLJHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAWM" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABGBUA" varchar(8) NOT NULL default '00000000',
  "AAABGBUB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABGBAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAWV" (
  "AAABLKWV" varchar(8) NOT NULL default '00000000',
  "AAABGAYY" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABKWQO" varchar(30) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABGAYS" varchar(8) NOT NULL default '00000000',
  "AAABGAYT" varchar(6) NOT NULL default '000000',
  "AAABGAYU" varchar(12) NOT NULL default ' ',
  "AAABGAYV" varchar(32) NOT NULL default ' ',
  "AAABINMN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKWV","AAABGAYY"),
  KEY "AAABGAXW" ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAXY" (
  "AAABLKWV" varchar(8) NOT NULL default '00000000',
  "AAABGAYY" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABKWQO" varchar(30) NOT NULL default ' ',
  "AAABLUUW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(100) NOT NULL default ' ',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABGAYS" varchar(8) NOT NULL default '00000000',
  "AAABGAYT" varchar(6) NOT NULL default '000000',
  "AAABGAYU" varchar(12) NOT NULL default ' ',
  "AAABGAYV" varchar(32) NOT NULL default ' ',
  "AAABINMN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKWV","AAABGAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAYZ" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAZE" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBM" varchar(6) NOT NULL default '000000',
  "AAABGBBJ" smallint(6) NOT NULL default '0',
  "AAABMMRH" text,
  PRIMARY KEY ("AAABGBZZ","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAZL" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBM" varchar(6) NOT NULL default '000000',
  "AAABMMJT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGBZZ","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGAZR" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABGAZW" varchar(10) NOT NULL default ' ',
  "AAABGHGV" varchar(10) NOT NULL default ' ',
  "AAABINMN" varchar(3) NOT NULL default ' ',
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABHFCH" varchar(30) NOT NULL default ' ',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBAD" (
  "AAABGBAG" varchar(10) NOT NULL default ' ',
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABGBAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBAH" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBM" varchar(6) NOT NULL default '000000',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABGBAL" varchar(10) NOT NULL default ' ',
  "AAABGBAM" varchar(6) NOT NULL default '000000',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABGBAO" varchar(4) NOT NULL default '0000',
  "AAABGBAP" varchar(1) NOT NULL default ' ',
  "AAABGBAQ" varchar(4) NOT NULL default '0000',
  "AAABGBAR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABGBZZ","AAABGBBM","AAABGBAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBAV" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBM" varchar(6) NOT NULL default '000000',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABGBAZ" int(11) NOT NULL default '0',
  "AAABGBBA" int(11) NOT NULL default '0',
  "AAABGBBB" int(11) NOT NULL default '0',
  "AAABGBBC" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGBZZ","AAABGBBM","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBBG" (
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBM" varchar(6) NOT NULL default '000000',
  "AAABGBBJ" smallint(6) NOT NULL default '0',
  "AAABMMRH" blob,
  PRIMARY KEY ("AAABGBZZ","AAABGBBM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBBN" (
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGKYU" varchar(3) NOT NULL default ' ',
  "AAABGBCN" varchar(1) NOT NULL default '0',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABLPQZ" varchar(10) NOT NULL default '0000000000',
  "AAABGBBU" varchar(1) NOT NULL default ' ',
  "AAABGBBV" varchar(1) NOT NULL default ' ',
  "AAABGBBW" varchar(1) NOT NULL default ' ',
  "AAABGBBX" varchar(1) NOT NULL default ' ',
  "AAABGBBY" varchar(1) NOT NULL default ' ',
  "AAABGBBZ" varchar(1) NOT NULL default ' ',
  "AAABGBCA" varchar(1) NOT NULL default ' ',
  "AAABGKYJ" varchar(8) NOT NULL default ' ',
  "AAABGBCC" varchar(4) NOT NULL default ' ',
  "AAABGBCD" varchar(73) NOT NULL default ' ',
  "AAABGBVR" varchar(32) NOT NULL default ' ',
  "AAABGBCF" varchar(25) NOT NULL default ' ',
  "AAABGBCG" varchar(10) NOT NULL default '0000000000',
  "AAABGBCH" varchar(10) NOT NULL default '0000000000',
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGCAA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAR","AAABHFAQ","AAABGKYU","AAABGBCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBCO" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGBIW" varchar(10) NOT NULL default ' ',
  "AAABGBCR" varchar(32) NOT NULL default ' ',
  "AAABGBCS" varchar(60) NOT NULL default ' ',
  "AAABGBCT" varchar(32) NOT NULL default ' ',
  "AAABKNBT" varchar(10) NOT NULL default ' ',
  "AAABGBCV" varchar(3) NOT NULL default ' ',
  "AAABHWUC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBCY" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBDS" varchar(6) NOT NULL default '000000',
  "AAABGBDT" varchar(1) NOT NULL default '0',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGQWW" varchar(20) NOT NULL default ' ',
  "AAABGBDF" varchar(1) NOT NULL default ' ',
  "AAABGBDG" varchar(4) NOT NULL default ' ',
  "AAABGKYU" varchar(3) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGBDJ" varchar(1) NOT NULL default ' ',
  "AAABGBDK" varchar(1) NOT NULL default ' ',
  "AAABGBDL" varchar(1) NOT NULL default ' ',
  "AAABGBDM" varchar(1) NOT NULL default ' ',
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGCAA" varchar(20) NOT NULL default ' ',
  "AAABGBDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABGBDS","AAABGBDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBDU" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABGBIW" varchar(10) NOT NULL default ' ',
  "AAABHEZD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBEB" (
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAABGBYY" varchar(1) NOT NULL default ' ',
  "AAABIGEF" varchar(1) NOT NULL default ' ',
  "AAABGBEF" varchar(1) NOT NULL default ' ',
  "AAABGBXR" varchar(100) NOT NULL default ' ',
  "AAABGBXS" varchar(2) NOT NULL default ' ',
  "AAABGBXY" varchar(3) NOT NULL default '000',
  "AAABKYPM" varchar(12) NOT NULL default ' ',
  "AAABGBYA" varchar(1) NOT NULL default ' ',
  "AAABGBYB" varchar(8) NOT NULL default ' ',
  "AAABGBYC" varchar(1) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABHYMW" varchar(100) NOT NULL default ' ',
  "AAABHYMX" varchar(32) NOT NULL default ' ',
  "AAABGBYK" varchar(1) NOT NULL default ' ',
  "AAABGBYL" varchar(255) NOT NULL default ' ',
  "AAABGBYM" varchar(1) NOT NULL default ' ',
  "AAABGBEW" varchar(40) NOT NULL default ' ',
  "AAABGBYN" varchar(1) NOT NULL default '0',
  "AAABGBEY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBXV","AAABGBYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBFB" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBII" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBFF" (
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGIFA" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBFM" (
  "AAABGBFS" varchar(6) NOT NULL default '000000',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABGBFP" varchar(30) NOT NULL default ' ',
  "AAABGBFQ" varchar(30) NOT NULL default ' ',
  "AAABGBFR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBFT" (
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBGC" varchar(10) NOT NULL default ' ',
  "AAABGBIQ" varchar(1) NOT NULL default ' ',
  "AAABKYPM" varchar(12) NOT NULL default ' ',
  "AAABGBYB" varchar(8) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHFAR","AAABGBGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBGD" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBIO" varchar(1) NOT NULL default ' ',
  "AAABGBPI" varchar(10) NOT NULL default ' ',
  "AAABGBPK" varchar(10) NOT NULL default ' ',
  "AAABGBOZ" varchar(60) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABGBGL" varchar(1) NOT NULL default ' ',
  "AAABGBGM" varchar(1) NOT NULL default ' ',
  "AAABGBPP" varchar(1) NOT NULL default ' ',
  "AAABGBYK" varchar(1) NOT NULL default ' ',
  "AAABGBGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBGR" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGBOZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBGX" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBPM" varchar(10) NOT NULL default ' ',
  "AAABGBIQ" varchar(1) NOT NULL default ' ',
  "AAABGBIS" varchar(1) NOT NULL default ' ',
  "AAABGBPN" varchar(60) NOT NULL default ' ',
  "AAABLGAD" varchar(1) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHFAQ","AAABGBPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBHI" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBPM" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGBPN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABGBPM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBHQ" (
  "AAABGBHW" varchar(3) NOT NULL default ' ',
  "AAABLELB" varchar(14) NOT NULL default ' ',
  "AAABGBHY" varchar(8) NOT NULL default ' ',
  "AAABGBHU" varchar(16) NOT NULL default ' ',
  "AAABGBHV" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBHW","AAABLELB","AAABGBHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBHZ" (
  "AAABGBIE" varchar(16) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGBIC" varchar(1) NOT NULL default ' ',
  "AAABGBID" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBIE","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBIG" (
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBII" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBIK" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBYY" varchar(1) NOT NULL default ' ',
  "AAABGBIO" varchar(1) NOT NULL default ' ',
  "AAABGBIP" varchar(1) NOT NULL default ' ',
  "AAABGBIQ" varchar(1) NOT NULL default ' ',
  "AAABGBIR" varchar(1) NOT NULL default ' ',
  "AAABGBIS" varchar(1) NOT NULL default ' ',
  "AAABGBIT" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABGBPM" varchar(10) NOT NULL default ' ',
  "AAABGBIW" varchar(10) NOT NULL default ' ',
  "AAABGBIX" varchar(60) NOT NULL default ' ',
  "AAABGBIY" varchar(32) NOT NULL default ' ',
  "AAABGBIZ" varchar(32) NOT NULL default ' ',
  "AAABGBJA" varchar(1) NOT NULL default ' ',
  "AAABGBJB" varchar(1) NOT NULL default ' ',
  "AAABGBYV" varchar(60) NOT NULL default ' ',
  "AAABGBJD" varchar(10) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABGBJF" varchar(10) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABGBYG" varchar(12) NOT NULL default ' ',
  "AAABGBKV" varchar(20) NOT NULL default ' ',
  "AAABGBJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABGBYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBJO" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGBYY" varchar(1) NOT NULL default ' ',
  "AAABGBYV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABMRHS","AAABGBYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBJY" (
  "AAABIGEG" varchar(10) NOT NULL default ' ',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLTPE" varchar(20) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABGBKF" varchar(6) NOT NULL default '000000',
  "AAABGBKG" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABINFD","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBKK" (
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABGBKQ" varchar(100) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLENH","AAABGBKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBKR" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGBKV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBKZ" (
  "AAABGHHC" varchar(10) NOT NULL default ' ',
  "AAABGBLB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBLD" (
  "AAABGHHC" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABGHHC","AAABHFAR","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAABGBLY" varchar(30) NOT NULL default ' ',
  "AAABGBMF" varchar(1) NOT NULL default ' ',
  "AAABGBMG" varchar(1) NOT NULL default ' ',
  "AAABGBMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHB","AAABIJDD","AAABLQTX","AAABGBLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAABGBMM" varchar(30) NOT NULL default ' ',
  "AAABGBMF" varchar(1) NOT NULL default ' ',
  "AAABGBMG" varchar(1) NOT NULL default ' ',
  "AAABGBMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMHB","AAABIJDD","AAABLQTX","AAABGBMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLXO" varchar(1) NOT NULL default ' ',
  "AAABGBOU" varchar(3) NOT NULL default ' ',
  "AAABGBMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLLXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLXO" varchar(1) NOT NULL default ' ',
  "AAABGBMY" varchar(40) NOT NULL default ' ',
  "AAABGBMZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLLXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBOU" varchar(3) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAABGBNJ" varchar(1) NOT NULL default ' ',
  "AAABGBNK" varchar(1) NOT NULL default ' ',
  "AAABGBNL" varchar(1) NOT NULL default ' ',
  "AAABGBNM" varchar(1) NOT NULL default ' ',
  "AAABGBNN" varchar(1) NOT NULL default ' ',
  "AAABGBNO" varchar(1) NOT NULL default ' ',
  "AAABGBNP" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABGBNR" varchar(1) NOT NULL default ' ',
  "AAABGBNS" varchar(1) NOT NULL default ' ',
  "AAABGBNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGBOU" varchar(3) NOT NULL default ' ',
  "AAABGBOA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGBOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBOU" varchar(3) NOT NULL default ' ',
  "AAABGBOV" varchar(10) NOT NULL default ' ',
  "AAABGBOW" varchar(30) NOT NULL default ' ',
  "AAABGBOJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBOU","AAABGBOV","AAABGBOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBOU" varchar(3) NOT NULL default ' ',
  "AAABGBOV" varchar(10) NOT NULL default ' ',
  "AAABGBOW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBOU","AAABGBOV","AAABGBOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBOX" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBOZ" varchar(60) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBYV" varchar(60) NOT NULL default ' ',
  "AAABGBPC" varchar(12) NOT NULL default ' ',
  "AAABGBPD" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABGBYG" varchar(12) NOT NULL default ' ',
  "AAABGBPI" varchar(10) NOT NULL default ' ',
  "AAABGBPJ" varchar(60) NOT NULL default ' ',
  "AAABGBPK" varchar(10) NOT NULL default ' ',
  "AAABGBPL" varchar(60) NOT NULL default ' ',
  "AAABGBPM" varchar(10) NOT NULL default ' ',
  "AAABGBPN" varchar(60) NOT NULL default ' ',
  "AAABGBPO" varchar(1) NOT NULL default ' ',
  "AAABGBPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBPR" (
  "AAABHFCH" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABJAOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFCH","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBPY" (
  "AAABHFCH" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABJAOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFCH","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBQF" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABLOSZ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAABMMJT" varchar(200) NOT NULL default ' ',
  "AAABLFWB" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(132) NOT NULL default ' ',
  "AAABGBQP" varchar(1) NOT NULL default ' ',
  "AAABGBQQ" varchar(1) NOT NULL default ' ',
  "AAABGBQR" varchar(1) NOT NULL default ' ',
  "AAABGBQS" varchar(1) NOT NULL default ' ',
  "AAABGBQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABLTXI","AAABINFD","AAABHMQN","AAABLOSZ","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBRA" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBRE" varchar(30) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABGBYG" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBRK" (
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABGBRY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLGEF","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBRQ" (
  "AAABGBRY" varchar(6) NOT NULL default '000000',
  "AAABKMWB" varchar(6) NOT NULL default '000000',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABGBRU" varchar(40) NOT NULL default ' ',
  "AAABHKDA" varchar(40) NOT NULL default ' ',
  "AAABGBRW" varchar(40) NOT NULL default ' ',
  "AAABGBRX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGBRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBRZ" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLTLR" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(5) NOT NULL default ' ',
  "AAABGYNX" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBSI" varchar(8) NOT NULL default '00000000',
  "AAABGBSJ" varchar(6) NOT NULL default '000000',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGBSL" varchar(8) NOT NULL default '00000000',
  "AAABGBSM" varchar(6) NOT NULL default '000000',
  "AAABGBSN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABLTXI","AAABHFAR","AAABLUXQ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBST" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLTLR" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABLTXI","AAABHFAR","AAABLUXQ","AAABLTLR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBTH" (
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABGBTN" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGBTL","AAABIZCH","AAABGBTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBTO" (
  "AAABIGEG" varchar(10) NOT NULL default ' ',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABINFD","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBTV" (
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABLFTU" varchar(1) NOT NULL default ' ',
  "AAABGBVR" varchar(32) NOT NULL default ' ',
  "AAABGBUA" varchar(8) NOT NULL default '00000000',
  "AAABGBUB" varchar(6) NOT NULL default '000000',
  "AAABGBUC" varchar(8) NOT NULL default '00000000',
  "AAABGBUD" varchar(6) NOT NULL default '000000',
  "AAABGBUE" varchar(8) NOT NULL default '00000000',
  "AAABGBUF" varchar(6) NOT NULL default '000000',
  "AAABGBUG" varchar(12) NOT NULL default ' ',
  "AAABGBUH" varchar(8) NOT NULL default '00000000',
  "AAABGBUI" varchar(6) NOT NULL default '000000',
  "AAABGBUJ" int(11) NOT NULL default '0',
  "AAABGBUK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABHFAR","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBUN" (
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABGBUT" varchar(60) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABGBUT","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBUV" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGCBE" varchar(60) NOT NULL default ' ',
  "AAABGBVF" varchar(12) NOT NULL default ' ',
  "AAABGBVG" varchar(8) NOT NULL default '00000000',
  "AAABGBVH" varchar(6) NOT NULL default '000000',
  "AAABGBVI" varchar(12) NOT NULL default ' ',
  "AAABGBVJ" varchar(8) NOT NULL default '00000000',
  "AAABGBVK" varchar(6) NOT NULL default '000000',
  "AAABGBWH" varchar(12) NOT NULL default ' ',
  "AAABGBWI" varchar(8) NOT NULL default '00000000',
  "AAABGBWJ" varchar(6) NOT NULL default '000000',
  "AAABGBVO" varchar(1) NOT NULL default ' ',
  "AAABGBVP" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABGBVR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABILWI","AAABLUXQ","AAABIITH","AAABIITI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBVY" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLTLR" varchar(20) NOT NULL default ' ',
  "AAABGBWG" varchar(1) NOT NULL default ' ',
  "AAABGBWH" varchar(12) NOT NULL default ' ',
  "AAABGBWI" varchar(8) NOT NULL default '00000000',
  "AAABGBWJ" varchar(6) NOT NULL default '000000',
  "AAABGBWK" varchar(1) NOT NULL default ' ',
  "AAABMPMT" varchar(1) NOT NULL default ' ',
  "AAABIFVP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABILWI","AAABLUXQ","AAABIITH","AAABIITI","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBWU" (
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLTLR" varchar(20) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAQ","AAABHFAR","AAABILWI","AAABLUXQ","AAABIITH","AAABIITI","AAABLTLR","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBXM" (
  "AAABLITM" varchar(10) NOT NULL default ' ',
  "AAABGBYY" varchar(1) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABHFAR" varchar(10) NOT NULL default ' ',
  "AAABGBXR" varchar(100) NOT NULL default ' ',
  "AAABGBXS" varchar(2) NOT NULL default ' ',
  "AAABGBXT" varchar(64) NOT NULL default ' ',
  "AAABGBXU" varchar(1) NOT NULL default ' ',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAABGBXW" varchar(1) NOT NULL default ' ',
  "AAABGBXX" varchar(1) NOT NULL default ' ',
  "AAABGBXY" varchar(3) NOT NULL default '000',
  "AAABKYPM" varchar(12) NOT NULL default ' ',
  "AAABGBYA" varchar(1) NOT NULL default ' ',
  "AAABGBYB" varchar(8) NOT NULL default ' ',
  "AAABGBYC" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBYE" varchar(8) NOT NULL default '00000000',
  "AAABGBYF" varchar(6) NOT NULL default '000000',
  "AAABGBYG" varchar(12) NOT NULL default ' ',
  "AAABHYMW" varchar(100) NOT NULL default ' ',
  "AAABHYMX" varchar(32) NOT NULL default ' ',
  "AAABGBYJ" varchar(1) NOT NULL default ' ',
  "AAABGBYK" varchar(1) NOT NULL default ' ',
  "AAABGBYL" varchar(255) NOT NULL default ' ',
  "AAABGBYM" varchar(1) NOT NULL default ' ',
  "AAABGBYN" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLITM","AAABGBYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBYQ" (
  "AAABLITM" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGBYY" varchar(1) NOT NULL default ' ',
  "AAABHFAQ" varchar(10) NOT NULL default ' ',
  "AAABGBYV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLITM","AAABMRHS","AAABGBYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBYZ" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBZH" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGKYU" varchar(3) NOT NULL default ' ',
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGCAA" varchar(20) NOT NULL default ' ',
  "AAABHFCH" varchar(20) NOT NULL default ' ',
  "AAABGCAC" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABMKFU" varchar(20) NOT NULL default ' ',
  "AAABGQWW" varchar(10) NOT NULL default ' ',
  "AAABGCAG" varchar(1) NOT NULL default ' ',
  "AAABGCAH" varchar(1) NOT NULL default ' ',
  "AAABGCAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABGKYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGBZW" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGKYU" varchar(3) NOT NULL default ' ',
  "AAABGBZZ" varchar(10) NOT NULL default '0000000000',
  "AAABGCAA" varchar(20) NOT NULL default ' ',
  "AAABHFCH" varchar(20) NOT NULL default ' ',
  "AAABGCAC" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABMKFU" varchar(20) NOT NULL default ' ',
  "AAABGQWW" varchar(10) NOT NULL default ' ',
  "AAABGCAG" varchar(1) NOT NULL default ' ',
  "AAABGCAH" varchar(1) NOT NULL default ' ',
  "AAABGCAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABGKYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCAL" (
  "AAABGCAP" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCAP","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCAR" (
  "AAABGHHC" varchar(40) NOT NULL default ' ',
  "AAABGHGT" varchar(14) NOT NULL default '00000000000000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGHGW" varchar(3) NOT NULL default ' ',
  "AAABGHGX" varchar(1) NOT NULL default ' ',
  "AAABGHGM" varchar(1) NOT NULL default ' ',
  "AAABGXDK" varchar(1) NOT NULL default ' ',
  "AAABGCAZ" varchar(4) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGHGO" varchar(12) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABKWUO" varchar(12) NOT NULL default ' ',
  "AAABGCBE" varchar(60) NOT NULL default ' ',
  "AAABLJKC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHHC","AAABGHGT","AAABILWI","AAABGHGW","AAABGHGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCBL" (
  "AAABGCBP" varchar(12) NOT NULL default ' ',
  "AAABGCBQ" varchar(10) NOT NULL default ' ',
  "AAABGCBO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCBP","AAABGCBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABGCCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMAMX","AAABGCCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABGCCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMAMX","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABGCCV" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABMAMX","AAABGCCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABGCCZ" varchar(1) NOT NULL default ' ',
  "AAABGCDA" varchar(1) NOT NULL default ' ',
  "AAABGCDB" decimal(3,0) NOT NULL default '0',
  "AAABGCDC" varchar(1) NOT NULL default ' ',
  "AAABGCDD" varchar(1) NOT NULL default ' ',
  "AAABGCDE" varchar(1) NOT NULL default '0',
  "AAABGCDF" varchar(1) NOT NULL default '0',
  "AAABGCDG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXRN" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABIFNV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCT","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABGCED" varchar(1) NOT NULL default ' ',
  "AAABGCEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLURA","AAABLXOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGCEU" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABGCEP" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXOB","AAABMCVI","AAABGCEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABGCFE" varchar(3) NOT NULL default '000',
  "AAABLRRK" varchar(20) NOT NULL default ' ',
  "AAABLRRL" varchar(30) NOT NULL default ' ',
  "AAABLRRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR","AAABGCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGCFR" varchar(20) NOT NULL default ' ',
  "AAABGCFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGCFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCGB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT","AAABGCJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJJ" varchar(3) NOT NULL default ' ',
  "AAABGCGR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCJT","AAABGCJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJU" varchar(4) NOT NULL default ' ',
  "AAABGCJL" varchar(5) NOT NULL default '00000',
  "AAABLRXL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABGCHF" varchar(2) NOT NULL default '00',
  "AAABLGKR" varchar(2) NOT NULL default ' ',
  "AAABGCHH" varchar(1) NOT NULL default ' ',
  "AAABGCHI" varchar(2) NOT NULL default ' ',
  "AAABGCHJ" varchar(2) NOT NULL default '00',
  "AAABGCHK" varchar(4) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABGCHM" varchar(6) NOT NULL default '000000',
  "AAABGCHN" varchar(6) NOT NULL default '000000',
  "AAABGCHO" varchar(1) NOT NULL default ' ',
  "AAABGCHP" varchar(2) NOT NULL default ' ',
  "AAABGCHQ" varchar(2) NOT NULL default '00',
  "AAABGCHR" varchar(8) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGCHY" varchar(1) NOT NULL default ' ',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABGCIA" varchar(4) NOT NULL default ' ',
  "AAABGCIB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT","AAABGCJU","AAABGCJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJU" varchar(4) NOT NULL default ' ',
  "AAABGCIL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCJT","AAABGCJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJU" varchar(4) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABGCIV" varchar(1) NOT NULL default ' ',
  "AAABGCIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT","AAABGCJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJJ" varchar(3) NOT NULL default ' ',
  "AAABGCJU" varchar(4) NOT NULL default ' ',
  "AAABGCJL" varchar(5) NOT NULL default '00000',
  "AAABGCJG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT","AAABGCJJ","AAABGCJU","AAABGCJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCJT" varchar(10) NOT NULL default ' ',
  "AAABGCJU" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCJT","AAABGCJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCJV" (
  "AAABMQPU" varchar(2) NOT NULL default ' ',
  "AAABGCJX" varchar(2) NOT NULL default ' ',
  "AAABGCJY" varchar(1) NOT NULL default ' ',
  "AAABGCJZ" varchar(1) NOT NULL default ' ',
  "AAABGCKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(6) NOT NULL default ' ',
  "AAABMQPU" varchar(2) NOT NULL default ' ',
  "AAABGCKG" varchar(1) NOT NULL default ' ',
  "AAABGCKQ" varchar(3) NOT NULL default ' ',
  "AAABGCKI" varchar(4) NOT NULL default '0000',
  "AAABGCLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCKS" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABGCKQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGCKW" varchar(1) NOT NULL default ' ',
  "AAABGCKX" varchar(1) NOT NULL default ' ',
  "AAABGCKY" varchar(1) NOT NULL default ' ',
  "AAABGCKZ" varchar(1) NOT NULL default ' ',
  "AAABGCLA" varchar(1) NOT NULL default ' ',
  "AAABGCLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEWW" varchar(4) NOT NULL default ' ',
  "AAABHEWX" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(6) NOT NULL default ' ',
  "AAABGCLJ" varchar(1) NOT NULL default ' ',
  "AAABGCLK" varchar(1) NOT NULL default ' ',
  "AAABGCLL" varchar(1) NOT NULL default ' ',
  "AAABGCLM" varchar(3) NOT NULL default ' ',
  "AAABGCLN" varchar(8) NOT NULL default '00000000',
  "AAABGCLO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHEWW","AAABHEWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABGCLV" int(11) NOT NULL default '0',
  "AAABGCLW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCMM" varchar(10) NOT NULL default ' ',
  "AAABGCMC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCML" varchar(1) NOT NULL default ' ',
  "AAABGCMM" varchar(10) NOT NULL default ' ',
  "AAABGCMJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCML","AAABGCMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCPY" varchar(8) NOT NULL default ' ',
  "AAABGCOJ" varchar(10) NOT NULL default ' ',
  "AAABGCOK" varchar(1) NOT NULL default ' ',
  "AAABGCOL" varchar(10) NOT NULL default ' ',
  "AAABGCOM" varchar(1) NOT NULL default ' ',
  "AAABGCON" varchar(10) NOT NULL default ' ',
  "AAABGCOO" varchar(1) NOT NULL default ' ',
  "AAABGCOP" varchar(10) NOT NULL default ' ',
  "AAABGCOQ" varchar(1) NOT NULL default ' ',
  "AAABGCOR" varchar(10) NOT NULL default ' ',
  "AAABGCOS" varchar(1) NOT NULL default ' ',
  "AAABGCOT" varchar(10) NOT NULL default ' ',
  "AAABGCOU" varchar(1) NOT NULL default ' ',
  "AAABGCOV" varchar(10) NOT NULL default ' ',
  "AAABGCOW" varchar(1) NOT NULL default ' ',
  "AAABGCOX" varchar(10) NOT NULL default ' ',
  "AAABGCOY" varchar(1) NOT NULL default ' ',
  "AAABGCOZ" varchar(10) NOT NULL default ' ',
  "AAABGCPA" varchar(1) NOT NULL default ' ',
  "AAABGCPB" varchar(10) NOT NULL default ' ',
  "AAABGCPC" varchar(1) NOT NULL default ' ',
  "AAABGCPD" varchar(10) NOT NULL default ' ',
  "AAABGCPE" varchar(1) NOT NULL default ' ',
  "AAABGCPF" varchar(10) NOT NULL default ' ',
  "AAABGCPG" varchar(1) NOT NULL default ' ',
  "AAABGCPH" varchar(10) NOT NULL default ' ',
  "AAABGCPI" varchar(1) NOT NULL default ' ',
  "AAABGCPJ" varchar(10) NOT NULL default ' ',
  "AAABGCPK" varchar(1) NOT NULL default ' ',
  "AAABGCPL" varchar(10) NOT NULL default ' ',
  "AAABGCPM" varchar(1) NOT NULL default ' ',
  "AAABGCPN" varchar(10) NOT NULL default ' ',
  "AAABGCPO" varchar(1) NOT NULL default ' ',
  "AAABGCPP" varchar(10) NOT NULL default ' ',
  "AAABGCPQ" varchar(1) NOT NULL default ' ',
  "AAABGCPR" varchar(10) NOT NULL default ' ',
  "AAABGCPS" varchar(1) NOT NULL default ' ',
  "AAABGCPT" varchar(10) NOT NULL default ' ',
  "AAABGCPU" varchar(1) NOT NULL default ' ',
  "AAABGCPV" varchar(10) NOT NULL default ' ',
  "AAABGCPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCPY" varchar(8) NOT NULL default ' ',
  "AAABGCOJ" varchar(10) NOT NULL default ' ',
  "AAABGCOK" varchar(1) NOT NULL default ' ',
  "AAABGCOL" varchar(10) NOT NULL default ' ',
  "AAABGCOM" varchar(1) NOT NULL default ' ',
  "AAABGCON" varchar(10) NOT NULL default ' ',
  "AAABGCOO" varchar(1) NOT NULL default ' ',
  "AAABGCOP" varchar(10) NOT NULL default ' ',
  "AAABGCOQ" varchar(1) NOT NULL default ' ',
  "AAABGCOR" varchar(10) NOT NULL default ' ',
  "AAABGCOS" varchar(1) NOT NULL default ' ',
  "AAABGCOT" varchar(10) NOT NULL default ' ',
  "AAABGCOU" varchar(1) NOT NULL default ' ',
  "AAABGCOV" varchar(10) NOT NULL default ' ',
  "AAABGCOW" varchar(1) NOT NULL default ' ',
  "AAABGCOX" varchar(10) NOT NULL default ' ',
  "AAABGCOY" varchar(1) NOT NULL default ' ',
  "AAABGCOZ" varchar(10) NOT NULL default ' ',
  "AAABGCPA" varchar(1) NOT NULL default ' ',
  "AAABGCPB" varchar(10) NOT NULL default ' ',
  "AAABGCPC" varchar(1) NOT NULL default ' ',
  "AAABGCPD" varchar(10) NOT NULL default ' ',
  "AAABGCPE" varchar(1) NOT NULL default ' ',
  "AAABGCPF" varchar(10) NOT NULL default ' ',
  "AAABGCPG" varchar(1) NOT NULL default ' ',
  "AAABGCPH" varchar(10) NOT NULL default ' ',
  "AAABGCPI" varchar(1) NOT NULL default ' ',
  "AAABGCPJ" varchar(10) NOT NULL default ' ',
  "AAABGCPK" varchar(1) NOT NULL default ' ',
  "AAABGCPL" varchar(10) NOT NULL default ' ',
  "AAABGCPM" varchar(1) NOT NULL default ' ',
  "AAABGCPN" varchar(10) NOT NULL default ' ',
  "AAABGCPO" varchar(1) NOT NULL default ' ',
  "AAABGCPP" varchar(10) NOT NULL default ' ',
  "AAABGCPQ" varchar(1) NOT NULL default ' ',
  "AAABGCPR" varchar(10) NOT NULL default ' ',
  "AAABGCPS" varchar(1) NOT NULL default ' ',
  "AAABGCPT" varchar(10) NOT NULL default ' ',
  "AAABGCPU" varchar(1) NOT NULL default ' ',
  "AAABGCPV" varchar(10) NOT NULL default ' ',
  "AAABGCPW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLIXQ" varchar(4) NOT NULL default ' ',
  "AAABLUEZ" varchar(1) NOT NULL default ' ',
  "AAABLIXS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLZZR" varchar(4) NOT NULL default ' ',
  "AAABGCQG" varchar(4) NOT NULL default ' ',
  "AAABGCQH" varchar(4) NOT NULL default ' ',
  "AAABGCQI" varchar(4) NOT NULL default ' ',
  "AAABGCQJ" varchar(2) NOT NULL default ' ',
  "AAABGCQK" varchar(1) NOT NULL default ' ',
  "AAABGCQL" varchar(1) NOT NULL default ' ',
  "AAABGCQM" varchar(1) NOT NULL default ' ',
  "AAABGCQN" varchar(1) NOT NULL default ' ',
  "AAABGCQO" varchar(1) NOT NULL default ' ',
  "AAABGCQP" varchar(1) NOT NULL default ' ',
  "AAABGCQQ" varchar(1) NOT NULL default ' ',
  "AAABGCQR" varchar(1) NOT NULL default ' ',
  "AAABGCQS" varchar(1) NOT NULL default ' ',
  "AAABGCQT" varchar(1) NOT NULL default ' ',
  "AAABGCQU" varchar(1) NOT NULL default ' ',
  "AAABGCQV" varchar(1) NOT NULL default ' ',
  "AAABGCQW" varchar(1) NOT NULL default ' ',
  "AAABGCQX" varchar(1) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABGCQZ" varchar(4) NOT NULL default ' ',
  "AAABLLDG" varchar(4) NOT NULL default ' ',
  "AAABGCRB" varchar(4) NOT NULL default ' ',
  "AAABGCRC" varchar(1) NOT NULL default ' ',
  "AAABGCRD" varchar(4) NOT NULL default ' ',
  "AAABGCRE" varchar(4) NOT NULL default ' ',
  "AAABGCRF" varchar(1) NOT NULL default ' ',
  "AAABGCRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLIXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLIXQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLIXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCEO" varchar(4) NOT NULL default ' ',
  "AAABGCSU" varchar(8) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAABGCTK" varchar(6) NOT NULL default ' ',
  "AAABGCTL" varchar(4) NOT NULL default ' ',
  "AAABGCTM" varchar(12) NOT NULL default ' ',
  "AAABGCTN" varchar(1) NOT NULL default ' ',
  "AAABGCTO" varchar(1) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABGCTR" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMCVI","AAABMCEO","AAABGCSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCSV" (
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKZKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCSY" (
  "AAABKZKJ" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABKZKJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGCTJ" varchar(4) NOT NULL default ' ',
  "AAABGCTK" varchar(6) NOT NULL default ' ',
  "AAABGCTL" varchar(4) NOT NULL default ' ',
  "AAABGCTM" varchar(12) NOT NULL default ' ',
  "AAABGCTN" varchar(1) NOT NULL default ' ',
  "AAABGCTO" varchar(1) NOT NULL default ' ',
  "AAABMEEQ" varchar(12) NOT NULL default ' ',
  "AAABMEER" varchar(12) NOT NULL default ' ',
  "AAABGCTR" varchar(1) NOT NULL default ' ',
  "AAABMEES" varchar(68) NOT NULL default ' ',
  "AAABMEET" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCUH" varchar(50) NOT NULL default ' ',
  "AAABHAZI" varchar(8) NOT NULL default ' ',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABGCUL" varchar(132) NOT NULL default ' ',
  "AAABLRQQ" varchar(1) NOT NULL default ' ',
  "AAABIKRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCUH","AAABHAZI","AAABHAZS"),
  KEY "AAABGCUK" ("AAABGCUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCUM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAABGCUQ" varchar(10) NOT NULL default ' ',
  "AAABGCUR" varchar(20) NOT NULL default ' ',
  "AAABGCUS" varchar(40) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGYQM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABGCVC" varchar(40) NOT NULL default ' ',
  "AAABGYSC" varchar(30) NOT NULL default ' ',
  "AAABGCVE" varchar(16) NOT NULL default ' ',
  "AAABGCVF" varchar(40) NOT NULL default ' ',
  "AAABGCVG" varchar(30) NOT NULL default ' ',
  "AAABGCVH" varchar(16) NOT NULL default ' ',
  "AAABGCVI" varchar(40) NOT NULL default ' ',
  "AAABGCVJ" varchar(30) NOT NULL default ' ',
  "AAABGCVK" varchar(16) NOT NULL default ' ',
  "AAABGCVL" varchar(40) NOT NULL default ' ',
  "AAABGCVM" varchar(30) NOT NULL default ' ',
  "AAABGCVN" varchar(16) NOT NULL default ' ',
  "AAABGCVO" varchar(40) NOT NULL default ' ',
  "AAABGCVP" varchar(30) NOT NULL default ' ',
  "AAABGCVQ" varchar(16) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABGCVS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQIJ","AAABMEKY","AAABMAMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCVX" (
  "AAABGCWB" varchar(13) NOT NULL default ' ',
  "AAABGCVZ" varchar(13) NOT NULL default ' ',
  "AAABGCWA" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCWC" (
  "AAABLUVJ" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABGCWF" VARBINARY(255),
  "AAABGCWG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCXP" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABGCWV" varchar(3) NOT NULL default '000',
  "AAABGCXN" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABGCXP","AAABMREX","AAABMCVI","AAABMLCT","AAABGCWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABGCXF" varchar(3) NOT NULL default '000',
  "AAABGCXZ" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMLCT","AAABGCXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCXP" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABGCYD" varchar(2) NOT NULL default '00',
  "AAABGCXN" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABGCXP","AAABMREX","AAABMCVI","AAABMLCT","AAABGCYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABGCYD" varchar(2) NOT NULL default '00',
  "AAABGCXZ" decimal(4,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABMLCT","AAABGCYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCYL" varchar(3) NOT NULL default '000',
  "AAABGCYH" decimal(5,2) NOT NULL default '0.00',
  "AAABGCYI" decimal(5,2) NOT NULL default '0.00',
  "AAABGCYJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGCYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCYS" varchar(3) NOT NULL default '000',
  "AAABGCYP" decimal(5,2) NOT NULL default '0.00',
  "AAABGCYQ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGCYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABGCYW" decimal(5,2) NOT NULL default '0.00',
  "AAABGCYX" decimal(5,2) NOT NULL default '0.00',
  "AAABGCYY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCZN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGCZN" varchar(2) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGCZN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGCZO" (
  "AAABGCZX" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGCZZ" varchar(1) NOT NULL default ' ',
  "AAABGDAA" varchar(1) NOT NULL default ' ',
  "AAABGDAB" varchar(30) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABGCZW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABGCZX","AAABMREL","AAABGCZZ","AAABGDAA","AAABGDAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDAC" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABKBHF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABKBHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDAH" (
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
  KEY "AAABGDBX" ("AAABJAAI"),
  KEY "AAABGDBZ" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDCC" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABGDCF" ("AAABHZOJ"),
  KEY "AAABGDCH" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDCK" (
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
  KEY "AAABGDDG" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDDJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGDDX" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDEA" (
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
  KEY "AAABGDFQ" ("AAABJAAI"),
  KEY "AAABGDFS" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDFV" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABGDFY" ("AAABHZOJ"),
  KEY "AAABGDGA" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDGD" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABGDGH" varchar(20) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  "AAABGDGJ" varchar(3) NOT NULL default ' ',
  "AAABGDGK" varchar(3) NOT NULL default ' ',
  "AAABGDGL" varchar(4) NOT NULL default ' ',
  "AAABGDGM" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  "AAABGDGP" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABGDGS" varchar(1) NOT NULL default ' ',
  "AAABGDHN" varchar(20) NOT NULL default ' ',
  "AAABGDGU" smallint(6) NOT NULL default '0',
  "AAABGDGV" smallint(6) NOT NULL default '0',
  "AAABGDGW" smallint(6) NOT NULL default '0',
  "AAABGDGX" smallint(6) NOT NULL default '0',
  "AAABGDGY" varchar(3) NOT NULL default ' ',
  "AAABGDGZ" varchar(3) NOT NULL default ' ',
  "AAABGDHA" varchar(3) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABGDHE" varchar(1) NOT NULL default ' ',
  "AAABGDHF" varchar(1) NOT NULL default ' ',
  "AAABGDHG" varchar(30) NOT NULL default ' ',
  "AAABGDHH" varchar(30) NOT NULL default ' ',
  "AAABGDHI" varchar(30) NOT NULL default ' ',
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABGDHK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV"),
  KEY "AAABGDHM" ("AAABGDHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDHO" (
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
  KEY "AAABGDIK" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDIN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGDJB" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDJE" ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDJG" (
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  "AAABGDLL" varchar(1) NOT NULL default ' ',
  "AAABGDJR" varchar(3) NOT NULL default ' ',
  "AAABGDJS" varchar(3) NOT NULL default ' ',
  "AAABGDJT" varchar(35) NOT NULL default ' ',
  "AAABGDJU" varchar(1) NOT NULL default ' ',
  "AAABGDLN" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(32) NOT NULL default ' ',
  "AAABGDJY" varchar(1) NOT NULL default '0',
  "AAABHDYQ" decimal(10,3) NOT NULL default '0.000',
  "AAABHDYR" varchar(3) NOT NULL default ' ',
  "AAABGDKB" varchar(30) NOT NULL default ' ',
  "AAABGDKC" varchar(1) NOT NULL default ' ',
  "AAABGDKD" varchar(255) NOT NULL default ' ',
  "AAABGDKE" varchar(10) NOT NULL default ' ',
  "AAABGDKF" varchar(30) NOT NULL default ' ',
  "AAABGDKG" varchar(24) NOT NULL default ' ',
  "AAABGDKH" varchar(24) NOT NULL default ' ',
  "AAABGDKI" varchar(1) NOT NULL default ' ',
  "AAABGDKJ" varchar(1) NOT NULL default ' ',
  "AAABGDKK" varchar(1) NOT NULL default ' ',
  "AAABGDKL" varchar(1) NOT NULL default ' ',
  "AAABGDKM" varchar(1) NOT NULL default ' ',
  "AAABGDKN" varchar(1) NOT NULL default ' ',
  "AAABGDKO" varchar(1) NOT NULL default ' ',
  "AAABGDKP" varchar(3) NOT NULL default ' ',
  "AAABGDKQ" varchar(1) NOT NULL default ' ',
  "AAABGDKR" varchar(10) NOT NULL default ' ',
  "AAABGDKS" varchar(1) NOT NULL default ' ',
  "AAABGDKT" varchar(10) NOT NULL default ' ',
  "AAABGDKU" varchar(1) NOT NULL default ' ',
  "AAABGDKV" varchar(10) NOT NULL default ' ',
  "AAABHRXB" varchar(4) NOT NULL default ' ',
  "AAABGDKX" varchar(4) NOT NULL default ' ',
  "AAABGDKY" varchar(4) NOT NULL default ' ',
  "AAABGDKZ" varchar(1) NOT NULL default ' ',
  "AAABGDLA" varchar(1) NOT NULL default ' ',
  "AAABGDLB" varchar(10) NOT NULL default ' ',
  "AAABGDLC" varchar(1) NOT NULL default ' ',
  "AAABGDLD" varchar(1) NOT NULL default ' ',
  "AAABGDLE" varchar(1) NOT NULL default ' ',
  "AAABGDLF" varchar(1) NOT NULL default ' ',
  "AAABGDLG" varchar(200) NOT NULL default ' ',
  "AAABJGGL" varchar(32) NOT NULL default ' ',
  "AAABHRVE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGDNS"),
  KEY "AAABGDLK" ("AAABGDLL"),
  KEY "AAABGDLM" ("AAABGDLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDLO" (
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAABGDLQ" varchar(1) NOT NULL default ' ',
  "AAABGDLR" varchar(1) NOT NULL default ' ',
  "AAABGDLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGDNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDLU" (
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAABLGEF" varchar(10) NOT NULL default ' ',
  "AAABGDMC" varchar(10) NOT NULL default '0000000000',
  "AAABGDMD" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGDNS","AAABLGEF","AAABGDMC","AAABGDMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDME" (
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAABGDMG" varchar(10) NOT NULL default ' ',
  "AAABGDMH" varchar(10) NOT NULL default ' ',
  "AAABGDMI" varchar(10) NOT NULL default ' ',
  "AAABGDMJ" varchar(10) NOT NULL default ' ',
  "AAABGDMK" varchar(10) NOT NULL default ' ',
  "AAABGDML" varchar(10) NOT NULL default ' ',
  "AAABGDMM" varchar(10) NOT NULL default ' ',
  "AAABGDMN" varchar(10) NOT NULL default ' ',
  "AAABGDMO" varchar(10) NOT NULL default ' ',
  "AAABGDMP" varchar(10) NOT NULL default ' ',
  "AAABGDMQ" varchar(10) NOT NULL default ' ',
  "AAABGDMR" varchar(10) NOT NULL default ' ',
  "AAABGDMS" varchar(10) NOT NULL default ' ',
  "AAABGDMT" varchar(10) NOT NULL default ' ',
  "AAABGDMU" varchar(10) NOT NULL default ' ',
  "AAABGDMV" varchar(10) NOT NULL default ' ',
  "AAABGDMW" varchar(10) NOT NULL default ' ',
  "AAABGDMX" varchar(10) NOT NULL default ' ',
  "AAABGDMY" varchar(10) NOT NULL default ' ',
  "AAABGDMZ" varchar(10) NOT NULL default ' ',
  "AAABGDNA" varchar(10) NOT NULL default ' ',
  "AAABGDNB" varchar(10) NOT NULL default ' ',
  "AAABGDNC" varchar(10) NOT NULL default ' ',
  "AAABGDND" varchar(10) NOT NULL default ' ',
  "AAABGDNE" varchar(10) NOT NULL default ' ',
  "AAABGDNF" varchar(10) NOT NULL default ' ',
  "AAABGDNG" varchar(10) NOT NULL default ' ',
  "AAABGDNH" varchar(10) NOT NULL default ' ',
  "AAABGDNI" varchar(10) NOT NULL default ' ',
  "AAABGDNJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGDNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDNL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGDNS" varchar(30) NOT NULL default ' ',
  "AAABGDNO" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGDNS"),
  KEY "AAABGDNR" ("AAABGDNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDNT" (
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABGDNV" tinyint(3) unsigned NOT NULL default '0',
  "AAABGDRF" varchar(4) NOT NULL default ' ',
  "AAABGDNX" varchar(4) NOT NULL default ' ',
  "AAABGDNY" varchar(4) NOT NULL default ' ',
  "AAABGDNZ" varchar(30) NOT NULL default ' ',
  "AAABGDOA" varchar(1) NOT NULL default ' ',
  "AAABGDOB" varchar(1) NOT NULL default ' ',
  "AAABGDOC" varchar(1) NOT NULL default ' ',
  "AAABGDOD" varchar(4) NOT NULL default ' ',
  "AAABGDOE" varchar(30) NOT NULL default ' ',
  "AAABGDOF" varchar(1) NOT NULL default ' ',
  "AAABGDOG" varchar(1) NOT NULL default ' ',
  "AAABGDOH" varchar(1) NOT NULL default ' ',
  "AAABGDOI" varchar(4) NOT NULL default ' ',
  "AAABGDOJ" varchar(30) NOT NULL default ' ',
  "AAABGDOK" varchar(1) NOT NULL default ' ',
  "AAABGDOL" varchar(1) NOT NULL default ' ',
  "AAABGDOM" varchar(1) NOT NULL default ' ',
  "AAABGDON" varchar(4) NOT NULL default ' ',
  "AAABGDOO" varchar(30) NOT NULL default ' ',
  "AAABGDOP" varchar(1) NOT NULL default ' ',
  "AAABGDOQ" varchar(1) NOT NULL default ' ',
  "AAABGDOR" varchar(1) NOT NULL default ' ',
  "AAABGDOS" varchar(4) NOT NULL default ' ',
  "AAABGDOT" varchar(30) NOT NULL default ' ',
  "AAABGDOU" varchar(1) NOT NULL default ' ',
  "AAABGDOV" varchar(1) NOT NULL default ' ',
  "AAABGDOW" varchar(1) NOT NULL default ' ',
  "AAABGDOX" varchar(4) NOT NULL default ' ',
  "AAABGDOY" varchar(30) NOT NULL default ' ',
  "AAABGDOZ" varchar(1) NOT NULL default ' ',
  "AAABGDPA" varchar(1) NOT NULL default ' ',
  "AAABGDPB" varchar(1) NOT NULL default ' ',
  "AAABGDPC" varchar(4) NOT NULL default ' ',
  "AAABGDPD" varchar(30) NOT NULL default ' ',
  "AAABGDPE" varchar(1) NOT NULL default ' ',
  "AAABGDPF" varchar(1) NOT NULL default ' ',
  "AAABGDPG" varchar(1) NOT NULL default ' ',
  "AAABGDPH" varchar(4) NOT NULL default ' ',
  "AAABGDPI" varchar(30) NOT NULL default ' ',
  "AAABGDPJ" varchar(1) NOT NULL default ' ',
  "AAABGDPK" varchar(1) NOT NULL default ' ',
  "AAABGDPL" varchar(1) NOT NULL default ' ',
  "AAABGDPM" varchar(4) NOT NULL default ' ',
  "AAABGDPN" varchar(30) NOT NULL default ' ',
  "AAABGDPO" varchar(1) NOT NULL default ' ',
  "AAABGDPP" varchar(1) NOT NULL default ' ',
  "AAABGDPQ" varchar(1) NOT NULL default ' ',
  "AAABGDPR" varchar(1) NOT NULL default '0',
  "AAABGDPS" varchar(1) NOT NULL default '0',
  "AAABGDPT" varchar(1) NOT NULL default '0',
  "AAABGDPU" varchar(1) NOT NULL default '0',
  "AAABGDPV" varchar(1) NOT NULL default '0',
  "AAABGDPW" varchar(1) NOT NULL default '0',
  "AAABGDPX" varchar(1) NOT NULL default '0',
  "AAABGDPY" varchar(1) NOT NULL default '0',
  "AAABGDPZ" varchar(1) NOT NULL default '0',
  "AAABGDQA" varchar(2) NOT NULL default '00',
  "AAABGDQB" varchar(2) NOT NULL default '00',
  "AAABGDQC" varchar(2) NOT NULL default '00',
  "AAABGDQD" varchar(2) NOT NULL default '00',
  "AAABGDQE" varchar(2) NOT NULL default '00',
  "AAABGDQF" varchar(2) NOT NULL default '00',
  "AAABGDQG" varchar(2) NOT NULL default '00',
  "AAABGDQH" varchar(2) NOT NULL default '00',
  "AAABGDQI" varchar(2) NOT NULL default '00',
  "AAABGDQJ" varchar(1) NOT NULL default ' ',
  "AAABGDQK" varchar(1) NOT NULL default ' ',
  "AAABGDQL" varchar(1) NOT NULL default ' ',
  "AAABGDQM" varchar(1) NOT NULL default ' ',
  "AAABGDQN" varchar(1) NOT NULL default ' ',
  "AAABGDQO" varchar(1) NOT NULL default ' ',
  "AAABGDQP" varchar(1) NOT NULL default ' ',
  "AAABGDQQ" varchar(1) NOT NULL default ' ',
  "AAABGDQR" varchar(1) NOT NULL default ' ',
  "AAABGDQS" varchar(3) NOT NULL default ' ',
  "AAABGDQT" varchar(3) NOT NULL default ' ',
  "AAABGDQU" varchar(3) NOT NULL default ' ',
  "AAABGDQV" varchar(3) NOT NULL default ' ',
  "AAABGDQW" varchar(3) NOT NULL default ' ',
  "AAABGDQX" varchar(3) NOT NULL default ' ',
  "AAABGDQY" varchar(3) NOT NULL default ' ',
  "AAABGDQZ" varchar(3) NOT NULL default ' ',
  "AAABGDRA" varchar(3) NOT NULL default ' ',
  "AAABGDRB" varchar(1) NOT NULL default ' ',
  "AAABGDRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMUZ"),
  KEY "AAABGDRE" ("AAABGDRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDRG" (
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABHZPJ" varchar(4) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDRS" (
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
  KEY "AAABGDTI" ("AAABJAAI"),
  KEY "AAABGDTK" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDTN" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABGDTQ" ("AAABHZOJ"),
  KEY "AAABGDTS" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDTV" (
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
  KEY "AAABGDUR" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDUU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGDVI" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDVL" ("AAABLGEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDVN" (
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
  KEY "AAABGDXD" ("AAABJAAI"),
  KEY "AAABGDXF" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGDXI" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABGDXL" ("AAABHZOJ"),
  KEY "AAABGDXN" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDXQ" (
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
  KEY "AAABGDYM" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDYP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGDZD" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGDZG" (
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
  KEY "AAABGEAW" ("AAABJAAI"),
  KEY "AAABGEAY" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGEBB" ("AAABHZRB","AAABLFYJ"),
  KEY "AAABGEBE" ("AAABHZOJ"),
  KEY "AAABGEBG" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEBJ" (
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
  KEY "AAABGECF" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGECI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGECW" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGECZ" (
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
  KEY "AAABGEEP" ("AAABJAAI"),
  KEY "AAABGEER" ("AAABMRHK","AAABHZRB"),
  KEY "AAABGEEU" ("AAABLFYJ","AAABHZRB"),
  KEY "AAABGEEX" ("AAABHZOJ"),
  KEY "AAABGEEZ" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEFC" (
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
  KEY "AAABGEFY" ("AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEGB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB"),
  KEY "AAABGEGP" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEGS" (
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABJCXS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMUZ","AAABJCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEGX" (
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABGEHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMUZ","AAABGEHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEHC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLMUZ"),
  KEY "AAABGEHI" ("AAABLMUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEHK" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABLMUZ" varchar(4) NOT NULL default ' ',
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABHECM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLOZE","AAABLMUZ","AAABHFAO","AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEHU" (
  "AAABGEKW" varchar(32) NOT NULL default ' ',
  "AAABGEHW" varchar(2) NOT NULL default ' ',
  "AAABGEHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEHZ" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEIE" (
  "AAABGEJR" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEIH" (
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABGEIJ" varchar(1) NOT NULL default ' ',
  "AAABGEIK" varchar(1) NOT NULL default ' ',
  "AAABGEIL" varchar(1) NOT NULL default ' ',
  "AAABGEIM" varchar(1) NOT NULL default ' ',
  "AAABGEIN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEIP" (
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABHBWG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEKO","AAABHBWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEIU" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEIZ" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABGEJL" varchar(32) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(6) NOT NULL default '000000',
  "AAABGEJG" varchar(2) NOT NULL default ' ',
  "AAABGEJH" varchar(72) NOT NULL default ' ',
  "AAABGEKW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLECD","AAABGEJL","AAABGEKO","AAABMLQA","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEJP" (
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABGEJR" varchar(70) NOT NULL default ' ',
  "AAABLSBE" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEJU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGEKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEKA" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLRQO" varchar(32) NOT NULL default ' ',
  "AAABMKWZ" varchar(32) NOT NULL default ' ',
  "AAABGEKO" varchar(4) NOT NULL default ' ',
  "AAABHBMG" varchar(70) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABHZPY" varchar(10) NOT NULL default '0000000000',
  "AAABGEKI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABLRQO","AAABMKWZ"),
  KEY "AAABGEKM" ("AAABLUSV","AAABGEKO"),
  KEY "AAABGEKP" ("AAABLRQO"),
  KEY "AAABGEKR" ("AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEKT" (
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABHDWL" varchar(32) NOT NULL default ' ',
  "AAABGEKW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABHDWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEKZ" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABGELY" varchar(30) NOT NULL default ' ',
  "AAABGELZ" varchar(30) NOT NULL default ' ',
  "AAABGEMA" varchar(30) NOT NULL default ' ',
  "AAABGEMB" varchar(30) NOT NULL default ' ',
  "AAABGEMC" varchar(30) NOT NULL default ' ',
  "AAABGEMD" varchar(30) NOT NULL default ' ',
  "AAABGEME" varchar(30) NOT NULL default ' ',
  "AAABGEMF" varchar(1) NOT NULL default ' ',
  "AAABGEMG" varchar(30) NOT NULL default ' ',
  "AAABIKEM" decimal(3,1) NOT NULL default '0.0',
  "AAABLVJB" varchar(20) NOT NULL default ' ',
  "AAABGEMJ" varchar(1) NOT NULL default ' ',
  "AAABGEMK" varchar(30) NOT NULL default ' ',
  "AAABGEML" varchar(30) NOT NULL default ' ',
  "AAABGEMM" varchar(30) NOT NULL default ' ',
  "AAABGEMN" varchar(30) NOT NULL default ' ',
  "AAABGEMO" varchar(30) NOT NULL default ' ',
  "AAABIMAO" varchar(1) NOT NULL default ' ',
  "AAABGEMQ" varchar(8) NOT NULL default '00000000',
  "AAABGEMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGELW" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABGELY" varchar(30) NOT NULL default ' ',
  "AAABGELZ" varchar(30) NOT NULL default ' ',
  "AAABGEMA" varchar(30) NOT NULL default ' ',
  "AAABGEMB" varchar(30) NOT NULL default ' ',
  "AAABGEMC" varchar(30) NOT NULL default ' ',
  "AAABGEMD" varchar(30) NOT NULL default ' ',
  "AAABGEME" varchar(30) NOT NULL default ' ',
  "AAABGEMF" varchar(1) NOT NULL default ' ',
  "AAABGEMG" varchar(30) NOT NULL default ' ',
  "AAABIKEM" decimal(3,1) NOT NULL default '0.0',
  "AAABLVJB" varchar(20) NOT NULL default ' ',
  "AAABGEMJ" varchar(1) NOT NULL default ' ',
  "AAABGEMK" varchar(30) NOT NULL default ' ',
  "AAABGEML" varchar(30) NOT NULL default ' ',
  "AAABGEMM" varchar(30) NOT NULL default ' ',
  "AAABGEMN" varchar(30) NOT NULL default ' ',
  "AAABGEMO" varchar(30) NOT NULL default ' ',
  "AAABIMAO" varchar(1) NOT NULL default ' ',
  "AAABGEMQ" varchar(8) NOT NULL default '00000000',
  "AAABGEMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEMT" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  "AAABGUDT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGENA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(60) NOT NULL default ' ',
  "AAABGUDT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGENH" (
  "AAABLRFA" varchar(6) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGENL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABGENO" varchar(2) NOT NULL default ' ',
  "AAABGENP" varchar(10) NOT NULL default ' ',
  "AAABGENQ" varchar(2) NOT NULL default ' ',
  "AAABGENR" varchar(2) NOT NULL default ' ',
  "AAABGENS" varchar(2) NOT NULL default ' ',
  "AAABGENT" varchar(1) NOT NULL default ' ',
  "AAABGENU" varchar(1) NOT NULL default ' ',
  "AAABGENV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIVCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGENY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIVCC" varchar(2) NOT NULL default ' ',
  "AAABGEOC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIVCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGEOI" varchar(1) NOT NULL default ' ',
  "AAABGEOJ" varchar(1) NOT NULL default ' ',
  "AAABGEOK" varchar(1) NOT NULL default ' ',
  "AAABGEOL" varchar(1) NOT NULL default ' ',
  "AAABGEOM" varchar(1) NOT NULL default ' ',
  "AAABGEON" varchar(1) NOT NULL default ' ',
  "AAABGEOO" varchar(1) NOT NULL default ' ',
  "AAABGEOP" varchar(1) NOT NULL default ' ',
  "AAABGEOQ" varchar(1) NOT NULL default ' ',
  "AAABGEOR" varchar(1) NOT NULL default ' ',
  "AAABGEOS" varchar(1) NOT NULL default ' ',
  "AAABGEOT" varchar(1) NOT NULL default ' ',
  "AAABGEOU" varchar(1) NOT NULL default ' ',
  "AAABGEOV" varchar(1) NOT NULL default ' ',
  "AAABGEOW" varchar(1) NOT NULL default ' ',
  "AAABGEOX" varchar(1) NOT NULL default ' ',
  "AAABGEOY" varchar(1) NOT NULL default ' ',
  "AAABGEOZ" varchar(1) NOT NULL default ' ',
  "AAABGEPA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABGEPK" varchar(2) NOT NULL default ' ',
  "AAABLXYW" varchar(4) NOT NULL default ' ',
  "AAABGFZT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABGEPK","AAABLXYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default '00000000',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXY","AAABMAZT","AAABJOUB"),
  KEY "AAABGEQB" ("AAABMAZT","AAABJOUB"),
  KEY "AAABGEQE" ("AAABMBXE"),
  KEY "AAABGEQG" ("AAABMBXY","AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default '00000000',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXY","AAABMAZT","AAABJOUB"),
  KEY "AAABGEQY" ("AAABMAZT","AAABJOUB"),
  KEY "AAABGERB" ("AAABMBXE"),
  KEY "AAABGERD" ("AAABMBXY","AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGERG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default '00000000',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXY","AAABMAZT","AAABJOUB"),
  KEY "AAABGERV" ("AAABMAZT","AAABJOUB"),
  KEY "AAABGERY" ("AAABMBXY","AAABMBXE"),
  KEY "AAABGESB" ("AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGESD" (
  "AAABGFUO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGESG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(10) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  "AAABGFJF" varchar(30) NOT NULL default ' ',
  "AAABGESN" varchar(16) NOT NULL default ' ',
  "AAABGESO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIRY","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGESS" (
  "AAABGFOS" varchar(32) NOT NULL default ' ',
  "AAABGFOR" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGESW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFOS","AAABGFOR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGETA" (
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABMMYV" varchar(14) NOT NULL default ' ',
  "AAABGETD" varchar(14) NOT NULL default ' ',
  "AAABGETE" varchar(32) NOT NULL default ' ',
  "AAABGETF" varchar(70) NOT NULL default ' ',
  "AAABGETG" varchar(70) NOT NULL default ' ',
  "AAABGETH" varchar(10) NOT NULL default ' ',
  "AAABGETI" varchar(25) NOT NULL default ' ',
  "AAABGETJ" varchar(10) NOT NULL default ' ',
  "AAABMINC" varchar(8) NOT NULL default ' ',
  "AAABGETL" varchar(32) NOT NULL default ' ',
  "AAABGETM" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABGFCW" varchar(20) NOT NULL default ' ',
  "AAABGFCX" varchar(100) NOT NULL default ' ',
  "AAABGETQ" varchar(1) NOT NULL default ' ',
  "AAABHAZH" varchar(1) NOT NULL default ' ',
  "AAABGETS" varchar(1) NOT NULL default ' ',
  "AAABGETT" varchar(1) NOT NULL default ' ',
  "AAABGETU" varchar(20) NOT NULL default ' ',
  "AAABGETV" varchar(70) NOT NULL default ' ',
  "AAABGETW" varchar(64) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABLLNF" varchar(14) NOT NULL default '00000000000000',
  "AAABLLNG" varchar(12) NOT NULL default ' ',
  "AAABLLNH" varchar(14) NOT NULL default '00000000000000',
  "AAABGEUB" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEUD" (
  "AAABGFOS" varchar(32) NOT NULL default ' ',
  "AAABGFOR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFOS","AAABGFOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEUI" (
  "AAABGEUO" varchar(10) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABGFJF" varchar(30) NOT NULL default ' ',
  "AAABGEUM" varchar(30) NOT NULL default ' ',
  "AAABGEUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXY","AAABMAZT","AAABJOUB"),
  KEY "AAABGEVE" ("AAABJOUB"),
  KEY "AAABGEVG" ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEVI" (
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(70) NOT NULL default ' ',
  "AAABGEVM" varchar(70) NOT NULL default ' ',
  "AAABGEVN" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBVI","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEVQ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGEVS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEVU" (
  "AAABHBVI" varchar(20) NOT NULL default ' ',
  "AAABLWIE" varchar(2) NOT NULL default ' ',
  "AAABMBXO" varchar(40) NOT NULL default ' ',
  "AAABLQHQ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABHBVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEWA" (
  "AAABMKAI" varchar(32) NOT NULL default ' ',
  "AAABGEXZ" int(11) NOT NULL default '0',
  "AAABGFLE" decimal(21,7) NOT NULL default '0.0000000',
  "AAABGEXB" int(11) NOT NULL default '0',
  "AAABGFTC" varchar(8) NOT NULL default '00000000',
  "AAABGFTD" varchar(6) NOT NULL default '000000',
  "AAABKNBI" varchar(255) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKAI","AAABGEXZ","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEWR" (
  "AAABMKAI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEWU" (
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEWX" (
  "AAABMKAI" varchar(32) NOT NULL default ' ',
  "AAABGEXZ" int(11) NOT NULL default '0',
  "AAABGFLE" decimal(21,7) NOT NULL default '0.0000000',
  "AAABGEXB" int(11) NOT NULL default '0',
  "AAABGFTC" varchar(8) NOT NULL default '00000000',
  "AAABGFTD" varchar(6) NOT NULL default '000000',
  "AAABKWSB" varchar(40) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(10) NOT NULL default ' ',
  "AAABGEXH" varchar(2) NOT NULL default ' ',
  "AAABJOTK" varchar(20) NOT NULL default ' ',
  "AAABGEXJ" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABIOAE" varchar(12) NOT NULL default ' ',
  "AAABGEXP" varchar(4) NOT NULL default ' ',
  "AAABMNYU" varchar(16) NOT NULL default ' ',
  "AAABLQHQ" varchar(255) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKAI","AAABGEXZ","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEYB" (
  "AAABMKAI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEYI" (
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABIJOU" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEYN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABGFLG" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(40) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABGFTK" varchar(12) NOT NULL default '000000000000',
  "AAABGFTL" varchar(40) NOT NULL default ' ',
  "AAABGFTM" varchar(12) NOT NULL default '000000000000',
  "AAABGFLT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMONG","AAABMOIA","AAABHMNR","AAABGFLE"),
  KEY "AAABGEZL" ("AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGEZN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABGFBJ" int(11) NOT NULL default '0',
  "AAABGEZT" varchar(50) NOT NULL default ' ',
  "AAABLDFD" varchar(6) NOT NULL default '000000',
  "AAABGEZV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMONG","AAABMOIA","AAABHMNR","AAABGFLE","AAABGFBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFAB" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMMRH" int(11) NOT NULL default '0',
  "AAABGFAH" varchar(255) NOT NULL default ' ',
  "AAABGFAI" varchar(255) NOT NULL default ' ',
  "AAABGFAJ" varchar(255) NOT NULL default ' ',
  "AAABGFAK" varchar(255) NOT NULL default ' ',
  "AAABGFAL" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMOIA","AAABHMNR","AAABGFLE","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFAR" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABGFBJ" int(11) NOT NULL default '0',
  "AAABLWIE" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABGFBA" int(11) NOT NULL default '0',
  "AAABHCEC" int(11) NOT NULL default '0',
  "AAABGFBC" varchar(132) NOT NULL default ' ',
  "AAABGFBD" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMOIA","AAABHMNR","AAABGFLE","AAABMNLA","AAABGFBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFBK" (
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABGFBM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMAZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFBO" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAABMDXM" varchar(64) NOT NULL default ' ',
  "AAABGFBS" varchar(32) NOT NULL default ' ',
  "AAABGFBT" varchar(1) NOT NULL default ' ',
  "AAABGFBU" varchar(30) NOT NULL default ' ',
  "AAABGFBV" varchar(30) NOT NULL default ' ',
  "AAABGFBW" varchar(1) NOT NULL default ' ',
  "AAABGFBX" varchar(8) NOT NULL default '00000000',
  "AAABGFBY" varchar(1) NOT NULL default ' ',
  "AAABGFBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABGFCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFCC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFFE" varchar(10) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGFFE","AAABGFCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFCK" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABGFCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABGFCU" varchar(3) NOT NULL default ' ',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  "AAABGFCW" varchar(20) NOT NULL default ' ',
  "AAABGFCX" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFDA" (
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABGFDI" varchar(5) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  "AAABMJVE" varchar(80) NOT NULL default ' ',
  "AAABILZU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAZT","AAABGFDI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFDK" (
  "AAABGFDP" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGFDN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFDP","AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFNX" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXF","AAABGFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFED" (
  "AAABGFFE" varchar(10) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGFEJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABGFFE","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFEM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGFFE" varchar(10) NOT NULL default ' ',
  "AAABGFET" varchar(25) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGFFE","AAABGFET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFEU" (
  "AAABGFFG" varchar(10) NOT NULL default ' ',
  "AAABGFFE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFFG","AAABGFFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFEZ" (
  "AAABGFFE" varchar(10) NOT NULL default ' ',
  "AAABLISR" varchar(25) NOT NULL default ' ',
  "AAABGFFG" varchar(10) NOT NULL default ' ',
  "AAABGFFH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFFE","AAABLISR","AAABGFFG","AAABGFFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABGFNX" varchar(1) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXF","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFFU" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default '00000000',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXY","AAABMAZT","AAABJOUB"),
  KEY "AAABGFGM" ("AAABMAZT","AAABJOUB"),
  KEY "AAABGFGP" ("AAABMBXY","AAABMBXE"),
  KEY "AAABGFGS" ("AAABMBXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(15) NOT NULL default '000000000000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFGY" (
  "AAABMINC" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABGFUO" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABHHZS" varchar(1) NOT NULL default ' ',
  "AAABGFVG" varchar(10) NOT NULL default ' ',
  "AAABGFOS" varchar(32) NOT NULL default ' ',
  "AAABGFOR" varchar(10) NOT NULL default ' ',
  "AAABGFOL" varchar(255) NOT NULL default ' ',
  "AAABGFOM" varchar(255) NOT NULL default ' ',
  "AAABGFON" varchar(27) NOT NULL default ' ',
  "AAABGFOO" varchar(27) NOT NULL default ' ',
  "AAABGFOP" varchar(27) NOT NULL default ' ',
  "AAABGFOQ" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMINC","AAABLTXI","AAABGFUO","AAABLPNX","AAABHHZS","AAABGFVG","AAABGFOS","AAABGFOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFHV" (
  "AAABMMRH" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFIU" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABGFIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGFIU","AAABLLLR","AAABMBXE","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFIU" varchar(10) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABGFIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGFIU","AAABLLLR","AAABMOIA","AAABHMNR","AAABGFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(10) NOT NULL default ' ',
  "AAABGFJD" varchar(10) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  "AAABGFJF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIRY","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFJJ" (
  "AAABGFVG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFJM" (
  "AAABGFLU" varchar(3) NOT NULL default '000',
  "AAABGFLV" varchar(32) NOT NULL default ' ',
  "AAABGFKX" varchar(12) NOT NULL default ' ',
  "AAABGFKY" varchar(8) NOT NULL default '00000000',
  "AAABGFKZ" varchar(6) NOT NULL default '000000',
  "AAABGFLA" varchar(1) NOT NULL default ' ',
  "AAABGFLB" varchar(10) NOT NULL default ' ',
  "AAABGFJU" varchar(40) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABGFKA" varchar(8) NOT NULL default '00000000',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFLU","AAABGFLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFKP" varchar(10) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(10) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABGFKM" varchar(1) NOT NULL default ' ',
  "AAABGFKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGFKP","AAABLQGL","AAABMIRY","AAABIYQF","AAABLNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFKU" (
  "AAABGFLU" varchar(3) NOT NULL default '000',
  "AAABGFLV" varchar(32) NOT NULL default ' ',
  "AAABGFKX" varchar(12) NOT NULL default ' ',
  "AAABGFKY" varchar(8) NOT NULL default '00000000',
  "AAABGFKZ" varchar(6) NOT NULL default '000000',
  "AAABGFLA" varchar(1) NOT NULL default ' ',
  "AAABGFLB" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABHMNR" varchar(3) NOT NULL default ' ',
  "AAABGFLE" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABGFLG" varchar(4) NOT NULL default ' ',
  "AAABMNDR" varchar(16) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(40) NOT NULL default ' ',
  "AAABMPMV" varchar(8) NOT NULL default '00000000',
  "AAABGFTK" varchar(12) NOT NULL default '000000000000',
  "AAABGFTL" varchar(40) NOT NULL default ' ',
  "AAABGFTM" varchar(12) NOT NULL default '000000000000',
  "AAABGFLT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGFLU","AAABGFLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFLW" (
  "AAABIJUH" varchar(3) NOT NULL default '000',
  "AAABGFLY" varchar(10) NOT NULL default ' ',
  "AAABGFLZ" varchar(10) NOT NULL default ' ',
  "AAABGFMA" varchar(10) NOT NULL default ' ',
  "AAABGFMB" varchar(10) NOT NULL default ' ',
  "AAABGFMC" varchar(10) NOT NULL default ' ',
  "AAABGFMD" varchar(10) NOT NULL default ' ',
  "AAABGFME" varchar(10) NOT NULL default ' ',
  "AAABGFMF" varchar(10) NOT NULL default ' ',
  "AAABGFMG" varchar(10) NOT NULL default ' ',
  "AAABGFMH" varchar(10) NOT NULL default ' ',
  "AAABGFMI" varchar(10) NOT NULL default ' ',
  "AAABGFMJ" varchar(10) NOT NULL default ' ',
  "AAABIJTA" varchar(10) NOT NULL default ' ',
  "AAABGFML" varchar(10) NOT NULL default ' ',
  "AAABGFMM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIJUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFMO" (
  "AAABLTYA" varchar(10) NOT NULL default ' ',
  "AAABGFMQ" varchar(40) NOT NULL default ' ',
  "AAABGFMR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLTYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABJOTS" varchar(20) NOT NULL default ' ',
  "AAABGFNX" varchar(1) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABGFNS" varchar(4) NOT NULL default '0000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXF","AAABJOTS","AAABGFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGFNX" varchar(1) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABGFNP" varchar(10) NOT NULL default ' ',
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABGFNS" varchar(4) NOT NULL default '0000',
  "AAABILZU" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXF","AAABGFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFNY" (
  "AAABGFOR" varchar(10) NOT NULL default ' ',
  "AAABGFOS" varchar(32) NOT NULL default ' ',
  "AAABIHLK" int(11) NOT NULL default '0',
  "AAABGFOU" varchar(32) NOT NULL default ' ',
  "AAABGFVG" varchar(10) NOT NULL default ' ',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABGFOF" varchar(32) NOT NULL default ' ',
  "AAABGFOG" varchar(255) NOT NULL default ' ',
  "AAABGFOH" varchar(255) NOT NULL default ' ',
  "AAABGFOI" varchar(255) NOT NULL default ' ',
  "AAABGFOJ" varchar(255) NOT NULL default ' ',
  "AAABGFOK" varchar(1) NOT NULL default ' ',
  "AAABGFOL" varchar(255) NOT NULL default ' ',
  "AAABGFOM" varchar(255) NOT NULL default ' ',
  "AAABGFON" varchar(27) NOT NULL default ' ',
  "AAABGFOO" varchar(27) NOT NULL default ' ',
  "AAABGFOP" varchar(27) NOT NULL default ' ',
  "AAABGFOQ" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFOR","AAABGFOS","AAABIHLK","AAABGFOU","AAABGFVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(4) NOT NULL default '0000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABGFPI" varchar(1) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABILZU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABMRFP","AAABGFPI","AAABMBXE","AAABMBXF","AAABMAZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSIV" varchar(10) NOT NULL default ' ',
  "AAABHSIQ" varchar(32) NOT NULL default ' ',
  "AAABMMJT" varchar(79) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSIV","AAABHSIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFPU" (
  "AAABGFRN" varchar(4) NOT NULL default ' ',
  "AAABGFRO" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABJRGL" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(1) NOT NULL default ' ',
  "AAABLDTB" varchar(1) NOT NULL default ' ',
  "AAABGFQB" varchar(1) NOT NULL default ' ',
  "AAABGFQC" varchar(1) NOT NULL default ' ',
  "AAABGFQD" varchar(1) NOT NULL default ' ',
  "AAABHYMT" varchar(1) NOT NULL default ' ',
  "AAABHYMU" varchar(1) NOT NULL default ' ',
  "AAABHYMW" varchar(1) NOT NULL default ' ',
  "AAABGFQH" varchar(1) NOT NULL default ' ',
  "AAABLUXQ" varchar(1) NOT NULL default ' ',
  "AAABGFQJ" varchar(1) NOT NULL default ' ',
  "AAABGFQK" varchar(1) NOT NULL default ' ',
  "AAABGFQL" varchar(1) NOT NULL default ' ',
  "AAABGFQM" varchar(1) NOT NULL default ' ',
  "AAABGFQN" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  "AAABGFQQ" varchar(1) NOT NULL default ' ',
  "AAABGFQR" varchar(1) NOT NULL default ' ',
  "AAABGFQS" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABGFQU" varchar(1) NOT NULL default ' ',
  "AAABGFQV" varchar(1) NOT NULL default ' ',
  "AAABHAGF" varchar(1) NOT NULL default ' ',
  "AAABJSDF" varchar(1) NOT NULL default ' ',
  "AAABGFQY" varchar(1) NOT NULL default ' ',
  "AAABGFQZ" varchar(1) NOT NULL default ' ',
  "AAABLRSH" varchar(1) NOT NULL default ' ',
  "AAABGFRB" varchar(1) NOT NULL default ' ',
  "AAABGFRC" varchar(1) NOT NULL default ' ',
  "AAABGFRD" varchar(1) NOT NULL default ' ',
  "AAABGFRE" varchar(1) NOT NULL default ' ',
  "AAABILZU" varchar(1) NOT NULL default ' ',
  "AAABIFVP" varchar(1) NOT NULL default ' ',
  "AAABGFRH" varchar(1) NOT NULL default ' ',
  "AAABGFRI" varchar(1) NOT NULL default ' ',
  "AAABGFRJ" varchar(1) NOT NULL default ' ',
  "AAABGFRK" varchar(1) NOT NULL default ' ',
  "AAABGFRL" varchar(1) NOT NULL default ' ',
  "AAABGFRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFRN","AAABGFRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABHSIV" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  "AAABGFRV" varchar(1) NOT NULL default ' ',
  "AAABGFRW" varchar(1) NOT NULL default ' ',
  "AAABGFRX" varchar(1) NOT NULL default ' ',
  "AAABGFRY" varchar(1) NOT NULL default ' ',
  "AAABGFRZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABHSIV","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNBI" varchar(4) NOT NULL default ' ',
  "AAABGFTP" varchar(10) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABGFTB" varchar(40) NOT NULL default ' ',
  "AAABGFTC" varchar(8) NOT NULL default '00000000',
  "AAABGFTD" varchar(6) NOT NULL default '000000',
  "AAABGFTE" varchar(8) NOT NULL default '00000000',
  "AAABGFTF" varchar(6) NOT NULL default '000000',
  "AAABGFTG" varchar(4) NOT NULL default ' ',
  "AAABGFTH" varchar(30) NOT NULL default ' ',
  "AAABLQHQ" varchar(70) NOT NULL default ' ',
  "AAABLILU" varchar(1) NOT NULL default ' ',
  "AAABGFTK" varchar(12) NOT NULL default '000000000000',
  "AAABGFTL" varchar(40) NOT NULL default ' ',
  "AAABGFTM" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAABKNBI","AAABGFTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFTQ" (
  "AAABGFTV" varchar(1) NOT NULL default ' ',
  "AAABGFTW" varchar(1) NOT NULL default ' ',
  "AAABGFTX" varchar(1) NOT NULL default ' ',
  "AAABGFTY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFTV","AAABGFTW","AAABGFTX","AAABGFTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFUB" varchar(10) NOT NULL default ' ',
  "AAABGFUC" varchar(10) NOT NULL default ' ',
  "AAABGFUD" varchar(10) NOT NULL default ' ',
  "AAABGFUE" varchar(10) NOT NULL default ' ',
  "AAABIVNA" varchar(27) NOT NULL default ' ',
  "AAABIVNB" varchar(27) NOT NULL default ' ',
  "AAABIVNC" varchar(27) NOT NULL default ' ',
  "AAABIVND" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFUK" (
  "AAABGFUO" varchar(10) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABGFUN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFUO","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFUQ" (
  "AAABGFVO" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFVO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFUW" (
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFVC" (
  "AAABGFVG" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGFVF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFVG","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABGFVS" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  "AAABGFVO" varchar(10) NOT NULL default ' ',
  "AAABGFVP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABGFVS","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFVU" (
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFXW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFWI" (
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAZT","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXE","AAABMBXF","AAABLTXI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFXA" (
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  "AAABGFXC" varchar(10) NOT NULL default ' ',
  "AAABMAZT" varchar(2) NOT NULL default ' ',
  "AAABGFXE" varchar(20) NOT NULL default ' ',
  "AAABGFXF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFXH" (
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  "AAABGFXW" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABJOUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFXN" (
  "AAABGFXW" varchar(10) NOT NULL default ' ',
  "AAABGFXP" varchar(30) NOT NULL default ' ',
  "AAABGFXQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFXS" (
  "AAABGFXW" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABJLPZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFXW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFXY" (
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABGFYE" varchar(32) NOT NULL default ' ',
  "AAABGFYF" varchar(1) NOT NULL default ' ',
  "AAABGFYC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMRZ","AAABGFYE","AAABGFYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFYG" (
  "AAABGFYL" varchar(128) NOT NULL default ' ',
  "AAABGFYM" varchar(2) NOT NULL default ' ',
  "AAABIKFK" varchar(1) NOT NULL default ' ',
  "AAABGFYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGFYL","AAABGFYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFYN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGFZF" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABGFZI" int(11) NOT NULL default '0',
  "AAABHFCH" varchar(4) NOT NULL default ' ',
  "AAABGFYU" varchar(70) NOT NULL default ' ',
  "AAABKNBI" varchar(3) NOT NULL default '000',
  "AAABJTGS" varchar(80) NOT NULL default ' ',
  "AAABHHCK" varchar(80) NOT NULL default ' ',
  "AAABGFYY" varchar(2) NOT NULL default ' ',
  "AAABGFYZ" varchar(1) NOT NULL default ' ',
  "AAABINHC" varchar(50) NOT NULL default ' ',
  "AAABINHD" varchar(50) NOT NULL default ' ',
  "AAABINHE" varchar(50) NOT NULL default ' ',
  "AAABINHF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGFZF","AAABMRFQ","AAABMQIX","AAABGFZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFZV" varchar(3) NOT NULL default ' ',
  "AAABGFZM" varchar(8) NOT NULL default '00000000',
  "AAABGFZN" varchar(6) NOT NULL default '000000',
  "AAABGFZO" varchar(8) NOT NULL default '00000000',
  "AAABGFZP" varchar(6) NOT NULL default '000000',
  "AAABGFZQ" varchar(8) NOT NULL default '00000000',
  "AAABGFZR" varchar(6) NOT NULL default '000000',
  "AAABGFZS" varchar(8) NOT NULL default '00000000',
  "AAABGFZT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGFZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGFZW" (
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABILQC" varchar(10) NOT NULL default ' ',
  "AAABILQD" varchar(10) NOT NULL default ' ',
  "AAABILQE" varchar(10) NOT NULL default ' ',
  "AAABILQF" varchar(10) NOT NULL default ' ',
  "AAABILQG" varchar(10) NOT NULL default ' ',
  "AAABILQH" varchar(10) NOT NULL default ' ',
  "AAABILQI" varchar(10) NOT NULL default ' ',
  "AAABILQJ" varchar(10) NOT NULL default ' ',
  "AAABILQK" varchar(10) NOT NULL default ' ',
  "AAABILQL" varchar(10) NOT NULL default ' ',
  "AAABGGAO" varchar(4) NOT NULL default ' ',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABGGAK" varchar(8) NOT NULL default ' ',
  "AAABGGAL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLBW"),
  KEY "AAABGGAN" ("AAABGGAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGAP" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABLFYC","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGAW" (
  "AAABGGBA" varchar(20) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGBA","AAABLFYC","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGBD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGBJ" (
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMLBW","AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGBR" (
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLBW","AAABILHK","AAABMRCU","AAABMKXW","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGCE" (
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABJHGJ" varchar(1) NOT NULL default ' ',
  "AAABJHGK" varchar(1) NOT NULL default ' ',
  "AAABIKRS" varchar(1) NOT NULL default ' ',
  "AAABIKRT" varchar(1) NOT NULL default ' ',
  "AAABIKRU" varchar(1) NOT NULL default ' ',
  "AAABLQJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOKY" varchar(8) NOT NULL default '00000000',
  "AAABLOKZ" varchar(6) NOT NULL default '000000',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABLOLA" varchar(12) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABGGCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOKY","AAABLOKZ","AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGCZ" (
  "AAABGGDM" varchar(6) NOT NULL default '000000',
  "AAABGGDB" varchar(30) NOT NULL default ' ',
  "AAABHJAF" varchar(8) NOT NULL default '00000000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAABGGDE" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDF" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDG" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDH" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDI" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDJ" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDK" decimal(15,2) NOT NULL default '0.00',
  "AAABGGDL" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABGGDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABLOKY" varchar(8) NOT NULL default '00000000',
  "AAABLOKZ" varchar(6) NOT NULL default '000000',
  "AAABLOLA" varchar(12) NOT NULL default ' ',
  "AAABGGED" varchar(40) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABLOKY" varchar(8) NOT NULL default '00000000',
  "AAABLOKZ" varchar(6) NOT NULL default '000000',
  "AAABLOLA" varchar(12) NOT NULL default ' ',
  "AAABGGED" varchar(40) NOT NULL default ' ',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGEH" (
  "AAABGGEW" varchar(20) NOT NULL default ' ',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGEW","AAABMLBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGEM" (
  "AAABGGEW" varchar(20) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABGGEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGES" (
  "AAABGGEW" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGEW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGEY" (
  "AAABHFPN" varchar(4) NOT NULL default ' ',
  "AAABHFPO" varchar(4) NOT NULL default ' ',
  "AAABHFPP" varchar(120) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABHUHJ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABHFPH" varchar(1) NOT NULL default ' ',
  "AAABHFPI" varchar(1) NOT NULL default ' ',
  "AAABHFPJ" varchar(1) NOT NULL default ' ',
  "AAABHFPK" varchar(6) NOT NULL default '000000',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  "AAABHFPM" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABHFPN","AAABHFPO","AAABHFPP"),
  KEY "AAABGGFQ" ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGFU" (
  "AAABMONG" varchar(3) NOT NULL default ' ',
  "AAABGGGL" varchar(8) NOT NULL default ' ',
  "AAABGGGM" varchar(40) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLESS" varchar(84) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMNLA" varchar(4) NOT NULL default ' ',
  "AAABGGGS" varchar(40) NOT NULL default ' ',
  "AAABGGGE" varchar(8) NOT NULL default '00000000',
  "AAABGGGF" varchar(3) NOT NULL default ' ',
  "AAABGGGG" varchar(12) NOT NULL default ' ',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABMRCK" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGGGL","AAABGGGM","AAABLRST","AAABMKFH","AAABLESS","AAABIRUN","AAABMNLA","AAABGGGS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGGT" (
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGGX" (
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGGHA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGHD" (
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABGGHO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUEO","AAABGGHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGHI" (
  "AAABLUEO" varchar(10) NOT NULL default ' ',
  "AAABGGHO" varchar(4) NOT NULL default '0000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABGGHO","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGHQ" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGGHT" varchar(1) NOT NULL default ' ',
  "AAABLTQF" varchar(10) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABGGJL" varchar(32) NOT NULL default ' ',
  "AAABGGHX" varchar(32) NOT NULL default ' ',
  "AAABGGHY" varchar(32) NOT NULL default ' ',
  "AAABHJRW" varchar(1) NOT NULL default ' ',
  "AAABGGIA" varchar(1) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABLLNF" varchar(6) NOT NULL default '000000',
  "AAABGGJQ" varchar(4) NOT NULL default ' ',
  "AAABGGIF" varchar(12) NOT NULL default ' ',
  "AAABGGIG" varchar(8) NOT NULL default '00000000',
  "AAABGGIH" varchar(6) NOT NULL default '000000',
  "AAABGGII" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABGGIK" varchar(12) NOT NULL default ' ',
  "AAABGGIL" varchar(8) NOT NULL default '00000000',
  "AAABGGIM" varchar(6) NOT NULL default '000000',
  "AAABGGIN" varchar(20) NOT NULL default ' ',
  "AAABGGIO" varchar(4) NOT NULL default '0000',
  "AAABGGIP" varchar(8) NOT NULL default ' ',
  "AAABGGIQ" varchar(10) NOT NULL default '0000000000',
  "AAABGGIR" varchar(10) NOT NULL default '0000000000',
  "AAABGGLM" varchar(38) NOT NULL default ' ',
  "AAABGGLN" varchar(38) NOT NULL default ' ',
  "AAABGGJN" varchar(2) NOT NULL default ' ',
  "AAABGGJT" varchar(26) NOT NULL default ' ',
  "AAABGGJI" varchar(10) NOT NULL default ' ',
  "AAABGGIX" varchar(1) NOT NULL default ' ',
  "AAABGGIY" varchar(1) NOT NULL default ' ',
  "AAABGGIZ" varchar(2) NOT NULL default '00',
  "AAABLDFD" varchar(2) NOT NULL default '00',
  "AAABGGJB" varchar(1) NOT NULL default ' ',
  "AAABGGJC" varchar(4) NOT NULL default ' ',
  "AAABGGJD" varchar(12) NOT NULL default ' ',
  "AAABHJPU" varchar(8) NOT NULL default '00000000',
  "AAABGGJF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV"),
  KEY "AAABGGJH" ("AAABGGJI","AAABLTQF"),
  KEY "AAABGGJK" ("AAABGGJL"),
  KEY "AAABGGJM" ("AAABGGJN"),
  KEY "AAABGGJO" ("AAABLTQF","AAABGGJQ","AAABLHGN"),
  KEY "AAABGGJS" ("AAABGGJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGJU" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABGGJW" varchar(12) NOT NULL default ' ',
  "AAABGGKB" varchar(10) NOT NULL default ' ',
  "AAABGGJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD"),
  KEY "AAABGGKA" ("AAABGGKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGKC" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLUZL" varchar(40) NOT NULL default ' ',
  "AAABLTQF" varchar(10) NOT NULL default ' ',
  "AAABGGKG" varchar(4) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGKK" (
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABGGKN" varchar(80) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGKT" varchar(5) NOT NULL default '00000',
  "AAABGGKU" varchar(5) NOT NULL default '00000',
  "AAABGGKV" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(2) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAABGGKZ" varchar(8) NOT NULL default ' ',
  "AAABGGLA" varchar(12) NOT NULL default ' ',
  "AAABGGLB" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGLD" varchar(1) NOT NULL default ' ',
  "AAABGGLE" varchar(4) NOT NULL default ' ',
  "AAABGGLF" varchar(5) NOT NULL default '00000',
  "AAABGGLG" varchar(4) NOT NULL default ' ',
  "AAABGGLH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGLJ" (
  "AAABIKBS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABGGLM" varchar(38) NOT NULL default ' ',
  "AAABGGLN" varchar(38) NOT NULL default ' ',
  "AAABGGLO" varchar(32) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABGGLR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKBS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGLU" (
  "AAABIKBS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKBS","AAABLTXI","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABHYAL" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGMS" varchar(18) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABGGMN" decimal(5,0) NOT NULL default '0',
  "AAABGGMO" decimal(15,2) NOT NULL default '0.00',
  "AAABGGMP" decimal(15,2) NOT NULL default '0.00',
  "AAABGGMQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGMS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABGGNM" varchar(4) NOT NULL default ' ',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGGNI" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABMLES","AAABGGNM","AAABGLCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABGGOA" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABGIMG" varchar(10) NOT NULL default ' ',
  "AAABGIMH" varchar(10) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  "AAABGGNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRGH","AAABGGOA","AAABMCRJ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABGGOH" varchar(1) NOT NULL default ' ',
  "AAABKIUC" varchar(1) NOT NULL default ' ',
  "AAABGGOJ" varchar(3) NOT NULL default ' ',
  "AAABGGOK" varchar(3) NOT NULL default ' ',
  "AAABGGOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRGH","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGOX" (
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABKQEQ" varchar(5) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLTXG","AAABLKXR","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMECK" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABHAAP" varchar(1) NOT NULL default ' ',
  "AAABGGPK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMECK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGQO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGQO" varchar(2) NOT NULL default ' ',
  "AAABICRN" varchar(15) NOT NULL default ' ',
  "AAABICRM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGGQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAABGGQO" varchar(2) NOT NULL default ' ',
  "AAABLBYG" varchar(8) NOT NULL default '00000000',
  "AAABGGQQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABGGQJ" varchar(5) NOT NULL default ' ',
  "AAABJMIU" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLDBD","AAABGGQO","AAABLBYG","AAABGGQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGGVN" varchar(2) NOT NULL default ' ',
  "AAABGGQU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMWY" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGMWY","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMWY" varchar(3) NOT NULL default '000',
  "AAABLLBG" varchar(3) NOT NULL default '000',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMWY","AAABLLBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(2) NOT NULL default '00',
  "AAABGGRS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMECK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGSK" varchar(2) NOT NULL default '00',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABHAAP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGSK" varchar(2) NOT NULL default '00',
  "AAABGGSH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGSZ" varchar(2) NOT NULL default '00',
  "AAABGGSO" varchar(1) NOT NULL default ' ',
  "AAABGGSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGSZ" varchar(2) NOT NULL default '00',
  "AAABGGSW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGTM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGGTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGTM" varchar(2) NOT NULL default '00',
  "AAABGGTJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGTZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGTZ" varchar(4) NOT NULL default ' ',
  "AAABGGTW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLAQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLAQG" varchar(3) NOT NULL default ' ',
  "AAABGGUJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLAQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGUZ" varchar(4) NOT NULL default ' ',
  "AAABGGUW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGVN" varchar(2) NOT NULL default '00',
  "AAABGGVD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGVN" varchar(2) NOT NULL default '00',
  "AAABGGVK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIDUS" varchar(2) NOT NULL default '00',
  "AAABGGVR" varchar(1) NOT NULL default ' ',
  "AAABGGVS" varchar(1) NOT NULL default ' ',
  "AAABGGVT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIDUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIDUS" varchar(2) NOT NULL default '00',
  "AAABGGWA" varchar(15) NOT NULL default ' ',
  "AAABGGWB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIDUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGXH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGGXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGWK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKVKH" varchar(4) NOT NULL default ' ',
  "AAABKYNY" varchar(255) NOT NULL default ' ',
  "AAABKYNZ" varchar(255) NOT NULL default ' ',
  "AAABKYOA" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGWS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABKYNY" varchar(255) NOT NULL default ' ',
  "AAABKYNZ" varchar(255) NOT NULL default ' ',
  "AAABKYOA" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGXH" varchar(2) NOT NULL default '00',
  "AAABGGXE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGXU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGGXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGXU" varchar(2) NOT NULL default '00',
  "AAABGGXR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGYH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGGYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGYH" varchar(2) NOT NULL default '00',
  "AAABGGYE" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGYV" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGYV" varchar(4) NOT NULL default ' ',
  "AAABGGYS" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCB" varchar(10) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  "AAABGGZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCCB" varchar(10) NOT NULL default ' ',
  "AAABGGZH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLCCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGGZY" varchar(4) NOT NULL default ' ',
  "AAABLZNE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGGZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGZY" varchar(4) NOT NULL default ' ',
  "AAABGGZV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGGZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGGZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHAM" varchar(4) NOT NULL default ' ',
  "AAABLQPX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGHAM" varchar(4) NOT NULL default ' ',
  "AAABGHAJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGHAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHAN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCDD" varchar(4) NOT NULL default ' ',
  "AAABKEVT" varchar(10) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLCDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHBG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGHBG" varchar(2) NOT NULL default ' ',
  "AAABGHBD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGHBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABIZHF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGNAZ","AAABIZHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABIZHF" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNAZ","AAABIZHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABIZHF" varchar(2) NOT NULL default '00',
  "AAABGHCS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNAZ","AAABIZHF","AAABGHCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABIZHF" varchar(2) NOT NULL default '00',
  "AAABGHCS" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNAZ","AAABIZHF","AAABGHCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHDQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGHDC" varchar(1) NOT NULL default ' ',
  "AAABGHDD" varchar(1) NOT NULL default ' ',
  "AAABGHDE" varchar(30) NOT NULL default ' ',
  "AAABGHDF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGHDQ" varchar(10) NOT NULL default ' ',
  "AAABGHDN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGHDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHED" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGHED" varchar(10) NOT NULL default ' ',
  "AAABGHEA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGHED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(5) NOT NULL default '00000',
  "AAABGHEN" varchar(10) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABGHEJ" varchar(1) NOT NULL default ' ',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABGHEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWUH" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(4) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABMLXR" varchar(5) NOT NULL default '00000',
  "AAABLXDP" varchar(1) NOT NULL default ' ',
  "AAABGHEV" varchar(1) NOT NULL default ' ',
  "AAABGHEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWUH","AAABMALG","AAABLKXQ","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(5) NOT NULL default '00000',
  "AAABGHFI" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABGHFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABGHFN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXQ","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKWZ" varchar(6) NOT NULL default ' ',
  "AAABMOHX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMKWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALG","AAABHXNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHGG" (
  "AAABGHGT" varchar(14) NOT NULL default '00000000000000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGHGV" varchar(10) NOT NULL default ' ',
  "AAABGHGW" varchar(3) NOT NULL default ' ',
  "AAABGHGX" varchar(1) NOT NULL default ' ',
  "AAABGHGM" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABGHGO" varchar(12) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABKWUO" varchar(12) NOT NULL default ' ',
  "AAABGHGS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHGT","AAABILWI","AAABGHGV","AAABGHGW","AAABGHGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHGY" (
  "AAABGHHC" varchar(40) NOT NULL default ' ',
  "AAABLRTW" varchar(14) NOT NULL default '00000000000000',
  "AAABLEQO" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABGHHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABGHHV" varchar(20) NOT NULL default ' ',
  "AAABGHHH" varchar(128) NOT NULL default ' ',
  "AAABGHHI" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGYLC","AAABGHHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABGHHV" varchar(20) NOT NULL default ' ',
  "AAABIFVP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGYLC","AAABGHHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABGHHZ" varchar(1) NOT NULL default ' ',
  "AAABGHIA" varchar(2) NOT NULL default ' ',
  "AAABJLIP" varchar(2) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMELB" varchar(2) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHIH" (
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  "AAABHBWK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHIL" (
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSKP","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHIR" (
  "AAABJSKP" varchar(20) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABHBWK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABJSKP","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHJO" varchar(4) NOT NULL default ' ',
  "AAABLORS" varchar(20) NOT NULL default ' ',
  "AAABGHJB" int(11) NOT NULL default '0',
  "AAABGHJC" int(11) NOT NULL default '0',
  "AAABGHJD" int(11) NOT NULL default '0',
  "AAABGHJE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGHJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGHJO" varchar(4) NOT NULL default ' ',
  "AAABGHJL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGHJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMALG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGYLC" varchar(1) NOT NULL default '0',
  "AAABGHKH" varchar(1) NOT NULL default '0',
  "AAABGHKI" varchar(1) NOT NULL default ' ',
  "AAABGHKC" tinyint(3) unsigned NOT NULL default '0',
  "AAABLOZE" varchar(20) NOT NULL default ' ',
  "AAABGHKE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGYLC","AAABGHKH","AAABGHKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCXI" varchar(21) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHKO" (
  "AAABGHKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABJOTS" varchar(20) NOT NULL default ' ',
  "AAABGHLL" varchar(8) NOT NULL default '00000000',
  "AAABGHLM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLLNS","AAABJOTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABJOTS" varchar(20) NOT NULL default ' ',
  "AAABGHMO" varchar(12) NOT NULL default ' ',
  "AAABGHLL" varchar(8) NOT NULL default '00000000',
  "AAABGHLM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLLNS","AAABJOTS","AAABGHMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEJI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHLW" (
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLEJI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNS","AAABLEJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHMB" (
  "AAABIYQF" varchar(10) NOT NULL default ' ',
  "AAABGHMD" varchar(30) NOT NULL default ' ',
  "AAABGHME" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABGHMO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABGHMU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABGHNA" int(11) NOT NULL default '0',
  "AAABGHNB" int(11) NOT NULL default '0',
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGHOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHNF" (
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABKZAY" varchar(30) NOT NULL default ' ',
  "AAABGHOR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHOP","AAABKZAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHNL" (
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABKZAY" varchar(30) NOT NULL default ' ',
  "AAABGHNO" tinyint(3) unsigned NOT NULL default '0',
  "AAABGHNP" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABGHOP","AAABKZAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHNS" (
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABKZAY" varchar(30) NOT NULL default ' ',
  "AAABGHOR" varchar(20) NOT NULL default ' ',
  "AAABGHOS" int(11) NOT NULL default '0',
  "AAABHFQC" varchar(40) NOT NULL default ' ',
  "AAABGHNY" varchar(4) NOT NULL default ' ',
  "AAABGHNZ" int(11) NOT NULL default '0',
  "AAABGHOA" int(11) NOT NULL default '0',
  "AAABGHOB" varchar(1) NOT NULL default ' ',
  "AAABHFSP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHOP","AAABKZAY","AAABGHOR","AAABGHOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHOH" (
  "AAABGHOP" varchar(12) NOT NULL default ' ',
  "AAABKZAY" varchar(30) NOT NULL default ' ',
  "AAABGHOR" varchar(20) NOT NULL default ' ',
  "AAABGHOS" int(11) NOT NULL default '0',
  "AAABGHOT" int(11) NOT NULL default '0',
  "AAABGHON" varchar(100) NOT NULL default ' ',
  "AAABLTQG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGHOP","AAABKZAY","AAABGHOR","AAABGHOS","AAABGHOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHOU" (
  "AAABGHOZ" varchar(2) NOT NULL default '00',
  "AAABGHOW" varchar(2) NOT NULL default '00',
  "AAABGHOX" varchar(2) NOT NULL default '00',
  "AAABGHOY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(7) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCKE" varchar(12) NOT NULL default ' ',
  "AAABLXWL" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(7) NOT NULL default ' ',
  "AAABGHPP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHPT" (
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABGHWL" varchar(40) NOT NULL default ' ',
  "AAABGHPW" varchar(40) NOT NULL default ' ',
  "AAABGHWN" varchar(40) NOT NULL default ' ',
  "AAABGHWO" varchar(40) NOT NULL default ' ',
  "AAABGHWP" varchar(40) NOT NULL default ' ',
  "AAABGHWM" varchar(40) NOT NULL default ' ',
  "AAABGHQB" varchar(40) NOT NULL default ' ',
  "AAABGHWR" varchar(40) NOT NULL default ' ',
  "AAABGHWS" varchar(40) NOT NULL default ' ',
  "AAABGHWT" varchar(40) NOT NULL default ' ',
  "AAABGHWU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHQH" (
  "AAABGHVF" varchar(20) NOT NULL default ' ',
  "AAABINFD" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABGHQL" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(8) NOT NULL default ' ',
  "AAABLUDG" varchar(80) NOT NULL default ' ',
  "AAABGHQO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHVF","AAABINFD","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHQS" (
  "AAABGIDW" varchar(25) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABMAKT" varchar(4) NOT NULL default '0000',
  "AAABGHRZ" varchar(4) NOT NULL default '0000',
  "AAABGHRP" varchar(10) NOT NULL default ' ',
  "AAABGHRQ" varchar(32) NOT NULL default ' ',
  "AAABGHRR" varchar(32) NOT NULL default ' ',
  "AAABGHTC" varchar(25) NOT NULL default ' ',
  "AAABGHRT" varchar(8) NOT NULL default ' ',
  "AAABGHRU" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW","AAABLHMS","AAABJECS","AAABMAKT","AAABGHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHRJ" (
  "AAABGIDW" varchar(25) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABJECS" varchar(60) NOT NULL default ' ',
  "AAABMAKT" varchar(4) NOT NULL default '0000',
  "AAABGHRZ" varchar(4) NOT NULL default '0000',
  "AAABGHRP" varchar(10) NOT NULL default ' ',
  "AAABGHRQ" varchar(32) NOT NULL default ' ',
  "AAABGHRR" varchar(32) NOT NULL default ' ',
  "AAABGHTC" varchar(25) NOT NULL default ' ',
  "AAABGHRT" varchar(8) NOT NULL default ' ',
  "AAABGHRU" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW","AAABLHMS","AAABJECS","AAABMAKT","AAABGHRZ"),
  KEY "AAABGHSA" ("AAABGIDW","AAABLHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHSD" (
  "AAABGIDW" varchar(25) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABGHTC" varchar(25) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABGIET" varchar(12) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABGHTH" varchar(6) NOT NULL default '000000',
  "AAABGHTI" varchar(12) NOT NULL default ' ',
  "AAABGHTJ" varchar(60) NOT NULL default ' ',
  "AAABHMQN" varchar(20) NOT NULL default ' ',
  "AAABGHTW" varchar(2) NOT NULL default ' ',
  "AAABINFD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW","AAABLHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHSW" (
  "AAABGIDW" varchar(25) NOT NULL default ' ',
  "AAABLHMS" varchar(6) NOT NULL default '000000',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABGHTC" varchar(25) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABGIET" varchar(12) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABGHTH" varchar(6) NOT NULL default '000000',
  "AAABGHTI" varchar(12) NOT NULL default ' ',
  "AAABGHTJ" varchar(60) NOT NULL default ' ',
  "AAABHMQN" varchar(20) NOT NULL default ' ',
  "AAABGHTW" varchar(2) NOT NULL default ' ',
  "AAABINFD" varchar(3) NOT NULL default ' ',
  "AAABJEEM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW","AAABLHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHTQ" (
  "AAABGHVC" varchar(20) NOT NULL default ' ',
  "AAABLUVJ" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(20) NOT NULL default ' ',
  "AAABGHVF" varchar(20) NOT NULL default ' ',
  "AAABINFD" varchar(3) NOT NULL default ' ',
  "AAABGHTW" varchar(2) NOT NULL default ' ',
  "AAABGHTX" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(8) NOT NULL default ' ',
  "AAABGHTZ" varchar(16) NOT NULL default ' ',
  "AAABGHUA" varchar(60) NOT NULL default ' ',
  "AAABGHUB" varchar(60) NOT NULL default ' ',
  "AAABGHUC" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(20) NOT NULL default ' ',
  "AAABGHUE" varchar(20) NOT NULL default ' ',
  "AAABGHUF" varchar(3) NOT NULL default ' ',
  "AAABGHUG" varchar(3) NOT NULL default ' ',
  "AAABGHUH" varchar(3) NOT NULL default ' ',
  "AAABGHUI" varchar(3) NOT NULL default ' ',
  "AAABGHUJ" varchar(3) NOT NULL default ' ',
  "AAABGHUK" varchar(3) NOT NULL default ' ',
  "AAABGHUL" varchar(3) NOT NULL default ' ',
  "AAABGHUM" varchar(1) NOT NULL default ' ',
  "AAABGHUN" varchar(1) NOT NULL default ' ',
  "AAABGHUO" varchar(1) NOT NULL default ' ',
  "AAABGHUP" varchar(1) NOT NULL default ' ',
  "AAABGHUQ" varchar(1) NOT NULL default ' ',
  "AAABGHUR" varchar(1) NOT NULL default ' ',
  "AAABGHUS" varchar(1) NOT NULL default ' ',
  "AAABHMQN" varchar(20) NOT NULL default ' ',
  "AAABGHUU" varchar(3) NOT NULL default ' ',
  "AAABGHUV" varchar(3) NOT NULL default ' ',
  "AAABGHUW" varchar(3) NOT NULL default ' ',
  "AAABGHUX" varchar(1) NOT NULL default ' ',
  "AAABGHUY" varchar(1) NOT NULL default ' ',
  "AAABGHUZ" varchar(1) NOT NULL default ' ',
  "AAABGHVA" varchar(25) NOT NULL default ' ',
  "AAABGHVB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABGHVC","AAABLUVJ","AAABMOHM","AAABGHVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZNE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZNE" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHVT" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGHWG" varchar(2) NOT NULL default '00',
  "AAABGHVW" varchar(8) NOT NULL default ' ',
  "AAABGHVX" varchar(40) NOT NULL default ' ',
  "AAABGHVY" varchar(2) NOT NULL default '00',
  "AAABGHVZ" varchar(3) NOT NULL default ' ',
  "AAABLUVS" varchar(40) NOT NULL default ' ',
  "AAABLUVJ" varchar(8) NOT NULL default ' ',
  "AAABGHWC" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLEKT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW","AAABGHWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHWH" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGHWL" varchar(1) NOT NULL default ' ',
  "AAABGHWM" varchar(1) NOT NULL default ' ',
  "AAABGHWN" varchar(1) NOT NULL default ' ',
  "AAABGHWO" varchar(1) NOT NULL default ' ',
  "AAABGHWP" varchar(1) NOT NULL default ' ',
  "AAABGHWQ" varchar(1) NOT NULL default ' ',
  "AAABGHWR" varchar(1) NOT NULL default ' ',
  "AAABGHWS" varchar(1) NOT NULL default ' ',
  "AAABGHWT" varchar(1) NOT NULL default ' ',
  "AAABGHWU" varchar(1) NOT NULL default ' ',
  "AAABGHWV" varchar(1) NOT NULL default ' ',
  "AAABGHWW" varchar(1) NOT NULL default ' ',
  "AAABGHWX" varchar(1) NOT NULL default ' ',
  "AAABGHWY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHXA" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABMUCY" varchar(1) NOT NULL default ' ',
  "AAABGHXF" varchar(1) NOT NULL default ' ',
  "AAABGHXG" varchar(1) NOT NULL default ' ',
  "AAABGHXH" varchar(1) NOT NULL default ' ',
  "AAABGHXI" varchar(1) NOT NULL default ' ',
  "AAABGHXJ" varchar(1) NOT NULL default ' ',
  "AAABGHXK" varchar(1) NOT NULL default ' ',
  "AAABGHXL" varchar(1) NOT NULL default ' ',
  "AAABGHXM" varchar(1) NOT NULL default ' ',
  "AAABGHXN" varchar(1) NOT NULL default ' ',
  "AAABGHXO" varchar(1) NOT NULL default ' ',
  "AAABGHXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHXR" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGIDC" varchar(1) NOT NULL default ' ',
  "AAABGIDD" varchar(1) NOT NULL default ' ',
  "AAABGIDE" varchar(1) NOT NULL default ' ',
  "AAABGIDF" varchar(1) NOT NULL default ' ',
  "AAABGIDG" varchar(1) NOT NULL default ' ',
  "AAABGIDH" varchar(1) NOT NULL default ' ',
  "AAABGIDI" varchar(1) NOT NULL default ' ',
  "AAABGIDJ" varchar(1) NOT NULL default ' ',
  "AAABGIDK" varchar(1) NOT NULL default ' ',
  "AAABGIDL" varchar(1) NOT NULL default ' ',
  "AAABGIDM" varchar(1) NOT NULL default ' ',
  "AAABGIDN" varchar(1) NOT NULL default ' ',
  "AAABGIDO" varchar(1) NOT NULL default ' ',
  "AAABGIDP" varchar(1) NOT NULL default ' ',
  "AAABGIDQ" varchar(1) NOT NULL default ' ',
  "AAABGIDR" varchar(1) NOT NULL default ' ',
  "AAABGIDS" varchar(1) NOT NULL default ' ',
  "AAABGIDT" varchar(1) NOT NULL default ' ',
  "AAABGIDU" varchar(1) NOT NULL default ' ',
  "AAABGIDV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHYQ" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGHYU" varchar(1) NOT NULL default ' ',
  "AAABGHYV" varchar(1) NOT NULL default ' ',
  "AAABGHYW" varchar(1) NOT NULL default ' ',
  "AAABGHYX" varchar(1) NOT NULL default ' ',
  "AAABGHYY" varchar(1) NOT NULL default ' ',
  "AAABGHYZ" varchar(1) NOT NULL default ' ',
  "AAABGHZA" varchar(1) NOT NULL default ' ',
  "AAABGHZB" varchar(1) NOT NULL default ' ',
  "AAABGHZC" varchar(1) NOT NULL default ' ',
  "AAABGHZD" varchar(1) NOT NULL default ' ',
  "AAABGHZE" varchar(1) NOT NULL default ' ',
  "AAABGHZF" varchar(1) NOT NULL default ' ',
  "AAABGHZG" varchar(1) NOT NULL default ' ',
  "AAABGHZH" varchar(1) NOT NULL default ' ',
  "AAABGHZI" varchar(1) NOT NULL default ' ',
  "AAABGHZJ" varchar(1) NOT NULL default ' ',
  "AAABGHZK" varchar(1) NOT NULL default ' ',
  "AAABGHZL" varchar(1) NOT NULL default ' ',
  "AAABGHZM" varchar(1) NOT NULL default ' ',
  "AAABGHZN" varchar(1) NOT NULL default ' ',
  "AAABGHZO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGHZQ" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGHZU" varchar(1) NOT NULL default ' ',
  "AAABGHZV" varchar(1) NOT NULL default ' ',
  "AAABGHZW" varchar(1) NOT NULL default ' ',
  "AAABGHZX" varchar(1) NOT NULL default ' ',
  "AAABGHZY" varchar(1) NOT NULL default ' ',
  "AAABGHZZ" varchar(1) NOT NULL default ' ',
  "AAABGIAA" varchar(1) NOT NULL default ' ',
  "AAABGIAB" varchar(1) NOT NULL default ' ',
  "AAABGIAC" varchar(1) NOT NULL default ' ',
  "AAABGIAD" varchar(1) NOT NULL default ' ',
  "AAABGIAE" varchar(1) NOT NULL default ' ',
  "AAABGIAF" varchar(1) NOT NULL default ' ',
  "AAABGIAG" varchar(1) NOT NULL default ' ',
  "AAABGIAH" varchar(1) NOT NULL default ' ',
  "AAABGIAI" varchar(1) NOT NULL default ' ',
  "AAABJFMU" varchar(1) NOT NULL default ' ',
  "AAABGIAK" varchar(1) NOT NULL default ' ',
  "AAABGIAL" varchar(1) NOT NULL default ' ',
  "AAABGIAM" varchar(1) NOT NULL default ' ',
  "AAABGIAN" varchar(1) NOT NULL default ' ',
  "AAABGIAO" varchar(1) NOT NULL default ' ',
  "AAABGIAP" varchar(1) NOT NULL default ' ',
  "AAABGIAQ" varchar(1) NOT NULL default ' ',
  "AAABGIAR" varchar(1) NOT NULL default ' ',
  "AAABGIAS" varchar(1) NOT NULL default ' ',
  "AAABGIAT" varchar(1) NOT NULL default ' ',
  "AAABGIAU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIAW" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGIBA" varchar(1) NOT NULL default ' ',
  "AAABGIBB" varchar(1) NOT NULL default ' ',
  "AAABGIBC" varchar(1) NOT NULL default ' ',
  "AAABGIBD" varchar(1) NOT NULL default ' ',
  "AAABGIBE" varchar(1) NOT NULL default ' ',
  "AAABGIBF" varchar(1) NOT NULL default ' ',
  "AAABGIBG" varchar(1) NOT NULL default ' ',
  "AAABGIBH" varchar(1) NOT NULL default ' ',
  "AAABGIBI" varchar(1) NOT NULL default ' ',
  "AAABGIBJ" varchar(1) NOT NULL default ' ',
  "AAABGIBK" varchar(1) NOT NULL default ' ',
  "AAABGIBL" varchar(1) NOT NULL default ' ',
  "AAABGIBM" varchar(1) NOT NULL default ' ',
  "AAABGIBN" varchar(1) NOT NULL default ' ',
  "AAABGIBO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIBQ" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGIBU" varchar(1) NOT NULL default ' ',
  "AAABGIBV" varchar(1) NOT NULL default ' ',
  "AAABGIBW" varchar(1) NOT NULL default ' ',
  "AAABGIBX" varchar(1) NOT NULL default ' ',
  "AAABGIBY" varchar(1) NOT NULL default ' ',
  "AAABGIBZ" varchar(1) NOT NULL default ' ',
  "AAABGICA" varchar(1) NOT NULL default ' ',
  "AAABGICB" varchar(1) NOT NULL default ' ',
  "AAABGICC" varchar(1) NOT NULL default ' ',
  "AAABGICD" varchar(1) NOT NULL default ' ',
  "AAABGICE" varchar(1) NOT NULL default ' ',
  "AAABGICF" varchar(1) NOT NULL default ' ',
  "AAABGICG" varchar(1) NOT NULL default ' ',
  "AAABGICH" varchar(1) NOT NULL default ' ',
  "AAABGICI" varchar(1) NOT NULL default ' ',
  "AAABGICJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGICL" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGICN" varchar(1) NOT NULL default ' ',
  "AAABGICO" varchar(1) NOT NULL default ' ',
  "AAABGICP" varchar(1) NOT NULL default ' ',
  "AAABGICQ" varchar(1) NOT NULL default ' ',
  "AAABGICR" varchar(1) NOT NULL default ' ',
  "AAABGICS" varchar(1) NOT NULL default ' ',
  "AAABGICT" varchar(1) NOT NULL default ' ',
  "AAABGICU" varchar(1) NOT NULL default ' ',
  "AAABGICV" varchar(1) NOT NULL default ' ',
  "AAABGICW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGICY" (
  "AAABGIDW" varchar(14) NOT NULL default ' ',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  "AAABGIDB" varchar(8) NOT NULL default '00000000',
  "AAABGIDC" varchar(50) NOT NULL default ' ',
  "AAABGIDD" varchar(50) NOT NULL default ' ',
  "AAABGIDE" varchar(50) NOT NULL default ' ',
  "AAABGIDF" varchar(50) NOT NULL default ' ',
  "AAABGIDG" varchar(50) NOT NULL default ' ',
  "AAABGIDH" varchar(50) NOT NULL default ' ',
  "AAABGIDI" varchar(50) NOT NULL default ' ',
  "AAABGIDJ" varchar(50) NOT NULL default ' ',
  "AAABGIDK" varchar(50) NOT NULL default ' ',
  "AAABGIDL" varchar(50) NOT NULL default ' ',
  "AAABGIDM" varchar(50) NOT NULL default ' ',
  "AAABGIDN" varchar(50) NOT NULL default ' ',
  "AAABGIDO" varchar(50) NOT NULL default ' ',
  "AAABGIDP" varchar(50) NOT NULL default ' ',
  "AAABGIDQ" varchar(50) NOT NULL default ' ',
  "AAABGIDR" varchar(50) NOT NULL default ' ',
  "AAABGIDS" varchar(50) NOT NULL default ' ',
  "AAABGIDT" varchar(50) NOT NULL default ' ',
  "AAABGIDU" varchar(50) NOT NULL default ' ',
  "AAABGIDV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIDX" (
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABLGEA" varchar(60) NOT NULL default ' ',
  "AAABMMRH" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYD","AAABMLQA","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIEG" (
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLGEA" varchar(60) NOT NULL default ' ',
  "AAABLLBG" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABGOAW" varchar(1) NOT NULL default ' ',
  "AAABGIEM" varchar(1) NOT NULL default ' ',
  "AAABJSQD" varchar(60) NOT NULL default ' ',
  "AAABGIEO" int(11) NOT NULL default '0',
  "AAABGIEP" int(11) NOT NULL default '0',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABMDXM" varchar(80) NOT NULL default ' ',
  "AAABGIET" varchar(12) NOT NULL default ' ',
  "AAABIMEK" varchar(8) NOT NULL default '00000000',
  "AAABIMEL" varchar(6) NOT NULL default '000000',
  "AAABGIEW" varchar(12) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABHMQN" varchar(10) NOT NULL default ' ',
  "AAABGIFA" varchar(1) NOT NULL default ' ',
  "AAABGIFB" varchar(1) NOT NULL default ' ',
  "AAABGIFC" varchar(12) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABGIFF" varchar(30) NOT NULL default '000000000000000000000000000000',
  "AAABGIFG" varchar(30) NOT NULL default '000000000000000000000000000000',
  "AAABGIFH" varchar(1) NOT NULL default ' ',
  "AAABGIFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABGIFN" varchar(1) NOT NULL default ' ',
  "AAABGIFO" varchar(1) NOT NULL default ' ',
  "AAABGIFP" varchar(1) NOT NULL default ' ',
  "AAABGIFQ" varchar(1) NOT NULL default ' ',
  "AAABGIFR" varchar(1) NOT NULL default ' ',
  "AAABGIFS" varchar(1) NOT NULL default ' ',
  "AAABGIFT" varchar(1) NOT NULL default ' ',
  "AAABGIFU" varchar(1) NOT NULL default ' ',
  "AAABGIFV" varchar(1) NOT NULL default ' ',
  "AAABGIFW" varchar(1) NOT NULL default ' ',
  "AAABKZEG" varchar(1) NOT NULL default ' ',
  "AAABGIFY" varchar(1) NOT NULL default ' ',
  "AAABGIFZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNSU" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIGK" (
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  "AAABHDXB" varchar(4) NOT NULL default ' ',
  "AAABGIGN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIGP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHFAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIGV" (
  "AAABGIHD" varchar(8) NOT NULL default ' ',
  "AAABGIGX" varchar(26) NOT NULL default ' ',
  "AAABGIGY" smallint(6) NOT NULL default '0',
  "AAABGIGZ" varchar(8) NOT NULL default ' ',
  "AAABGIHA" smallint(6) NOT NULL default '0',
  "AAABGIHB" smallint(6) NOT NULL default '0',
  "AAABGIHC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKZJ" varchar(12) NOT NULL default '000000000000',
  "AAABGKZK" varchar(4) NOT NULL default '0000',
  "AAABHNRX" varchar(50) NOT NULL default ' ',
  "AAABHYGA" varchar(30) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  "AAABGIHP" varchar(1) NOT NULL default ' ',
  "AAABIIQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKZJ","AAABGKZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABGIHX" varchar(1) NOT NULL default ' ',
  "AAABLLCF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABHANX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABGIIO" varchar(3) NOT NULL default '000',
  "AAABGIIP" decimal(15,2) NOT NULL default '0.00',
  "AAABGIIQ" varchar(1) NOT NULL default ' ',
  "AAABGIIR" varchar(2) NOT NULL default ' ',
  "AAABGIIS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKRHV","AAABLCFN","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABGIJD" varchar(8) NOT NULL default '00000000',
  "AAABLUMI" varchar(8) NOT NULL default '00000000',
  "AAABGIJF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAABGIJN" varchar(1) NOT NULL default ' ',
  "AAABGIJO" varchar(1) NOT NULL default ' ',
  "AAABGIJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRHV","AAABKRXI","AAABLCFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIJU" (
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABGIKI" varchar(21) NOT NULL default ' ',
  "AAABGIKJ" varchar(2) NOT NULL default '00',
  "AAABGIKK" varchar(2) NOT NULL default '00',
  "AAABMLZG" varchar(2) NOT NULL default '00',
  "AAABGIKC" varchar(1) NOT NULL default ' ',
  "AAABGIKD" varchar(1) NOT NULL default '0',
  "AAABGIKE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIKF","AAABMNLX","AAABMNLY","AAABGIKI","AAABGIKJ","AAABGIKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGILE" varchar(14) NOT NULL default ' ',
  "AAABGILF" varchar(14) NOT NULL default ' ',
  "AAABGIOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKRXI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGILE" varchar(14) NOT NULL default ' ',
  "AAABGILF" varchar(14) NOT NULL default ' ',
  "AAABGIOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMCRJ","AAABMCNU","AAABKRXI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGILN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGIMG" varchar(10) NOT NULL default ' ',
  "AAABGIMH" varchar(10) NOT NULL default ' ',
  "AAABGIOR" varchar(1) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKRXI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGIMG" varchar(10) NOT NULL default ' ',
  "AAABGIMH" varchar(10) NOT NULL default ' ',
  "AAABGIOR" varchar(1) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMCRJ","AAABKRXI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKRXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGINC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKRVS" varchar(1) NOT NULL default '0',
  "AAABGINJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMCRJ","AAABKRXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGINO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLECD","AAABMCRJ","AAABKRXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  "AAABGIOE" varchar(1) NOT NULL default ' ',
  "AAABGIOF" varchar(1) NOT NULL default ' ',
  "AAABGIOG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXI" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGIOP" varchar(6) NOT NULL default ' ',
  "AAABGIOQ" varchar(6) NOT NULL default ' ',
  "AAABGIOR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMCRJ","AAABKRXI","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGIPR" varchar(4) NOT NULL default ' ',
  "AAABGIPB" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABGIQS" varchar(3) NOT NULL default '000',
  "AAABGIPY" varchar(1) NOT NULL default ' ',
  "AAABGIQA" varchar(3) NOT NULL default ' ',
  "AAABGIQT" varchar(16) NOT NULL default ' ',
  "AAABGIQX" varchar(1) NOT NULL default ' ',
  "AAABGIQY" varchar(1) NOT NULL default ' ',
  "AAABGIQZ" varchar(3) NOT NULL default ' ',
  "AAABGIRC" varchar(30) NOT NULL default ' ',
  "AAABGIRD" varchar(1) NOT NULL default ' ',
  "AAABGIPM" varchar(1) NOT NULL default ' ',
  "AAABGIPN" decimal(3,0) NOT NULL default '0',
  "AAABGIPO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGIPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGISA" varchar(4) NOT NULL default ' ',
  "AAABGIPW" varchar(1) NOT NULL default ' ',
  "AAABGIPX" varchar(1) NOT NULL default ' ',
  "AAABGIPY" varchar(1) NOT NULL default ' ',
  "AAABGIPZ" varchar(1) NOT NULL default ' ',
  "AAABGIQA" varchar(3) NOT NULL default ' ',
  "AAABGIQS" varchar(3) NOT NULL default '000',
  "AAABGIQT" varchar(16) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABGIQY" varchar(1) NOT NULL default ' ',
  "AAABGIQZ" varchar(3) NOT NULL default ' ',
  "AAABGIQG" varchar(1) NOT NULL default ' ',
  "AAABGIRC" varchar(30) NOT NULL default ' ',
  "AAABGIRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGISA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGISK" varchar(4) NOT NULL default ' ',
  "AAABGIQQ" varchar(1) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABGIQS" varchar(3) NOT NULL default '000',
  "AAABGIQT" varchar(16) NOT NULL default ' ',
  "AAABGIQU" varchar(3) NOT NULL default ' ',
  "AAABHOTF" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGIQX" varchar(1) NOT NULL default ' ',
  "AAABGIQY" varchar(1) NOT NULL default ' ',
  "AAABGIQZ" varchar(3) NOT NULL default ' ',
  "AAABGIRA" varchar(1) NOT NULL default ' ',
  "AAABGIRB" varchar(1) NOT NULL default ' ',
  "AAABGIRC" varchar(30) NOT NULL default ' ',
  "AAABGIRD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGIRQ" varchar(4) NOT NULL default ' ',
  "AAABGISG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABGIRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGISA" varchar(4) NOT NULL default ' ',
  "AAABGISG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABGISA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGISB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGISK" varchar(4) NOT NULL default ' ',
  "AAABGISG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABGISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGISL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIVD" varchar(4) NOT NULL default ' ',
  "AAABGISV" varchar(1) NOT NULL default ' ',
  "AAABGISW" varchar(1) NOT NULL default ' ',
  "AAABGISQ" varchar(1) NOT NULL default ' ',
  "AAABGISR" varchar(1) NOT NULL default ' ',
  "AAABGISS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIVD","AAABGISV","AAABGISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGISX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGIUR" varchar(4) NOT NULL default ' ',
  "AAABGITB" varchar(1) NOT NULL default ' ',
  "AAABGITC" decimal(5,3) NOT NULL default '0.000',
  "AAABGITD" tinyint(3) unsigned NOT NULL default '0',
  "AAABGITE" varchar(1) NOT NULL default ' ',
  "AAABKRQI" varchar(4) NOT NULL default ' ',
  "AAABHPYD" varchar(10) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABKRDV" varchar(4) NOT NULL default ' ',
  "AAABLHMG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGIUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGITN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGITQ" varchar(1) NOT NULL default '0',
  "AAABGITR" varchar(1) NOT NULL default '0',
  "AAABGITS" varchar(1) NOT NULL default '0',
  "AAABGITT" varchar(1) NOT NULL default ' ',
  "AAABGITU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGITX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMDEP","AAABMLDW","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGIUR" varchar(4) NOT NULL default ' ',
  "AAABGIUN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABGIUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIVD" varchar(4) NOT NULL default ' ',
  "AAABGIUV" varchar(1) NOT NULL default ' ',
  "AAABGIUW" varchar(1) NOT NULL default ' ',
  "AAABGIUX" varchar(1) NOT NULL default ' ',
  "AAABGIUY" varchar(1) NOT NULL default ' ',
  "AAABGIUZ" varchar(1) NOT NULL default ' ',
  "AAABGIVA" varchar(1) NOT NULL default ' ',
  "AAABGIVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIVK" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGIVI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIVK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIVM" (
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABKAHC" varchar(8) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABGIWA" varchar(8) NOT NULL default '00000000',
  "AAABGIWB" varchar(6) NOT NULL default '000000',
  "AAABMLPO" varchar(1) NOT NULL default '0',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGKYH" varchar(80) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABGIVW" varchar(175) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUVJ","AAABKAHC","AAABLZKF","AAABGIWA","AAABGIWB","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIWD" (
  "AAABGIWJ" varchar(1) NOT NULL default ' ',
  "AAABGIWK" varchar(1) NOT NULL default ' ',
  "AAABLEJH" varchar(60) NOT NULL default ' ',
  "AAABGIWH" varchar(14) NOT NULL default '00000000000000',
  "AAABLEQO" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABGIWJ","AAABGIWK","AAABLEJH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKZJ" varchar(12) NOT NULL default '000000000000',
  "AAABGKZK" varchar(4) NOT NULL default '0000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABGIWR" varchar(1) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKZJ","AAABGKZK","AAABLXUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABGIXN" varchar(2) NOT NULL default ' ',
  "AAABGIXO" varchar(2) NOT NULL default ' ',
  "AAABIFIX" varchar(10) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABLAOA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLDAZ","AAABLCAK","AAABGIXN","AAABGIXO","AAABIFIX","AAABLIMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIXR" (
  "AAABGIXZ" varchar(3) NOT NULL default '000',
  "AAABGIXT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIXV" (
  "AAABGIXZ" varchar(3) NOT NULL default '000',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIXZ","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABGIYF" varchar(4) NOT NULL default ' ',
  "AAABGIYG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIYX" varchar(10) NOT NULL default ' ',
  "AAABGIYN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGIYX" varchar(10) NOT NULL default ' ',
  "AAABGIYU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGIYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIZL" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(1) NOT NULL default ' ',
  "AAABMCWT" varchar(1) NOT NULL default ' ',
  "AAABGIZI" varchar(1) NOT NULL default ' ',
  "AAABLKLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYMH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLYMH" varchar(10) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLYMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGIZZ" (
  "AAABHBUM" varchar(4) NOT NULL default ' ',
  "AAABHBUL" varchar(4) NOT NULL default ' ',
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  "AAABGJQA" varchar(32) NOT NULL default ' ',
  "AAABGJQB" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABGJQE" varchar(10) NOT NULL default '0000000000',
  "AAABIJJA" varchar(4) NOT NULL default ' ',
  "AAABGJPR" varchar(2) NOT NULL default ' ',
  "AAABGJPS" varchar(1) NOT NULL default ' ',
  "AAABGJPT" varchar(6) NOT NULL default ' ',
  "AAABGJPU" int(11) NOT NULL default '0',
  "AAABGJPV" int(11) NOT NULL default '0',
  "AAABGWQU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABHBUM","AAABHBUL","AAABGWQV","AAABGJQA","AAABGJQB","AAABLPJD","AAABGJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJBM" varchar(12) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABGJBY" varchar(7) NOT NULL default ' ',
  "AAABGJBB" varchar(7) NOT NULL default ' ',
  "AAABGJBC" varchar(7) NOT NULL default ' ',
  "AAABKQHN" decimal(13,3) NOT NULL default '0.000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKNQN" varchar(4) NOT NULL default ' ',
  "AAABGJLF" varchar(1) NOT NULL default ' ',
  "AAABGJLG" varchar(1) NOT NULL default ' ',
  "AAABGJLH" varchar(1) NOT NULL default ' ',
  "AAABGJLI" varchar(1) NOT NULL default ' ',
  "AAABGJLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJBM","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJBY" varchar(7) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABGJBT" varchar(1) NOT NULL default ' ',
  "AAABGJBU" varchar(1) NOT NULL default ' ',
  "AAABGJBV" varchar(4) NOT NULL default ' ',
  "AAABGJBW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJBZ" (
  "AAABGJCH" varchar(22) NOT NULL default ' ',
  "AAABGJCB" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJCD" (
  "AAABGJCH" varchar(22) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJCH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJCJ" (
  "AAABGJCL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJJI" varchar(10) NOT NULL default '0000000000',
  "AAABGJDS" varchar(8) NOT NULL default '00000000',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default ' ',
  "AAABGJIH" varchar(4) NOT NULL default '0000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABKXHP" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABGJHJ" varchar(1) NOT NULL default ' ',
  "AAABKXIN" varchar(8) NOT NULL default '00000000',
  "AAABKXIO" decimal(13,2) NOT NULL default '0.00',
  "AAABKXIP" varchar(5) NOT NULL default ' ',
  "AAABKXIQ" decimal(13,2) NOT NULL default '0.00',
  "AAABGJDE" varchar(5) NOT NULL default ' ',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABGJDG" decimal(13,2) NOT NULL default '0.00',
  "AAABGJDH" varchar(5) NOT NULL default ' ',
  "AAABGJHN" varchar(4) NOT NULL default ' ',
  "AAABKTUF" varchar(15) NOT NULL default ' ',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABKXMN" varchar(12) NOT NULL default ' ',
  "AAABKXMO" varchar(8) NOT NULL default '00000000',
  "AAABKXMP" varchar(6) NOT NULL default '000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJJI","AAABGJDS","AAABKXML","AAABKXMM","AAABGJIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJHO" varchar(15) NOT NULL default ' ',
  "AAABGJEA" varchar(15) NOT NULL default ' ',
  "AAABGJEB" varchar(22) NOT NULL default ' ',
  "AAABGJEC" varchar(1) NOT NULL default ' ',
  "AAABGJED" varchar(8) NOT NULL default '00000000',
  "AAABGSOH" varchar(2) NOT NULL default ' ',
  "AAABGJEF" varchar(1) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABLAXL" varchar(4) NOT NULL default '0000',
  "AAABLAXM" varchar(1) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJHO"),
  KEY "AAABGJEU" ("AAABLAXK"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJHO" varchar(15) NOT NULL default ' ',
  "AAABLHFC" varchar(5) NOT NULL default '00000',
  "AAABGJJI" varchar(10) NOT NULL default '0000000000',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJHO","AAABLHFC","AAABGJJI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJHO" varchar(15) NOT NULL default ' ',
  "AAABLHFC" varchar(5) NOT NULL default '00000',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABGJFY" varchar(1) NOT NULL default ' ',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABGTKB" varchar(4) NOT NULL default ' ',
  "AAABLAXF" varchar(8) NOT NULL default ' ',
  "AAABGJGK" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABGJGM" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABKZEO" varchar(10) NOT NULL default ' ',
  "AAABKZEP" varchar(4) NOT NULL default '0000',
  "AAABKZEQ" varchar(10) NOT NULL default ' ',
  "AAABKZER" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJHO","AAABLHFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJJI" varchar(10) NOT NULL default '0000000000',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABGJIH" varchar(4) NOT NULL default '0000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABGJHJ" varchar(1) NOT NULL default ' ',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABLAPD" varchar(1) NOT NULL default ' ',
  "AAABLAPE" varchar(2) NOT NULL default ' ',
  "AAABGJHN" varchar(4) NOT NULL default ' ',
  "AAABGJHO" varchar(15) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABGJHV" varchar(22) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJJI","AAABLCIO","AAABLCIP","AAABGJIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJJI" varchar(10) NOT NULL default '0000000000',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABGJIQ" varchar(6) NOT NULL default '000000',
  "AAABGJIR" varchar(8) NOT NULL default '00000000',
  "AAABGJIS" varchar(1) NOT NULL default ' ',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABLADK" varchar(1) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGJJI"),
  KEY "AAABGJJJ" ("AAABKZRY"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLAXF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLAYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABGJKH" varchar(10) NOT NULL default ' ',
  "AAABGJKI" varchar(4) NOT NULL default ' ',
  "AAABGJKJ" varchar(3) NOT NULL default ' ',
  "AAABGJKK" varchar(9) NOT NULL default ' ',
  "AAABGJKL" varchar(12) NOT NULL default ' ',
  "AAABGJKM" varchar(25) NOT NULL default ' ',
  "AAABGJKN" decimal(13,3) NOT NULL default '0.000',
  "AAABGJKO" varchar(3) NOT NULL default ' ',
  "AAABGJKP" varchar(10) NOT NULL default ' ',
  "AAABGJKQ" varchar(4) NOT NULL default ' ',
  "AAABGJKR" varchar(3) NOT NULL default ' ',
  "AAABGJKS" varchar(9) NOT NULL default ' ',
  "AAABGJKT" varchar(12) NOT NULL default ' ',
  "AAABGJKU" varchar(25) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAABKNPL" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABKNPD" varchar(18) NOT NULL default ' ',
  "AAABKNQK" varchar(18) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABKNQN" varchar(4) NOT NULL default ' ',
  "AAABGJLF" varchar(1) NOT NULL default ' ',
  "AAABGJLG" varchar(1) NOT NULL default ' ',
  "AAABGJLH" varchar(1) NOT NULL default ' ',
  "AAABGJLI" varchar(1) NOT NULL default ' ',
  "AAABGJLJ" varchar(1) NOT NULL default ' ',
  "AAABGJLK" varchar(1) NOT NULL default ' ',
  "AAABGJLL" varchar(1) NOT NULL default ' ',
  "AAABGJLM" varchar(1) NOT NULL default ' ',
  "AAABGJLN" varchar(1) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABGJLT" varchar(2) NOT NULL default ' ',
  "AAABGJLU" varchar(4) NOT NULL default ' ',
  "AAABGJLV" varchar(18) NOT NULL default ' ',
  "AAABGJLW" varchar(18) NOT NULL default ' ',
  "AAABGJLX" varchar(4) NOT NULL default ' ',
  "AAABGJLY" varchar(10) NOT NULL default ' ',
  "AAABGJLZ" varchar(10) NOT NULL default ' ',
  "AAABGJMA" varchar(10) NOT NULL default ' ',
  "AAABGJMB" varchar(4) NOT NULL default ' ',
  "AAABGJMC" varchar(3) NOT NULL default ' ',
  "AAABGJMD" varchar(8) NOT NULL default '00000000',
  "AAABGJME" varchar(1) NOT NULL default ' ',
  "AAABGJMF" varchar(1) NOT NULL default ' ',
  "AAABGJMG" varchar(1) NOT NULL default ' ',
  "AAABGJMH" varchar(1) NOT NULL default ' ',
  "AAABGJMI" varchar(1) NOT NULL default ' ',
  "AAABGJMJ" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABGJML" varchar(1) NOT NULL default ' ',
  "AAABGJMM" varchar(2) NOT NULL default ' ',
  "AAABGJMN" varchar(2) NOT NULL default ' ',
  "AAABGOYB" varchar(4) NOT NULL default ' ',
  "AAABGJMP" varchar(8) NOT NULL default '00000000',
  "AAABGJMQ" varchar(2) NOT NULL default ' ',
  "AAABGJMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJMW" varchar(6) NOT NULL default '000000',
  "AAABGJMX" varchar(8) NOT NULL default '00000000',
  "AAABGJMY" varchar(4) NOT NULL default ' ',
  "AAABGJMZ" varchar(6) NOT NULL default '000000',
  "AAABGJNA" varchar(8) NOT NULL default '00000000',
  "AAABGJNB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJND" (
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  "AAABGJQE" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABGWQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJNH" (
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABHBUL" varchar(4) NOT NULL default ' ',
  "AAABGJNK" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABHCDU" varchar(6) NOT NULL default '000000',
  "AAABGJNN" varchar(8) NOT NULL default '00000000',
  "AAABGJNO" varchar(6) NOT NULL default '000000',
  "AAABGJPW" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHBUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJNT" (
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABHBUL" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHBUL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGVLV" varchar(4) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGVLV","AAABJGWB","AAABJGWE","AAABJGWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABLTVR" VARBINARY(255) NOT NULL,
  PRIMARY KEY ("AAABMRIL","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJOS" (
  "AAABGJPB" varchar(12) NOT NULL default ' ',
  "AAABHBUM" varchar(4) NOT NULL default ' ',
  "AAABHBUL" varchar(4) NOT NULL default ' ',
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  "AAABGJQA" varchar(32) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABGJPA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJPC" (
  "AAABKQPS" decimal(21,7) NOT NULL default '0.0000000',
  "AAABGJPG" decimal(21,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABKQPS","AAABGJPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJPH" (
  "AAABLTXI" varchar(10) NOT NULL default '0000000000',
  "AAABHBUL" varchar(4) NOT NULL default ' ',
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  "AAABGJQA" varchar(32) NOT NULL default ' ',
  "AAABGJQB" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(4) NOT NULL default ' ',
  "AAABHBUM" varchar(4) NOT NULL default ' ',
  "AAABGJQE" varchar(10) NOT NULL default '0000000000',
  "AAABIJJA" varchar(4) NOT NULL default ' ',
  "AAABGJPR" varchar(2) NOT NULL default ' ',
  "AAABGJPS" varchar(1) NOT NULL default ' ',
  "AAABGJPT" varchar(6) NOT NULL default ' ',
  "AAABGJPU" int(11) NOT NULL default '0',
  "AAABGJPV" int(11) NOT NULL default '0',
  "AAABGJPW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLTXI","AAABHBUL","AAABGWQV","AAABGJQA","AAABGJQB","AAABLPJD","AAABHBUM","AAABGJQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABJQPI" varchar(3) NOT NULL default '000',
  "AAABJQPD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX","AAABJQPI","AAABJQPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABJQPI" varchar(3) NOT NULL default '000',
  "AAABJQPD" varchar(4) NOT NULL default ' ',
  "AAABJQPE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMRHS","AAABLCPX","AAABJQPI","AAABJQPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJSK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGJSK" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGJSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJSJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGJSJ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGJSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJSJ" varchar(4) NOT NULL default ' ',
  "AAABGJSK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGJSJ","AAABGJSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHEWW" varchar(4) NOT NULL default ' ',
  "AAABGJSX" varchar(4) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABGJSR" varchar(1) NOT NULL default ' ',
  "AAABGJSS" decimal(4,1) NOT NULL default '0.0',
  "AAABGJST" decimal(4,1) NOT NULL default '0.0',
  "AAABGJSU" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHEWW","AAABGJSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGJTI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMREF"),
  UNIQUE KEY "AAABGJTG" ("AAABMRIL","AAABGJTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJTJ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(20) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABGJVA" varchar(4) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGJUJ" varchar(1) NOT NULL default ' ',
  "AAABGJUK" varchar(2) NOT NULL default ' ',
  "AAABGJUL" varchar(1) NOT NULL default ' ',
  "AAABGJUM" varchar(1) NOT NULL default ' ',
  "AAABGJUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABGJVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABGJVA" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABGJVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(50) NOT NULL default ' ',
  "AAABHZUM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABHZUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJVI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABMKIQ" varchar(1) NOT NULL default ' ',
  "AAABGJVM" varchar(1) NOT NULL default ' ',
  "AAABGJVN" varchar(1) NOT NULL default ' ',
  "AAABGJVO" varchar(1) NOT NULL default ' ',
  "AAABGJWU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJVS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJVY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGJYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJWG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABGJWJ" varchar(30) NOT NULL default ' ',
  "AAABGJWK" varchar(30) NOT NULL default ' ',
  "AAABGJWL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJWO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default '0',
  "AAABHSKS" varchar(1) NOT NULL default ' ',
  "AAABGJWU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD","AAABGJYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJWY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK","AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJXG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGJXW" varchar(3) NOT NULL default '000',
  "AAABGJXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK","AAABLECD","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJXR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGJXW" varchar(3) NOT NULL default '000',
  "AAABGJXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK","AAABLECD","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJYC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGJYK" varchar(6) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGJYK","AAABLECD","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKPR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJYS" (
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGJZC" varchar(5) NOT NULL default ' ',
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGJZC","AAABKWJY","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJZF" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABLSBE" varchar(32) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABGJZO" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(6) NOT NULL default ' ',
  "AAABHECM" tinyint(3) unsigned NOT NULL default '0',
  "AAABGJZR" varchar(1) NOT NULL default ' ',
  "AAABGJZS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGJZV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKAB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKPR" varchar(10) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  "AAABGKOP" varchar(4) NOT NULL default '0000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGKAS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKPR","AAABGKPS","AAABGKOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMYW" varchar(3) NOT NULL default ' ',
  "AAABGKBB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMMYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABKNSR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKBL" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKBQ" (
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKBT" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKBY" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXQJ" varchar(2) NOT NULL default '00',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLXIY" varchar(14) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLXIY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKCX" (
  "AAABLTYE" varchar(12) NOT NULL default ' ',
  "AAABLTYF" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABGKDB" varchar(8) NOT NULL default '00000000',
  "AAABGKDC" varchar(1) NOT NULL default ' ',
  "AAABMAYH" varchar(4) NOT NULL default ' ',
  "AAABMAYI" varchar(3) NOT NULL default '000',
  "AAABMAYJ" varchar(12) NOT NULL default ' ',
  "AAABMAYK" varchar(68) NOT NULL default ' ',
  "AAABMAYL" varchar(1) NOT NULL default ' ',
  "AAABMAYM" varchar(1) NOT NULL default ' ',
  "AAABMAYN" varchar(1) NOT NULL default ' ',
  "AAABMAYO" varchar(1) NOT NULL default '0',
  "AAABMAYP" int(11) NOT NULL default '0',
  "AAABMAYQ" int(11) NOT NULL default '0',
  "AAABMAYR" varchar(16) NOT NULL default ' ',
  "AAABMAYS" varchar(1) NOT NULL default ' ',
  "AAABMAYT" varchar(1) NOT NULL default ' ',
  "AAABMAYU" varchar(12) NOT NULL default ' ',
  "AAABMAYV" varchar(12) NOT NULL default ' ',
  "AAABMAYW" varchar(12) NOT NULL default ' ',
  "AAABMAYX" varchar(6) NOT NULL default ' ',
  "AAABMAYY" varchar(12) NOT NULL default ' ',
  "AAABMAYZ" varchar(1) NOT NULL default ' ',
  "AAABMAZA" varchar(1) NOT NULL default ' ',
  "AAABMAZB" varchar(1) NOT NULL default '0',
  "AAABMAZC" varchar(1) NOT NULL default ' ',
  "AAABMAZE" int(11) NOT NULL default '0',
  "AAABGKEA" smallint(6) NOT NULL default '0',
  "AAABLEEI" text,
  PRIMARY KEY ("AAABLTYE","AAABLTYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKPR" varchar(10) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKPR","AAABGKPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHAZS" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(10) NOT NULL default ' ',
  "AAABGKEV" varchar(6) NOT NULL default '000000',
  "AAABGKEQ" varchar(6) NOT NULL default '000000',
  "AAABGKER" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHAZS","AAABMOIB","AAABGKEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHELQ" varchar(2) NOT NULL default ' ',
  "AAABHELR" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABHELU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKJQ","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHEJU" varchar(30) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGKJQ","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  "AAABGKGF" varchar(1) NOT NULL default ' ',
  "AAABGKGG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE","AAABLILC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE","AAABLILC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKHZ" varchar(1) NOT NULL default ' ',
  "AAABGKIH" varchar(1) NOT NULL default ' ',
  "AAABGKHN" varchar(5) NOT NULL default ' ',
  "AAABGKHO" decimal(9,5) NOT NULL default '0.00000',
  "AAABHHTV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKHZ" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKIH" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGKIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAABGKIU" varchar(1) NOT NULL default ' ',
  "AAABIJZM" varchar(1) NOT NULL default ' ',
  "AAABHGYT" varchar(1) NOT NULL default ' ',
  "AAABGKIX" varchar(7) NOT NULL default ' ',
  "AAABGKIY" varchar(10) NOT NULL default ' ',
  "AAABGKIZ" varchar(1) NOT NULL default ' ',
  "AAABGKJA" varchar(1) NOT NULL default ' ',
  "AAABGKJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGKJQ" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGKJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHELQ" varchar(2) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABGKJW" varchar(1) NOT NULL default ' ',
  "AAABLHZN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHELQ","AAABMAMY","AAABMQIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHELQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHELQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHELQ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHELQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKKP" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABGKKS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKKV" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABGKKY" varchar(1) NOT NULL default ' ',
  "AAABLELN" varchar(12) NOT NULL default ' ',
  "AAABGKLA" varchar(20) NOT NULL default ' ',
  "AAABGKLB" varchar(20) NOT NULL default ' ',
  "AAABGKLC" varchar(10) NOT NULL default ' ',
  "AAABGKLD" varchar(8) NOT NULL default '00000000',
  "AAABMKKB" varchar(8) NOT NULL default '00000000',
  "AAABILHK" varchar(4) NOT NULL default ' ',
  "AAABGKLG" varchar(1) NOT NULL default ' ',
  "AAABGKLH" varchar(32) NOT NULL default ' ',
  "AAABGKLI" varchar(4) NOT NULL default ' ',
  "AAABGKLJ" varchar(3) NOT NULL default '000',
  "AAABGKLK" varchar(30) NOT NULL default ' ',
  "AAABGKLL" varchar(20) NOT NULL default ' ',
  "AAABGKLM" varchar(1) NOT NULL default ' ',
  "AAABGKLN" varchar(1) NOT NULL default ' ',
  "AAABGKLO" varchar(1) NOT NULL default ' ',
  "AAABGKLP" varchar(1) NOT NULL default ' ',
  "AAABGKLQ" varchar(10) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHEGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKLY" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABHEGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKMD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHEGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKMJ" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABLDLX" varchar(32) NOT NULL default ' ',
  "AAABGKMM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABLDLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKMP" (
  "AAABLDLX" varchar(32) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABLDLX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKMV" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  "AAABMMJT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABMLPV","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABGKNF" varchar(6) NOT NULL default ' ',
  "AAABGKNG" varchar(1) NOT NULL default ' ',
  "AAABGKNH" varchar(1) NOT NULL default ' ',
  "AAABGKNI" varchar(1) NOT NULL default ' ',
  "AAABGKNJ" varchar(1) NOT NULL default ' ',
  "AAABGKNK" varchar(1) NOT NULL default ' ',
  "AAABGKNL" varchar(1) NOT NULL default ' ',
  "AAABGKNM" varchar(1) NOT NULL default ' ',
  "AAABKUSD" varchar(1) NOT NULL default ' ',
  "AAABGKNO" varchar(1) NOT NULL default ' ',
  "AAABGKNP" varchar(1) NOT NULL default ' ',
  "AAABGKNQ" varchar(1) NOT NULL default ' ',
  "AAABGKNR" varchar(1) NOT NULL default ' ',
  "AAABGKNS" varchar(1) NOT NULL default ' ',
  "AAABGKNT" varchar(1) NOT NULL default ' ',
  "AAABGKNU" varchar(1) NOT NULL default ' ',
  "AAABGKNV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKPR" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGKOP" varchar(4) NOT NULL default '0000',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKPR","AAABMQDU","AAABGKOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKOQ" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABJOTS" varchar(60) NOT NULL default ' ',
  "AAABGKOU" varchar(4) NOT NULL default '0000',
  "AAABLKIV" varchar(40) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABLQGL" varchar(100) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHVKD" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABGKPE" varchar(1) NOT NULL default ' ',
  "AAABGKPF" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABIRUN","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKPR" varchar(10) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  "AAABGKPO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGKPR","AAABGKPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABIMUR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHJ","AAABLHPM","AAABLUNJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABHNLR" varchar(4) NOT NULL default ' ',
  "AAABGKQO" varchar(1) NOT NULL default ' ',
  "AAABGKQP" varchar(1) NOT NULL default ' ',
  "AAABGKQQ" varchar(1) NOT NULL default ' ',
  "AAABGKQR" varchar(1) NOT NULL default ' ',
  "AAABGKQS" varchar(1) NOT NULL default ' ',
  "AAABGKQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKQPE","AAABHNLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQPE" varchar(2) NOT NULL default '00',
  "AAABHNLR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQPE","AAABHNLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKRH" (
  "AAABHCXI" varchar(20) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABHCXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKRN" (
  "AAABHCXI" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(40) NOT NULL default ' ',
  "AAABMNNA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCXI","AAABMKFH","AAABMRCU","AAABMKXW","AAABMNMZ","AAABMNNA"),
  KEY "AAABGKSA" ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKSC" (
  "AAABHCXI" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMUR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABHCXI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKSI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDAZ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLCPX","AAABLDAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKSQ" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABMLPO" int(11) NOT NULL default '0',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHRWW" varchar(2) NOT NULL default '00',
  "AAABMKWZ" varchar(32) NOT NULL default ' ',
  "AAABHZOJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABMLPO","AAABMRHK","AAABHRWW","AAABMKWZ"),
  KEY "AAABGKTC" ("AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGKTH" varchar(1) NOT NULL default ' ',
  "AAABGKTI" varchar(1) NOT NULL default ' ',
  "AAABGKTJ" varchar(1) NOT NULL default ' ',
  "AAABGKTK" varchar(1) NOT NULL default ' ',
  "AAABGKTL" varchar(1) NOT NULL default ' ',
  "AAABGKTM" varchar(1) NOT NULL default ' ',
  "AAABGKTN" varchar(1) NOT NULL default ' ',
  "AAABGKTO" varchar(1) NOT NULL default ' ',
  "AAABGKTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKTS" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABGKTX" varchar(32) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABGKTX","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKTZ" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABIZCB" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABGNLC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABIYCS","AAABIZCB","AAABMIRX","AAABMIRY","AAABGKUN","AAABMDEP","AAABGNLC"),
  KEY "AAABGKUQ" ("AAABMIRX","AAABMIRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKUT" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABGKVA" varchar(10) NOT NULL default ' ',
  "AAABHVEK" varchar(1) NOT NULL default ' ',
  "AAABMLCC" varchar(1) NOT NULL default ' ',
  "AAABGKUY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABGKVA"),
  KEY "AAABGKVB" ("AAABHVEK"),
  KEY "AAABGKVD" ("AAABMLCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKVF" (
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAABGKVI" varchar(30) NOT NULL default ' ',
  "AAABGPKK" varchar(1) NOT NULL default ' ',
  "AAABGKVK" varchar(1) NOT NULL default ' ',
  "AAABGKVL" varchar(1) NOT NULL default ' ',
  "AAABGKVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKVO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGKXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKVV" (
  "AAABGKVZ" varchar(1) NOT NULL default ' ',
  "AAABGKWA" varchar(24) NOT NULL default ' ',
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKVZ","AAABGKWA","AAABGKXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKWC" (
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKXE","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKWK" (
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABGKXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKXE","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  "AAABMMYX" varchar(8) NOT NULL default ' ',
  "AAABGKXC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKXE","AAABLYTE","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKLZ" varchar(30) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKLZ","AAABMCVI","AAABKQVZ","AAABLXQQ","AAABKRTQ"),
  KEY "AAABGKXY" ("AAABMRIL","AAABLXQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKYB" (
  "AAABHFCH" varchar(20) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABGKYU" varchar(3) NOT NULL default ' ',
  "AAABMKFU" varchar(20) NOT NULL default ' ',
  "AAABGKYG" varchar(1) NOT NULL default ' ',
  "AAABGKYH" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGKYJ" varchar(8) NOT NULL default ' ',
  "AAABGKYK" varchar(20) NOT NULL default ' ',
  "AAABGKYL" varchar(3) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABKNBI" varchar(73) NOT NULL default ' ',
  "AAABLEJH" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFCH","AAABILWI","AAABGKYU","AAABMKFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKZJ" varchar(12) NOT NULL default '000000000000',
  "AAABGKZK" varchar(4) NOT NULL default '0000',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABGKZP" varchar(5) NOT NULL default '00000',
  "AAABGKZC" varchar(1) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABGKZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKZJ","AAABGKZK","AAABLXUH","AAABGKZP"),
  KEY "AAABGKZN" ("AAABLXUH","AAABGKZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKZQ" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKZT" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGKZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABGLAC" int(11) NOT NULL default '0',
  "AAABGLCK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGLPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGLAL" varchar(1) NOT NULL default ' ',
  "AAABGLAM" varchar(1) NOT NULL default ' ',
  "AAABGLAN" varchar(1) NOT NULL default '0',
  "AAABGLAO" int(11) NOT NULL default '0',
  "AAABGLAP" varchar(3) NOT NULL default ' ',
  "AAABGLAQ" varchar(2) NOT NULL default '00',
  "AAABGLIK" varchar(1) NOT NULL default '0',
  "AAABGLIL" int(11) NOT NULL default '0',
  "AAABGLIM" varchar(3) NOT NULL default ' ',
  "AAABGLIN" varchar(2) NOT NULL default '00',
  "AAABGLIO" varchar(1) NOT NULL default '0',
  "AAABGLIP" int(11) NOT NULL default '0',
  "AAABGLIQ" varchar(3) NOT NULL default ' ',
  "AAABGLIR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGLPW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLID" varchar(32) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABGLBW" varchar(2) NOT NULL default ' ',
  "AAABGLBX" varchar(4) NOT NULL default '0000',
  "AAABGLBY" varchar(1) NOT NULL default ' ',
  "AAABGLBZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABGLCG" varchar(1) NOT NULL default '0',
  "AAABGLCH" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLCI" varchar(1) NOT NULL default '0',
  "AAABGLCJ" varchar(6) NOT NULL default '000000',
  "AAABGLCK" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGLPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLID" varchar(32) NOT NULL default ' ',
  "AAABLJJO" varchar(5) NOT NULL default ' ',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABGLCZ" varchar(4) NOT NULL default ' ',
  "AAABGLDA" varchar(1) NOT NULL default ' ',
  "AAABGLDB" varchar(1) NOT NULL default ' ',
  "AAABGLDC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLID" varchar(32) NOT NULL default ' ',
  "AAABGLDI" varchar(20) NOT NULL default ' ',
  "AAABGLDJ" varchar(20) NOT NULL default ' ',
  "AAABGLDK" varchar(20) NOT NULL default ' ',
  "AAABGLDL" varchar(20) NOT NULL default ' ',
  "AAABGLDM" varchar(20) NOT NULL default ' ',
  "AAABGLDN" varchar(20) NOT NULL default ' ',
  "AAABGLDO" varchar(20) NOT NULL default ' ',
  "AAABGLDP" varchar(20) NOT NULL default ' ',
  "AAABGLDQ" varchar(20) NOT NULL default ' ',
  "AAABGLDR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLID" varchar(32) NOT NULL default ' ',
  "AAABGLDX" varchar(6) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABGLEG" varchar(10) NOT NULL default ' ',
  "AAABGLEH" varchar(4) NOT NULL default ' ',
  "AAABGLEI" varchar(4) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLID" varchar(32) NOT NULL default ' ',
  "AAABGLEV" varchar(2) NOT NULL default ' ',
  "AAABGLEW" varchar(8) NOT NULL default '00000000',
  "AAABGLEX" varchar(20) NOT NULL default ' ',
  "AAABKOUH" varchar(40) NOT NULL default ' ',
  "AAABGLEZ" varchar(1) NOT NULL default ' ',
  "AAABKQQE" varchar(10) NOT NULL default '0000000000',
  "AAABKQQF" varchar(8) NOT NULL default '00000000',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABGLFD" varchar(8) NOT NULL default '00000000',
  "AAABGLFE" varchar(8) NOT NULL default '00000000',
  "AAABGLFF" varchar(20) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABGLFJ" varchar(2) NOT NULL default ' ',
  "AAABHEZC" varchar(10) NOT NULL default ' ',
  "AAABGLFL" varchar(2) NOT NULL default '00',
  "AAABGLFM" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMGOZ" varchar(6) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABGLFQ" varchar(4) NOT NULL default ' ',
  "AAABKQHD" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABGLFT" varchar(4) NOT NULL default ' ',
  "AAABHHST" varchar(8) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABGLFZ" varchar(8) NOT NULL default ' ',
  "AAABGLGA" varchar(2) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKQJY" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABKQJX" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIS" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIT" varchar(3) NOT NULL default ' ',
  "AAABGLGI" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGJ" varchar(3) NOT NULL default ' ',
  "AAABGLGK" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGL" varchar(3) NOT NULL default ' ',
  "AAABGLGM" varchar(6) NOT NULL default '000000',
  "AAABGLGN" varchar(6) NOT NULL default '000000',
  "AAABKQIU" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIV" varchar(3) NOT NULL default ' ',
  "AAABGLGQ" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGR" varchar(3) NOT NULL default ' ',
  "AAABGLGS" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGT" varchar(3) NOT NULL default ' ',
  "AAABGLGU" varchar(6) NOT NULL default '000000',
  "AAABGLGV" varchar(6) NOT NULL default '000000',
  "AAABKQIW" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIX" varchar(3) NOT NULL default ' ',
  "AAABGLGY" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGZ" varchar(3) NOT NULL default ' ',
  "AAABGLHA" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHB" varchar(3) NOT NULL default ' ',
  "AAABGLHC" varchar(6) NOT NULL default '000000',
  "AAABGLHD" varchar(6) NOT NULL default '000000',
  "AAABKQIY" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIZ" varchar(3) NOT NULL default ' ',
  "AAABGLHG" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHH" varchar(3) NOT NULL default ' ',
  "AAABGLHI" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHJ" varchar(3) NOT NULL default ' ',
  "AAABGLHK" varchar(6) NOT NULL default '000000',
  "AAABGLHL" varchar(6) NOT NULL default '000000',
  "AAABKQJA" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJB" varchar(3) NOT NULL default ' ',
  "AAABGLHO" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHP" varchar(3) NOT NULL default ' ',
  "AAABGLHQ" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHR" varchar(3) NOT NULL default ' ',
  "AAABGLHS" varchar(6) NOT NULL default '000000',
  "AAABGLHT" varchar(6) NOT NULL default '000000',
  "AAABKQJC" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJD" varchar(3) NOT NULL default ' ',
  "AAABGLHW" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHX" varchar(3) NOT NULL default ' ',
  "AAABGLHY" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHZ" varchar(3) NOT NULL default ' ',
  "AAABGLIA" varchar(6) NOT NULL default '000000',
  "AAABGLIB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGLID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGLIJ" varchar(1) NOT NULL default '0',
  "AAABGLIK" varchar(1) NOT NULL default '0',
  "AAABGLIL" int(11) NOT NULL default '0',
  "AAABGLIM" varchar(1) NOT NULL default '0',
  "AAABGLIN" varchar(2) NOT NULL default '00',
  "AAABGLIO" varchar(1) NOT NULL default '0',
  "AAABGLIP" int(11) NOT NULL default '0',
  "AAABGLIQ" varchar(1) NOT NULL default '0',
  "AAABGLIR" varchar(2) NOT NULL default '00',
  "AAABGLIS" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLIT" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLIU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGLPW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGLJD" varchar(1) NOT NULL default ' ',
  "AAABGLJE" varchar(8) NOT NULL default '00000000',
  "AAABGLJF" varchar(8) NOT NULL default '00000000',
  "AAABGLJG" varchar(8) NOT NULL default '00000000',
  "AAABGLJH" varchar(1) NOT NULL default ' ',
  "AAABGLJI" varchar(1) NOT NULL default ' ',
  "AAABGLJJ" varchar(1) NOT NULL default ' ',
  "AAABGLJK" varchar(1) NOT NULL default ' ',
  "AAABGLJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLPW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLKB" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGLKB" varchar(5) NOT NULL default ' ',
  "AAABGLJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGLKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  "AAABGLMY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(30) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLMO" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLMU" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  "AAABGLMY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABGLTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLNB" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(30) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABGLTB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLNJ" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABGLTB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLNR" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLNX" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLOF" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLOM" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJYFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLOR" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJYFV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZIB" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLZIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLPW" varchar(8) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGLPS" varchar(1) NOT NULL default ' ',
  "AAABGLPT" varchar(15) NOT NULL default ' ',
  "AAABGLPU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLPW","AAABMEGE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHCXI" varchar(21) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHCXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(30) NOT NULL default ' ',
  "AAABGLSO" varchar(1) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABGLSQ" varchar(30) NOT NULL default ' ',
  "AAABKMWB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABGLTB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAABJLTX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLSE" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABJYFV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABGLTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLSK" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABIVVF" varchar(30) NOT NULL default ' ',
  "AAABGLSO" varchar(1) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABGLSQ" varchar(30) NOT NULL default ' ',
  "AAABKMWB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGMEA","AAABGLTB","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLSV" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABGLTB" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABGLTB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLTD" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLTJ" (
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  "AAABJLTX" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMEA","AAABJLTX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGLTU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLUF" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMDU" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJM" varchar(3) NOT NULL default '000',
  "AAABGLYC" varchar(1) NOT NULL default ' ',
  "AAABGLYD" varchar(1) NOT NULL default ' ',
  "AAABGLYE" varchar(1) NOT NULL default ' ',
  "AAABGMJE" varchar(30) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABGLUW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMDU","AAABGMGR","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMDU" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABGLYO" varchar(3) NOT NULL default '000',
  "AAABLRJM" varchar(3) NOT NULL default '000',
  "AAABGLYC" varchar(1) NOT NULL default ' ',
  "AAABGLYD" varchar(1) NOT NULL default ' ',
  "AAABGLYE" varchar(1) NOT NULL default ' ',
  "AAABGMJE" varchar(30) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMDU","AAABGMGR","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMDU" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGLVY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMDU","AAABGMGR","AAABMREF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLWE" (
  "AAABLNDO" varchar(3) NOT NULL default ' ',
  "AAABMMYV" varchar(1) NOT NULL default '0',
  "AAABGLWH" varchar(1) NOT NULL default ' ',
  "AAABGLWI" varchar(1) NOT NULL default '0',
  "AAABGLWJ" varchar(1) NOT NULL default ' ',
  "AAABGLWK" varchar(1) NOT NULL default '0',
  "AAABGLWL" varchar(1) NOT NULL default ' ',
  "AAABGLWM" varchar(1) NOT NULL default ' ',
  "AAABGLWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLNDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEJ" varchar(30) NOT NULL default ' ',
  "AAABLTXZ" varchar(30) NOT NULL default ' ',
  "AAABLMWD" varchar(30) NOT NULL default ' ',
  "AAABGLWU" varchar(30) NOT NULL default ' ',
  "AAABGLWV" varchar(3) NOT NULL default '000',
  "AAABGLWW" varchar(1) NOT NULL default ' ',
  "AAABGLWX" varchar(40) NOT NULL default ' ',
  "AAABGLWY" varchar(20) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  "AAABGLXA" varchar(10) NOT NULL default ' ',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEJ" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGLXI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMEJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABGLXQ" int(11) NOT NULL default '0',
  "AAABGLXR" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMGR","AAABGMGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABGLYO" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRJM" varchar(3) NOT NULL default '000',
  "AAABGLYC" varchar(1) NOT NULL default ' ',
  "AAABGLYD" varchar(1) NOT NULL default ' ',
  "AAABGLYE" varchar(5) NOT NULL default ' ',
  "AAABGLYF" varchar(1) NOT NULL default ' ',
  "AAABGMJE" varchar(30) NOT NULL default ' ',
  "AAABMLND" varchar(1) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABGLYJ" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMGR","AAABGMGU","AAABGLYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGLYU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMGR","AAABGMGU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGMCN" varchar(40) NOT NULL default ' ',
  "AAABGMCO" varchar(4) NOT NULL default ' ',
  "AAABGMCH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIKHY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABGMJJ" varchar(3) NOT NULL default '000',
  "AAABGMBH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGLZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMAY" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMBE" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABGMJJ" varchar(3) NOT NULL default '000',
  "AAABGMBH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMBJ" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMJV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMBP" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMBT" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMBZ" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIKHY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMCD" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGMCN" varchar(40) NOT NULL default ' ',
  "AAABGMCO" varchar(4) NOT NULL default ' ',
  "AAABGMCH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMCJ" (
  "AAABGMCN" varchar(40) NOT NULL default ' ',
  "AAABGMCO" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMCN","AAABGMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGMCZ" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMCX" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGMCZ" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMDD" (
  "AAABGMFE" varchar(1) NOT NULL default ' ',
  "AAABGMFF" varchar(3) NOT NULL default ' ',
  "AAABGMEW" varchar(1) NOT NULL default ' ',
  "AAABGMDH" varchar(3) NOT NULL default ' ',
  "AAABGMFA" varchar(2) NOT NULL default ' ',
  "AAABGMDJ" varchar(1) NOT NULL default ' ',
  "AAABGMDK" varchar(1) NOT NULL default ' ',
  "AAABGMFC" varchar(1) NOT NULL default ' ',
  "AAABGMEZ" varchar(1) NOT NULL default ' ',
  "AAABGMEX" varchar(1) NOT NULL default ' ',
  "AAABGMDO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMFE","AAABGMFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABGMDU" varchar(3) NOT NULL default '000',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAABGMDW" varchar(1) NOT NULL default ' ',
  "AAABGMDX" varchar(1) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABGMDZ" tinyint(3) unsigned NOT NULL default '0',
  "AAABGMEA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABGMEJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMGU","AAABGMEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMGU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMES" (
  "AAABGMFE" varchar(1) NOT NULL default ' ',
  "AAABGMFF" varchar(3) NOT NULL default ' ',
  "AAABGMFG" varchar(1) NOT NULL default ' ',
  "AAABGMEW" varchar(1) NOT NULL default ' ',
  "AAABGMEX" varchar(1) NOT NULL default ' ',
  "AAABGMEY" varchar(1) NOT NULL default ' ',
  "AAABGMEZ" varchar(1) NOT NULL default ' ',
  "AAABGMFA" varchar(2) NOT NULL default ' ',
  "AAABGMFB" varchar(1) NOT NULL default ' ',
  "AAABGMFC" varchar(1) NOT NULL default ' ',
  "AAABGMFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMFE","AAABGMFF","AAABGMFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIZMY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(30) NOT NULL default ' ',
  "AAABGMFQ" varchar(2) NOT NULL default '00',
  "AAABGMFR" varchar(3) NOT NULL default '000',
  "AAABGMFS" varchar(3) NOT NULL default ' ',
  "AAABGMFT" varchar(3) NOT NULL default '000',
  "AAABGMFU" varchar(3) NOT NULL default ' ',
  "AAABGMFV" varchar(1) NOT NULL default '0',
  "AAABGMFW" varchar(1) NOT NULL default ' ',
  "AAABGMFX" varchar(20) NOT NULL default ' ',
  "AAABGMFY" varchar(20) NOT NULL default ' ',
  "AAABGMFZ" varchar(24) NOT NULL default ' ',
  "AAABGMGA" varchar(20) NOT NULL default ' ',
  "AAABGMGB" varchar(3) NOT NULL default '000',
  "AAABGMGC" varchar(3) NOT NULL default ' ',
  "AAABGMGD" varchar(3) NOT NULL default '000',
  "AAABGMGE" varchar(3) NOT NULL default ' ',
  "AAABGMGF" varchar(3) NOT NULL default '000',
  "AAABGMGG" varchar(3) NOT NULL default ' ',
  "AAABGMGH" varchar(1) NOT NULL default ' ',
  "AAABGMGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(30) NOT NULL default ' ',
  "AAABGMGU" varchar(3) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABLMOZ" varchar(30) NOT NULL default ' ',
  "AAABGMGR" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABGMGU","AAABLILW","AAABLMOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(30) NOT NULL default ' ',
  "AAABGMHB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABGMJJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABLMUZ" varchar(1) NOT NULL default ' ',
  "AAABGMJD" varchar(30) NOT NULL default ' ',
  "AAABGMJE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMIZ" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABLMUZ" varchar(1) NOT NULL default ' ',
  "AAABGMJD" varchar(30) NOT NULL default ' ',
  "AAABGMJE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMJH" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABGMJJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABGMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMJL" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMJV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMJR" (
  "AAABGMJV" varchar(30) NOT NULL default ' ',
  "AAABMRHK" varchar(30) NOT NULL default ' ',
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMJV","AAABMRHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMJX" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABIJQL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMKB" (
  "AAABMBXY" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMKU" varchar(10) NOT NULL default ' ',
  "AAABGMLK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGMKU" varchar(10) NOT NULL default ' ',
  "AAABGMKR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGMKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMLK" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGMLK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGMLK" varchar(10) NOT NULL default ' ',
  "AAABGMLH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGMLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMLL" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMLP" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMLV" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS","AAABMREL","AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMMD" (
  "AAABGMML" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMMG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMML" varchar(5) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGMML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMMM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMMRH" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABLUXQ","AAABGMNS","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMMV" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABGMMY" varchar(4) NOT NULL default '0000',
  "AAABGMMZ" varchar(4) NOT NULL default '0000',
  "AAABMLPX" varchar(40) NOT NULL default ' ',
  "AAABGMNB" varchar(30) NOT NULL default ' ',
  "AAABGMNC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMNF" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS","AAABMREL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMNN" (
  "AAABGMNS" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGMNS","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMELA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCRJ","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABGMOT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLEST","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMSD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMSD" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJJKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJJKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJJKO" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJJKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMQK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMQK" varchar(8) NOT NULL default '00000000',
  "AAABMOHX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABGMSD" varchar(2) NOT NULL default ' ',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABGMSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  "AAABGMRA" tinyint(3) unsigned NOT NULL default '0',
  "AAABGMRB" varchar(1) NOT NULL default ' ',
  "AAABGMRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABLXID" varchar(4) NOT NULL default ' ',
  "AAABLXIC" varchar(3) NOT NULL default ' ',
  "AAABGMSD" varchar(2) NOT NULL default ' ',
  "AAABHKNF" varchar(2) NOT NULL default ' ',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABLXID","AAABLXIC","AAABGMSD","AAABHKNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMSF" (
  "AAABHJTB" varchar(3) NOT NULL default '000',
  "AAABHJSU" varchar(1) NOT NULL default ' ',
  "AAABHJSV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMSK" (
  "AAABHJTB" varchar(3) NOT NULL default '000',
  "AAABHJSZ" varchar(5) NOT NULL default ' ',
  "AAABHJTA" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABHJTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHKNF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHKNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHKNF" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHKNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMTM" varchar(6) NOT NULL default ' ',
  "AAABGMTN" varchar(2) NOT NULL default '00',
  "AAABGMTG" varchar(30) NOT NULL default ' ',
  "AAABGMTH" varchar(20) NOT NULL default ' ',
  "AAABGMTI" varchar(1) NOT NULL default ' ',
  "AAABJJKO" varchar(1) NOT NULL default ' ',
  "AAABJJKP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGMTM","AAABGMTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMTT" varchar(12) NOT NULL default ' ',
  "AAABGMTR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABGNFM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHE","AAABGNFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDFU" varchar(4) NOT NULL default ' ',
  "AAABIFHG" varchar(4) NOT NULL default ' ',
  "AAABGMUF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDFU" varchar(4) NOT NULL default ' ',
  "AAABGMUM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGMUZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAABLCCB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABGMVR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCR","AAABLZNN","AAABLZNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMVW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGMWH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMWD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABGMWH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMWY" varchar(3) NOT NULL default '000',
  "AAABGMWN" varchar(132) NOT NULL default ' ',
  "AAABGMWO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGMWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGMWY" varchar(3) NOT NULL default '000',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGMWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABMLMR" varchar(4) NOT NULL default ' ',
  "AAABJWRA" varchar(1) NOT NULL default ' ',
  "AAABGMXF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABGMXL" varchar(1) NOT NULL default ' ',
  "AAABGMXM" varchar(1) NOT NULL default ' ',
  "AAABGMXN" varchar(1) NOT NULL default ' ',
  "AAABGMXO" varchar(1) NOT NULL default ' ',
  "AAABGMXP" varchar(1) NOT NULL default ' ',
  "AAABGMXQ" varchar(1) NOT NULL default ' ',
  "AAABGMXR" varchar(1) NOT NULL default ' ',
  "AAABGMXS" varchar(1) NOT NULL default ' ',
  "AAABGMXT" varchar(1) NOT NULL default ' ',
  "AAABGMXU" varchar(1) NOT NULL default ' ',
  "AAABGMXV" varchar(1) NOT NULL default ' ',
  "AAABGMXW" varchar(1) NOT NULL default ' ',
  "AAABGMXX" varchar(1) NOT NULL default ' ',
  "AAABGMXY" varchar(1) NOT NULL default ' ',
  "AAABGMXZ" varchar(1) NOT NULL default ' ',
  "AAABGMYA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  "AAABGMYI" varchar(20) NOT NULL default ' ',
  "AAABGMYJ" varchar(20) NOT NULL default ' ',
  "AAABGMYK" varchar(20) NOT NULL default ' ',
  "AAABGMYL" varchar(20) NOT NULL default ' ',
  "AAABGMYM" varchar(20) NOT NULL default ' ',
  "AAABGMYN" varchar(20) NOT NULL default ' ',
  "AAABGMYO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNAE" varchar(10) NOT NULL default ' ',
  "AAABGMYV" varchar(1) NOT NULL default '0',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABGMYX" varchar(1) NOT NULL default ' ',
  "AAABGYJW" varchar(1) NOT NULL default ' ',
  "AAABGMYZ" varchar(20) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABHBRF" varchar(2) NOT NULL default ' ',
  "AAABGMZC" varchar(1) NOT NULL default ' ',
  "AAABGMZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMZG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNAE" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNAE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMZO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNAE" varchar(10) NOT NULL default ' ',
  "AAABIKBY" varchar(10) NOT NULL default ' ',
  "AAABGMZS" varchar(3) NOT NULL default ' ',
  "AAABGMZT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNAE","AAABIKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGMZX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNAE" varchar(10) NOT NULL default ' ',
  "AAABIKBY" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNAE","AAABIKBY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNAZ","AAABLCCC","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNAZ" varchar(2) NOT NULL default '00',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNAZ","AAABLCCC","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAABGNBG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNBR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGYQM","AAABGNBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNCA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABHSKS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABGNCL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNCP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABGNHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNCV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNDD" (
  "AAABGNDH" varchar(1) NOT NULL default ' ',
  "AAABGNDI" varchar(1) NOT NULL default ' ',
  "AAABGNDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNDH","AAABGNDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNDJ" (
  "AAABGNDO" varchar(2) NOT NULL default ' ',
  "AAABGNDP" varchar(1) NOT NULL default ' ',
  "AAABGNDQ" varchar(1) NOT NULL default ' ',
  "AAABGNDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNDO","AAABGNDP","AAABGNDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNFM" varchar(4) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABGNHT" varchar(1) NOT NULL default ' ',
  "AAABGNHU" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABGNDY" varchar(1) NOT NULL default ' ',
  "AAABGNDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNEC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNEJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABGNEW" varchar(10) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAABGNEQ" varchar(1) NOT NULL default ' ',
  "AAABGNER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYLC","AAABLMNC","AAABGNEW","AAABHGWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNFM" varchar(4) NOT NULL default ' ',
  "AAABGNFC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABGNFM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCR","AAABGNFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABLCCR" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABGNFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(4) NOT NULL default ' ',
  "AAABGNHA" varchar(2) NOT NULL default ' ',
  "AAABGNHB" varchar(1) NOT NULL default ' ',
  "AAABGNHC" varchar(1) NOT NULL default ' ',
  "AAABGNHT" varchar(1) NOT NULL default ' ',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABGNHF" varchar(1) NOT NULL default ' ',
  "AAABGNHG" varchar(1) NOT NULL default ' ',
  "AAABGNHH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIFHG" varchar(4) NOT NULL default ' ',
  "AAABGNGT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFHG" varchar(4) NOT NULL default ' ',
  "AAABGNHA" varchar(2) NOT NULL default ' ',
  "AAABGNHB" varchar(1) NOT NULL default ' ',
  "AAABGNHC" varchar(1) NOT NULL default ' ',
  "AAABGNHT" varchar(1) NOT NULL default ' ',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABGNHF" varchar(1) NOT NULL default ' ',
  "AAABGNHG" varchar(1) NOT NULL default ' ',
  "AAABGNHH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIFHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNHO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABGNHT" varchar(1) NOT NULL default ' ',
  "AAABGNHU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABGNIB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNIF" (
  "AAABJETU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABJETU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNII" (
  "AAABJETU" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABJETU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNIO" (
  "AAABJETU" varchar(6) NOT NULL default ' ',
  "AAABGNIS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABJETU","AAABGNIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNIT" (
  "AAABGNIZ" varchar(4) NOT NULL default ' ',
  "AAABGNIV" varchar(8) NOT NULL default ' ',
  "AAABGNIW" varchar(1) NOT NULL default ' ',
  "AAABGNIX" varchar(2) NOT NULL default ' ',
  "AAABGNIY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNJA" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABGOKV" varchar(1) NOT NULL default ' ',
  "AAABGNJE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNJG" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABGOKV" varchar(1) NOT NULL default ' ',
  "AAABGNJS" varchar(4) NOT NULL default ' ',
  "AAABGNJT" varchar(4) NOT NULL default ' ',
  "AAABGNJU" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(20) NOT NULL default ' ',
  "AAABGNJO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMFSV","AAABGOKV","AAABGNJS","AAABGNJT","AAABGNJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNKL" varchar(6) NOT NULL default ' ',
  "AAABGNJY" varchar(1) NOT NULL default ' ',
  "AAABGNJZ" varchar(1) NOT NULL default ' ',
  "AAABGNKA" varchar(1) NOT NULL default ' ',
  "AAABGNKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNKL" varchar(6) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNKY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNKY" varchar(8) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGNKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNKZ" (
  "AAABGNLC" varchar(20) NOT NULL default ' ',
  "AAABGNLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNML" varchar(4) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNML","AAABGNMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNML" varchar(4) NOT NULL default ' ',
  "AAABGNLS" varchar(1) NOT NULL default ' ',
  "AAABGNLT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGNML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGNMD" varchar(4) NOT NULL default ' ',
  "AAABGNMI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGNMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGNML" varchar(4) NOT NULL default ' ',
  "AAABGNMI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGNML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNNC" varchar(2) NOT NULL default ' ',
  "AAABGNMP" varchar(1) NOT NULL default ' ',
  "AAABGNMQ" varchar(1) NOT NULL default ' ',
  "AAABGNMR" varchar(1) NOT NULL default ' ',
  "AAABGNMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNNC" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNNY" varchar(4) NOT NULL default ' ',
  "AAABGNNG" varchar(1) NOT NULL default ' ',
  "AAABGNNH" varchar(1) NOT NULL default ' ',
  "AAABGNNI" varchar(1) NOT NULL default ' ',
  "AAABGNQG" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABGNNL" varchar(1) NOT NULL default ' ',
  "AAABGNNM" varchar(1) NOT NULL default ' ',
  "AAABGNNN" varchar(4) NOT NULL default '0000',
  "AAABGNNO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGNNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNNY" varchar(4) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOAX" varchar(2) NOT NULL default ' ',
  "AAABGNOC" varchar(1) NOT NULL default ' ',
  "AAABGNOD" varchar(1) NOT NULL default '0',
  "AAABGNOE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOAX" varchar(2) NOT NULL default ' ',
  "AAABGNOL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGOAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNPX" varchar(8) NOT NULL default ' ',
  "AAABGNOS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNPX" varchar(8) NOT NULL default ' ',
  "AAABKZYE" varchar(2) NOT NULL default '00',
  "AAABGNOZ" varchar(30) NOT NULL default ' ',
  "AAABGNPA" varchar(1) NOT NULL default ' ',
  "AAABGNPB" varchar(1) NOT NULL default ' ',
  "AAABMINV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNPX","AAABKZYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIZS" varchar(10) NOT NULL default ' ',
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  "AAABGNPK" varchar(3) NOT NULL default ' ',
  "AAABGNPL" varchar(3) NOT NULL default ' ',
  "AAABGNPM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIZS","AAABLYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNPX" varchar(8) NOT NULL default ' ',
  "AAABGNPU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNQU" varchar(8) NOT NULL default ' ',
  "AAABGNQB" varchar(1) NOT NULL default ' ',
  "AAABGNQC" varchar(1) NOT NULL default ' ',
  "AAABGNQD" varchar(1) NOT NULL default ' ',
  "AAABGNQE" varchar(1) NOT NULL default ' ',
  "AAABGNQF" varchar(1) NOT NULL default ' ',
  "AAABGNQG" varchar(1) NOT NULL default ' ',
  "AAABGNQH" varchar(1) NOT NULL default ' ',
  "AAABGNQI" varchar(1) NOT NULL default ' ',
  "AAABGNQJ" varchar(1) NOT NULL default ' ',
  "AAABGNQK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNQU" varchar(8) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNRJ" varchar(3) NOT NULL default ' ',
  "AAABGNQY" varchar(1) NOT NULL default ' ',
  "AAABLLDO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNRJ" varchar(3) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNRX" varchar(5) NOT NULL default ' ',
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGNRX" varchar(5) NOT NULL default ' ',
  "AAABGNRU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGNRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNSP" varchar(1) NOT NULL default ' ',
  "AAABGNSB" varchar(1) NOT NULL default ' ',
  "AAABGNSC" varchar(1) NOT NULL default ' ',
  "AAABGNSD" varchar(1) NOT NULL default ' ',
  "AAABJCPO" varchar(1) NOT NULL default ' ',
  "AAABGNSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNSP" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNTL" varchar(1) NOT NULL default ' ',
  "AAABGNST" varchar(1) NOT NULL default ' ',
  "AAABGNSU" varchar(1) NOT NULL default ' ',
  "AAABGNSV" varchar(12) NOT NULL default ' ',
  "AAABGNSW" varchar(8) NOT NULL default '00000000',
  "AAABGOGT" varchar(12) NOT NULL default ' ',
  "AAABGNSY" varchar(8) NOT NULL default '00000000',
  "AAABGNSZ" varchar(1) NOT NULL default ' ',
  "AAABGNTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNTL" varchar(1) NOT NULL default ' ',
  "AAABGNTH" varchar(40) NOT NULL default ' ',
  "AAABGNTI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNTV" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  "AAABGNTR" varchar(5) NOT NULL default ' ',
  "AAABGNTS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNUT" varchar(2) NOT NULL default ' ',
  "AAABGNTZ" varchar(1) NOT NULL default ' ',
  "AAABGNUA" decimal(3,0) NOT NULL default '0',
  "AAABGNUB" varchar(1) NOT NULL default ' ',
  "AAABGNUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNVB" varchar(2) NOT NULL default ' ',
  "AAABGNUI" decimal(4,0) NOT NULL default '0',
  "AAABGNUJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNUT" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNVB" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNVC" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  "AAABGNVF" varchar(2) NOT NULL default ' ',
  "AAABGNVG" varchar(1) NOT NULL default ' ',
  "AAABGNVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMFSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNVK" (
  "AAABGNVS" varchar(1) NOT NULL default ' ',
  "AAABGNVT" varchar(1) NOT NULL default ' ',
  "AAABGNVU" varchar(1) NOT NULL default ' ',
  "AAABGNVV" varchar(1) NOT NULL default ' ',
  "AAABGNVP" varchar(1) NOT NULL default ' ',
  "AAABGNVQ" varchar(6) NOT NULL default ' ',
  "AAABGNVR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNVS","AAABGNVT","AAABGNVU","AAABGNVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNXJ" varchar(4) NOT NULL default ' ',
  "AAABGNWQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABGNWR" varchar(3) NOT NULL default ' ',
  "AAABGNWB" varchar(1) NOT NULL default ' ',
  "AAABGNWS" double NOT NULL default '0',
  "AAABGNWT" varchar(1) NOT NULL default ' ',
  "AAABGNWU" varchar(1) NOT NULL default ' ',
  "AAABGNWV" double NOT NULL default '0',
  "AAABGNWW" varchar(1) NOT NULL default ' ',
  "AAABGNWX" double NOT NULL default '0',
  "AAABGNWY" varchar(1) NOT NULL default ' ',
  "AAABGNWJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNXJ" varchar(4) NOT NULL default ' ',
  "AAABGNXB" varchar(3) NOT NULL default '000',
  "AAABGNWQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABGNWR" varchar(3) NOT NULL default ' ',
  "AAABGNWS" double NOT NULL default '0',
  "AAABGNWT" varchar(1) NOT NULL default ' ',
  "AAABGNWU" varchar(1) NOT NULL default ' ',
  "AAABGNWV" double NOT NULL default '0',
  "AAABGNWW" varchar(1) NOT NULL default ' ',
  "AAABGNWX" double NOT NULL default '0',
  "AAABGNWY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNXJ","AAABGNXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGNXJ" varchar(4) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGNXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  "AAABGNXN" varchar(3) NOT NULL default ' ',
  "AAABIMQW" varchar(2) NOT NULL default ' ',
  "AAABGNXP" varchar(8) NOT NULL default ' ',
  "AAABGNXQ" varchar(6) NOT NULL default ' ',
  "AAABGNXR" varchar(8) NOT NULL default '00000000',
  "AAABGNXS" varchar(1) NOT NULL default ' ',
  "AAABGNXT" varchar(1) NOT NULL default ' ',
  "AAABGNXU" varchar(1) NOT NULL default ' ',
  "AAABGNXV" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABKOGI" varchar(2) NOT NULL default ' ',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABGPLQ" varchar(2) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABGNYC" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  "AAABKOCR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABJDAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNYP" (
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNYS" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  "AAABGNYV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABLYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHOPI" varchar(2) NOT NULL default ' ',
  "AAABLXKB" varchar(3) NOT NULL default ' ',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHOPI","AAABLXKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNZP" (
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  "AAABGOAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLYAG","AAABGOAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGNZU" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABLYAG" varchar(2) NOT NULL default ' ',
  "AAABGOAB" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABLYAG","AAABGOAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDAN" varchar(8) NOT NULL default ' ',
  "AAABGOAF" varchar(1) NOT NULL default ' ',
  "AAABGOAG" varchar(1) NOT NULL default ' ',
  "AAABGOAH" varchar(1) NOT NULL default ' ',
  "AAABGOAI" varchar(1) NOT NULL default ' ',
  "AAABIKBS" varchar(1) NOT NULL default ' ',
  "AAABGOAK" varchar(1) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABGOAM" varchar(1) NOT NULL default ' ',
  "AAABGOAN" varchar(8) NOT NULL default ' ',
  "AAABGOAO" varchar(3) NOT NULL default ' ',
  "AAABGOAP" decimal(3,0) NOT NULL default '0',
  "AAABGOAQ" varchar(1) NOT NULL default ' ',
  "AAABGOAR" varchar(1) NOT NULL default ' ',
  "AAABGOAS" varchar(1) NOT NULL default ' ',
  "AAABGOAT" varchar(1) NOT NULL default ' ',
  "AAABGOAU" varchar(1) NOT NULL default ' ',
  "AAABGOAV" decimal(3,0) NOT NULL default '0',
  "AAABGOAW" varchar(1) NOT NULL default ' ',
  "AAABGOAX" varchar(2) NOT NULL default ' ',
  "AAABGOAY" decimal(6,4) NOT NULL default '0.0000',
  "AAABGOAZ" varchar(1) NOT NULL default ' ',
  "AAABGOBA" varchar(1) NOT NULL default ' ',
  "AAABGOBB" varchar(1) NOT NULL default ' ',
  "AAABGOBC" varchar(1) NOT NULL default ' ',
  "AAABGOBD" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJDAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOBG" (
  "AAABGOCQ" varchar(3) NOT NULL default ' ',
  "AAABGOVA" varchar(1) NOT NULL default ' ',
  "AAABGOBJ" varchar(1) NOT NULL default ' ',
  "AAABGOBK" varchar(1) NOT NULL default ' ',
  "AAABGOBL" varchar(1) NOT NULL default ' ',
  "AAABGOBM" varchar(1) NOT NULL default ' ',
  "AAABGOBN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGOCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOBP" (
  "AAABGOCK" varchar(1) NOT NULL default ' ',
  "AAABGOBR" varchar(1) NOT NULL default ' ',
  "AAABGOBS" varchar(1) NOT NULL default ' ',
  "AAABGOBT" varchar(1) NOT NULL default ' ',
  "AAABGOBU" varchar(1) NOT NULL default ' ',
  "AAABGOBV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGOCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGOCK" varchar(1) NOT NULL default ' ',
  "AAABGOCB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGOCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOCF" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOCK" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABGOCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOCL" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOCQ" varchar(3) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABGOCQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABGOCW" varchar(30) NOT NULL default ' ',
  "AAABGOCX" varchar(1) NOT NULL default ' ',
  "AAABGOGE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGODB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGODJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGODY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGODQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABGODY" varchar(12) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABGODY","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOEL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOEL" varchar(6) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOEL","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOEZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGOEZ" varchar(8) NOT NULL default ' ',
  "AAABGOEW" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGOEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFA" (
  "AAABGPKK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFD" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGPKK" varchar(1) NOT NULL default ' ',
  "AAABGOFG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABGPKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOFV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOFV" varchar(8) NOT NULL default ' ',
  "AAABGOFS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGOFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFW" (
  "AAABGOGE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABGOGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOFZ" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOGE" varchar(2) NOT NULL default ' ',
  "AAABGOGC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABGOGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOHF" varchar(2) NOT NULL default ' ',
  "AAABGOGI" decimal(3,0) NOT NULL default '0',
  "AAABGOGJ" decimal(3,0) NOT NULL default '0',
  "AAABGOGK" decimal(3,0) NOT NULL default '0',
  "AAABGOGL" decimal(3,0) NOT NULL default '0',
  "AAABGOGM" decimal(3,0) NOT NULL default '0',
  "AAABGOGN" decimal(3,0) NOT NULL default '0',
  "AAABGOGO" decimal(3,0) NOT NULL default '0',
  "AAABGOGP" decimal(3,0) NOT NULL default '0',
  "AAABGOGQ" decimal(3,0) NOT NULL default '0',
  "AAABGOGR" decimal(3,0) NOT NULL default '0',
  "AAABGOGS" varchar(1) NOT NULL default ' ',
  "AAABGOGT" varchar(12) NOT NULL default ' ',
  "AAABGOGU" varchar(8) NOT NULL default '00000000',
  "AAABGOGV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABGOHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOHF" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGOHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOHG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOJT" varchar(2) NOT NULL default ' ',
  "AAABGOIN" varchar(1) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  "AAABGOHL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOHO" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOHU" varchar(2) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOHU","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOHW" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOII" varchar(2) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOIC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOII" varchar(2) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOII","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOIK" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOJU" varchar(2) NOT NULL default ' ',
  "AAABGOIN" varchar(1) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOIR" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOIX" varchar(2) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOIX","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOIZ" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOJL" varchar(2) NOT NULL default ' ',
  "AAABHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOJF" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOJL" varchar(2) NOT NULL default ' ',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOJL","AAABINAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOJN" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABGOJT" varchar(2) NOT NULL default ' ',
  "AAABGOJU" varchar(2) NOT NULL default ' ',
  "AAABGOJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABGOJT","AAABGOJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABGOKA" varchar(1) NOT NULL default ' ',
  "AAABGOKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMQIJ","AAABMEKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOKG" (
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABGOKY" varchar(4) NOT NULL default ' ',
  "AAABGOKJ" varchar(2) NOT NULL default '00',
  "AAABGOKK" varchar(1) NOT NULL default ' ',
  "AAABGOKL" varchar(1) NOT NULL default ' ',
  "AAABGOKM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLW","AAABGOKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOKP" (
  "AAABGOKV" varchar(1) NOT NULL default ' ',
  "AAABGOKW" varchar(4) NOT NULL default ' ',
  "AAABKOGI" varchar(2) NOT NULL default ' ',
  "AAABGOKY" varchar(4) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABGOKV","AAABGOKW","AAABKOGI","AAABGOKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOLR" varchar(3) NOT NULL default ' ',
  "AAABGPJN" varchar(3) NOT NULL default ' ',
  "AAABGOLD" varchar(40) NOT NULL default ' ',
  "AAABGOLE" varchar(40) NOT NULL default ' ',
  "AAABGOLF" varchar(40) NOT NULL default ' ',
  "AAABGOLG" varchar(40) NOT NULL default ' ',
  "AAABGOLH" varchar(40) NOT NULL default ' ',
  "AAABGOLI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOLR" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOLR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGOMJ" varchar(2) NOT NULL default ' ',
  "AAABHHXC" varchar(30) NOT NULL default ' ',
  "AAABGOLX" varchar(1) NOT NULL default ' ',
  "AAABLYKS" varchar(1) NOT NULL default ' ',
  "AAABGOLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGOMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABGOMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABGOMJ" varchar(2) NOT NULL default ' ',
  "AAABKZXE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABGOMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
