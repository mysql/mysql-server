# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl34.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABCSZI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABCTAP" varchar(1) NOT NULL default ' ',
  "AAABCSZP" varchar(1) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABCTYB" varchar(4) NOT NULL default '0000',
  "AAABCTYC" varchar(2) NOT NULL default '00',
  "AAABCTAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYQM","AAABGNBZ","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCSZZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTAH" varchar(30) NOT NULL default ' ',
  "AAABCTAI" varchar(30) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTAH","AAABCTAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTAJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABCTAP" varchar(1) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABCTYB" varchar(4) NOT NULL default '0000',
  "AAABCTYC" varchar(2) NOT NULL default '00',
  "AAABCTAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGYQM","AAABGNBZ","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTAZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABHSKS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTBI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTBS" (
  "AAABGPHD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABGPHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTBV" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGPHD" varchar(4) NOT NULL default '0000',
  "AAABGNCZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABGPHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTCB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABCTCG" varchar(1) NOT NULL default ' ',
  "AAABCTCH" varchar(1) NOT NULL default ' ',
  "AAABCTCI" varchar(1) NOT NULL default ' ',
  "AAABLXDX" varchar(1) NOT NULL default ' ',
  "AAABCTCK" varchar(1) NOT NULL default ' ',
  "AAABCTCL" varchar(1) NOT NULL default ' ',
  "AAABCTCM" varchar(1) NOT NULL default ' ',
  "AAABCTCN" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTCS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTDC" (
  "AAABGNDH" varchar(1) NOT NULL default ' ',
  "AAABGNDI" varchar(1) NOT NULL default ' ',
  "AAABGNDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNDH","AAABGNDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTDI" (
  "AAABCTDM" varchar(1) NOT NULL default ' ',
  "AAABCTDN" varchar(1) NOT NULL default ' ',
  "AAABGNDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCTDM","AAABCTDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTDO" (
  "AAABGNDO" varchar(2) NOT NULL default ' ',
  "AAABGNDP" varchar(1) NOT NULL default ' ',
  "AAABGNDQ" varchar(1) NOT NULL default ' ',
  "AAABGNDN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGNDO","AAABGNDP","AAABGNDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTDW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABCTNI" varchar(1) NOT NULL default ' ',
  "AAABCTEB" varchar(1) NOT NULL default '0',
  "AAABCTEC" varchar(2) NOT NULL default '00',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCTGF" varchar(1) NOT NULL default ' ',
  "AAABCTGG" varchar(1) NOT NULL default ' ',
  "AAABCTGH" varchar(1) NOT NULL default ' ',
  "AAABCTGI" varchar(1) NOT NULL default ' ',
  "AAABCTGJ" varchar(1) NOT NULL default ' ',
  "AAABCTGK" varchar(1) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABCTEM" varchar(1) NOT NULL default ' ',
  "AAABCTEN" varchar(1) NOT NULL default ' ',
  "AAABCTEO" varchar(1) NOT NULL default ' ',
  "AAABCTEP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTET" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABCTGF" varchar(1) NOT NULL default ' ',
  "AAABCTGG" varchar(1) NOT NULL default ' ',
  "AAABCTGH" varchar(1) NOT NULL default ' ',
  "AAABCTGI" varchar(1) NOT NULL default ' ',
  "AAABCTGJ" varchar(1) NOT NULL default ' ',
  "AAABCTGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMNLW","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTFI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABCTGF" varchar(1) NOT NULL default ' ',
  "AAABCTGG" varchar(1) NOT NULL default ' ',
  "AAABCTGH" varchar(1) NOT NULL default ' ',
  "AAABCTGI" varchar(1) NOT NULL default ' ',
  "AAABCTGJ" varchar(1) NOT NULL default ' ',
  "AAABCTGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMNLW","AAABHSKS","AAABLSMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTFZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABCTGF" varchar(1) NOT NULL default ' ',
  "AAABCTGG" varchar(1) NOT NULL default ' ',
  "AAABCTGH" varchar(1) NOT NULL default ' ',
  "AAABCTGI" varchar(1) NOT NULL default ' ',
  "AAABCTGJ" varchar(1) NOT NULL default ' ',
  "AAABCTGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMNLW","AAABHSKS","AAABCTUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTGQ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABCTGV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTHA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMDXP" varchar(10) NOT NULL default ' ',
  "AAABMDWW" varchar(10) NOT NULL default ' ',
  "AAABCTLF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCTLG" varchar(30) NOT NULL default ' ',
  "AAABCTLH" varchar(30) NOT NULL default ' ',
  "AAABCTLI" varchar(30) NOT NULL default ' ',
  "AAABCTLJ" varchar(30) NOT NULL default ' ',
  "AAABCTLK" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABCTLM" varchar(30) NOT NULL default ' ',
  "AAABCTLN" varchar(30) NOT NULL default ' ',
  "AAABCTLO" varchar(1) NOT NULL default ' ',
  "AAABCTLP" varchar(30) NOT NULL default ' ',
  "AAABCTLQ" varchar(1) NOT NULL default ' ',
  "AAABCTLR" varchar(30) NOT NULL default ' ',
  "AAABCTLS" varchar(30) NOT NULL default ' ',
  "AAABCTLT" varchar(30) NOT NULL default ' ',
  "AAABCTLU" varchar(30) NOT NULL default ' ',
  "AAABCTLV" varchar(30) NOT NULL default ' ',
  "AAABCTLW" varchar(30) NOT NULL default ' ',
  "AAABCTLX" varchar(30) NOT NULL default ' ',
  "AAABCTLY" varchar(1) NOT NULL default ' ',
  "AAABCTLZ" varchar(30) NOT NULL default ' ',
  "AAABCTMA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTIE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTIV" varchar(6) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTIM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTIV" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCTIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTIW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTIZ" varchar(30) NOT NULL default ' ',
  "AAABCTJA" varchar(30) NOT NULL default ' ',
  "AAABCTJB" varchar(30) NOT NULL default ' ',
  "AAABCTJC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTJF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTJN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTJU" varchar(4) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTJV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABLTST" varchar(1) NOT NULL default ' ',
  "AAABCTKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABJHHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTKE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABJHHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTKO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABJHHP" varchar(10) NOT NULL default ' ',
  "AAABMDXP" varchar(10) NOT NULL default ' ',
  "AAABMDWW" varchar(10) NOT NULL default ' ',
  "AAABCTLF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCTKV" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABJHHP","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTLA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMDXP" varchar(10) NOT NULL default ' ',
  "AAABMDWW" varchar(10) NOT NULL default ' ',
  "AAABCTLF" tinyint(3) unsigned NOT NULL default '0',
  "AAABCTLG" varchar(30) NOT NULL default ' ',
  "AAABCTLH" varchar(30) NOT NULL default ' ',
  "AAABCTLI" varchar(30) NOT NULL default ' ',
  "AAABCTLJ" varchar(30) NOT NULL default ' ',
  "AAABCTLK" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABCTLM" varchar(30) NOT NULL default ' ',
  "AAABCTLN" varchar(30) NOT NULL default ' ',
  "AAABCTLO" varchar(1) NOT NULL default ' ',
  "AAABCTLP" varchar(30) NOT NULL default ' ',
  "AAABCTLQ" varchar(1) NOT NULL default ' ',
  "AAABCTLR" varchar(30) NOT NULL default ' ',
  "AAABCTLS" varchar(30) NOT NULL default ' ',
  "AAABCTLT" varchar(30) NOT NULL default ' ',
  "AAABCTLU" varchar(30) NOT NULL default ' ',
  "AAABCTLV" varchar(30) NOT NULL default ' ',
  "AAABCTLW" varchar(30) NOT NULL default ' ',
  "AAABCTLX" varchar(30) NOT NULL default ' ',
  "AAABCTLY" varchar(1) NOT NULL default ' ',
  "AAABCTLZ" varchar(30) NOT NULL default ' ',
  "AAABCTMA" varchar(30) NOT NULL default ' ',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTMF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABHMRI" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTMP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABEOIA" varchar(80) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTUY","AAABEOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTMZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABEOIA" varchar(80) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABCTNQ" varchar(7) NOT NULL default '0000000',
  "AAABCTNR" varchar(7) NOT NULL default '0000000',
  "AAABCTNS" varchar(7) NOT NULL default '0000000',
  "AAABCTNI" varchar(1) NOT NULL default ' ',
  "AAABLSBY" varchar(100) NOT NULL default ' ',
  "AAABCTNK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABEOIA","AAABCTUY","AAABCTNQ","AAABCTNR","AAABCTNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTNT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTPG" varchar(30) NOT NULL default ' ',
  "AAABCTNX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTOB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTPG" varchar(30) NOT NULL default ' ',
  "AAABCTOG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTOL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTPG" varchar(30) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABCTOQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTPG","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTOV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTPG" varchar(30) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABCTPB" varchar(50) NOT NULL default ' ',
  "AAABCTPC" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABCTPG","AAABMGHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTPI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCTQC" varchar(6) NOT NULL default ' ',
  "AAABCTPL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCTQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTPO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCTQC" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCTQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTPW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCTQC" varchar(6) NOT NULL default ' ',
  "AAABCTQD" varchar(6) NOT NULL default ' ',
  "AAABCTQA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCTQC","AAABCTQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTQE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABCTQI" varchar(6) NOT NULL default ' ',
  "AAABCTQJ" varchar(6) NOT NULL default ' ',
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABCTQL" varchar(4) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABFFUB" varchar(1) NOT NULL default ' ',
  "AAABCTQO" varchar(1) NOT NULL default ' ',
  "AAABGJXW" varchar(3) NOT NULL default '000',
  "AAABCTQQ" varchar(3) NOT NULL default '000',
  "AAABCTQR" varchar(3) NOT NULL default '000',
  "AAABCTQS" varchar(1) NOT NULL default ' ',
  "AAABCTQT" varchar(1) NOT NULL default ' ',
  "AAABCTQU" varchar(1) NOT NULL default ' ',
  "AAABCTQV" varchar(1) NOT NULL default ' ',
  "AAABCTQW" varchar(1) NOT NULL default ' ',
  "AAABCTQX" varchar(1) NOT NULL default ' ',
  "AAABCTQY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTRC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  "AAABCTRI" varchar(50) NOT NULL default ' ',
  "AAABCTRJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABGNHE","AAABCTUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTRO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABCTRW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP","AAABCTRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTRX" (
  "AAABCTTD" varchar(2) NOT NULL default ' ',
  "AAABCTRZ" varchar(30) NOT NULL default ' ',
  "AAABCTSA" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABCTSC" varchar(30) NOT NULL default ' ',
  "AAABCTSD" varchar(30) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCTTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTSH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCTTD" varchar(2) NOT NULL default ' ',
  "AAABDWNR" varchar(10) NOT NULL default ' ',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCTTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTSO" (
  "AAABCTSS" varchar(2) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABCTTD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCTSS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTSU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABCTTD" varchar(2) NOT NULL default ' ',
  "AAABCTSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP","AAABCTTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTTE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTTO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABCTTS" varchar(1) NOT NULL default ' ',
  "AAABCTTT" varchar(30) NOT NULL default ' ',
  "AAABCTTU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABGYQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTTY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABCTUD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTUI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGNHE" varchar(6) NOT NULL default ' ',
  "AAABCTUP" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGNHE","AAABCTUP","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTUR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCTUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTUV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCTVY" varchar(6) NOT NULL default ' ',
  "AAABCTUY" varchar(6) NOT NULL default ' ',
  "AAABLUER" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTVC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCTVY" varchar(6) NOT NULL default ' ',
  "AAABDIWI" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTVJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABCTVY" varchar(6) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABCTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTVS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(6) NOT NULL default ' ',
  "AAABCTVY" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLCCC","AAABCTVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTVZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGFDP" varchar(6) NOT NULL default ' ',
  "AAABGLWX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGFDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTWF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGFDP" varchar(6) NOT NULL default ' ',
  "AAABDIWI" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGFDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTWM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABGFDP" varchar(6) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABGFDP","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTWU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABETPH" varchar(1) NOT NULL default ' ',
  "AAABCTWZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTXD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABCTXI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTXN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCTXU" varchar(10) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABCTXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTXV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABCTYB" varchar(4) NOT NULL default '0000',
  "AAABCTYC" varchar(2) NOT NULL default '00',
  "AAABCTYD" varchar(30) NOT NULL default ' ',
  "AAABCTYE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTYJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABCTYN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKWSS","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTYR" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  "AAABCTZJ" varchar(1) NOT NULL default ' ',
  "AAABILEX" varchar(1) NOT NULL default ' ',
  "AAABDQUF" varchar(20) NOT NULL default ' ',
  "AAABDWNR" varchar(20) NOT NULL default ' ',
  "AAABCTZC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMMVU","AAABMNLX","AAABMNLY","AAABMDER","AAABMDBL","AAABCTZJ","AAABILEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMDBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLV","AAABMDER","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCTZV" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABJYTM" varchar(30) NOT NULL default ' ',
  "AAABDSHV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLX","AAABMNLY","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABDTJB" varchar(2) NOT NULL default ' ',
  "AAABCUAK" varchar(1) NOT NULL default ' ',
  "AAABCUAL" varchar(1) NOT NULL default ' ',
  "AAABCUAM" varchar(1) NOT NULL default ' ',
  "AAABCUAN" varchar(1) NOT NULL default ' ',
  "AAABCUAO" varchar(1) NOT NULL default ' ',
  "AAABCUAP" varchar(1) NOT NULL default ' ',
  "AAABCUAQ" varchar(1) NOT NULL default ' ',
  "AAABCUAR" varchar(1) NOT NULL default ' ',
  "AAABCUAS" varchar(1) NOT NULL default ' ',
  "AAABCUAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDTOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDTOD"),
  KEY "AAABCUBE" ("AAABMRIL","AAABDTOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUBH" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDBN" varchar(4) NOT NULL default ' ',
  "AAABMDBO" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABHDQL" varchar(1) NOT NULL default ' ',
  "AAABHDRV" varchar(1) NOT NULL default ' ',
  "AAABKNQI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILOU" varchar(6) NOT NULL default ' ',
  "AAABCUBX" varchar(1) NOT NULL default ' ',
  "AAABCUBY" varchar(21) NOT NULL default ' ',
  "AAABCUBZ" varchar(21) NOT NULL default ' ',
  "AAABEPVW" varchar(6) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABCUCC" decimal(9,3) NOT NULL default '0.000',
  "AAABCUCD" varchar(10) NOT NULL default ' ',
  "AAABCUGV" varchar(1) NOT NULL default ' ',
  "AAABCUGW" varchar(1) NOT NULL default ' ',
  "AAABCUGX" varchar(1) NOT NULL default ' ',
  "AAABCUGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABILOU" varchar(6) NOT NULL default ' ',
  "AAABLUDG" varchar(11) NOT NULL default ' ',
  "AAABIWTP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABILOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOCY" varchar(4) NOT NULL default ' ',
  "AAABCUCW" varchar(6) NOT NULL default ' ',
  "AAABCUCX" varchar(1) NOT NULL default ' ',
  "AAABCUCY" varchar(6) NOT NULL default ' ',
  "AAABCUCZ" varchar(1) NOT NULL default ' ',
  "AAABCUDA" varchar(6) NOT NULL default ' ',
  "AAABCUDB" varchar(1) NOT NULL default ' ',
  "AAABDOIZ" varchar(6) NOT NULL default ' ',
  "AAABCUDD" varchar(1) NOT NULL default ' ',
  "AAABDOJA" varchar(6) NOT NULL default ' ',
  "AAABCUDF" varchar(1) NOT NULL default ' ',
  "AAABCUDG" varchar(6) NOT NULL default ' ',
  "AAABCUDH" varchar(1) NOT NULL default ' ',
  "AAABCUDI" varchar(6) NOT NULL default ' ',
  "AAABDROJ" varchar(1) NOT NULL default ' ',
  "AAABCUDK" varchar(1) NOT NULL default '0',
  "AAABCUDL" varchar(6) NOT NULL default ' ',
  "AAABCUDM" varchar(1) NOT NULL default '0',
  "AAABCUDN" varchar(1) NOT NULL default '0',
  "AAABCUDO" varchar(1) NOT NULL default '0',
  "AAABCUDP" varchar(6) NOT NULL default ' ',
  "AAABCUDQ" varchar(1) NOT NULL default '0',
  "AAABCUDR" varchar(1) NOT NULL default '0',
  "AAABCUDS" varchar(1) NOT NULL default '0',
  "AAABCUDT" varchar(6) NOT NULL default ' ',
  "AAABCUDU" varchar(1) NOT NULL default '0',
  "AAABCUDV" varchar(1) NOT NULL default '0',
  "AAABCUDW" varchar(1) NOT NULL default '0',
  "AAABCUDX" varchar(6) NOT NULL default ' ',
  "AAABCUDY" varchar(1) NOT NULL default '0',
  "AAABCUDZ" varchar(1) NOT NULL default '0',
  "AAABCUEA" varchar(1) NOT NULL default '0',
  "AAABCUEB" varchar(6) NOT NULL default ' ',
  "AAABCUEC" varchar(1) NOT NULL default '0',
  "AAABCUED" varchar(1) NOT NULL default '0',
  "AAABCUEE" varchar(1) NOT NULL default '0',
  "AAABCUEF" varchar(6) NOT NULL default ' ',
  "AAABCUEG" varchar(1) NOT NULL default '0',
  "AAABCUEH" varchar(1) NOT NULL default '0',
  "AAABCUEI" varchar(1) NOT NULL default ' ',
  "AAABCUEJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKOCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKOCY" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKOCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCUFJ" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCUFJ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCUFJ" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCUFJ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCUGF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCUGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCUGF" varchar(3) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABCUFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCUGF","AAABKQWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCUGF" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCUGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHOML" varchar(3) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHOML")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(6) NOT NULL default ' ',
  "AAABLWJT" varchar(70) NOT NULL default ' ',
  "AAABCUGS" varchar(70) NOT NULL default ' ',
  "AAABCUGT" varchar(70) NOT NULL default ' ',
  "AAABDROJ" varchar(1) NOT NULL default ' ',
  "AAABCUGV" varchar(1) NOT NULL default ' ',
  "AAABCUGW" varchar(1) NOT NULL default ' ',
  "AAABCUGX" varchar(1) NOT NULL default ' ',
  "AAABCUGY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(6) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABCUHM" varchar(1) NOT NULL default ' ',
  "AAABCUHN" varchar(1) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQFL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGLFS","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGLFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCUIT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCUIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCUIT" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCUIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABCUJK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABCUJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABCUJK" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABCUJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMIH" varchar(4) NOT NULL default ' ',
  "AAABCUJO" varchar(4) NOT NULL default ' ',
  "AAABCUJP" varchar(4) NOT NULL default ' ',
  "AAABCUJQ" varchar(4) NOT NULL default ' ',
  "AAABDIFY" varchar(1) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABLTQJ" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(1) NOT NULL default ' ',
  "AAABEMIH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMIH" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABEMIH","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEMIH" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCULO" varchar(3) NOT NULL default ' ',
  "AAABMAWF" varchar(8) NOT NULL default '00000000',
  "AAABCULC" varchar(3) NOT NULL default '000',
  "AAABCULD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCULO","AAABMAWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCULH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCULO" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCULO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCULP" (
  "AAABCULW" varchar(40) NOT NULL default ' ',
  "AAABCULX" varchar(4) NOT NULL default ' ',
  "AAABCULY" varchar(30) NOT NULL default ' ',
  "AAABKNQN" varchar(4) NOT NULL default ' ',
  "AAABFIUY" varchar(40) NOT NULL default ' ',
  "AAABFIUZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABCULW","AAABCULX","AAABCULY","AAABKNQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUMA" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABJSBY" varchar(2) NOT NULL default '00',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLX","AAABMNLY","AAABMNLW","AAABJSBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCUMZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCUMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCUMZ" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCUMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(2) NOT NULL default '00',
  "AAABDTCE" varchar(2) NOT NULL default ' ',
  "AAABDTCF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(2) NOT NULL default '00',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABCUPO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCUNV" varchar(6) NOT NULL default '000000',
  "AAABCUNW" varchar(6) NOT NULL default '000000',
  "AAABCUNX" int(11) NOT NULL default '0',
  "AAABCUPE" varchar(4) NOT NULL default ' ',
  "AAABCUNZ" varchar(6) NOT NULL default ' ',
  "AAABGCHJ" varchar(2) NOT NULL default '00',
  "AAABGCHK" varchar(4) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABCUOD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGITH","AAABCUPO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABCUPE" varchar(4) NOT NULL default ' ',
  "AAABCUOU" varchar(2) NOT NULL default '00',
  "AAABCUON" varchar(6) NOT NULL default '000000',
  "AAABCUOO" varchar(6) NOT NULL default '000000',
  "AAABCUOP" decimal(5,2) NOT NULL default '0.00',
  "AAABCUOQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGITH","AAABCUPE","AAABCUOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABCUPE" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGITH","AAABCUPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABCUPO" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGITH","AAABCUPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABKRDV" varchar(4) NOT NULL default ' ',
  "AAABIFVK" varchar(1) NOT NULL default '0',
  "AAABCUPU" varchar(4) NOT NULL default ' ',
  "AAABCUPV" varchar(4) NOT NULL default ' ',
  "AAABCUPW" varchar(4) NOT NULL default ' ',
  "AAABCUPX" varchar(4) NOT NULL default ' ',
  "AAABCUPY" varchar(4) NOT NULL default ' ',
  "AAABCUPZ" varchar(4) NOT NULL default ' ',
  "AAABCUQA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGITH","AAABKRDV","AAABIFVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABKRDV" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGITH","AAABKRDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGITH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGITH" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGITH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCURC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDCXX" varchar(1) NOT NULL default ' ',
  "AAABCVAB" varchar(8) NOT NULL default ' ',
  "AAABDCXY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCURO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO","AAABLUSC","AAABDDBQ","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABKPRZ" varchar(1) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHBE" varchar(3) NOT NULL default ' ',
  "AAABDHBG" varchar(3) NOT NULL default ' ',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDSTR" varchar(4) NOT NULL default ' ',
  "AAABDCZB" varchar(4) NOT NULL default '0000',
  "AAABDCZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO","AAABKMJV","AAABDDBQ"),
  KEY "AAABCUTB" ("AAABMRIL","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABKPRZ" varchar(1) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHBE" varchar(3) NOT NULL default ' ',
  "AAABDHBG" varchar(3) NOT NULL default ' ',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDSTR" varchar(4) NOT NULL default ' ',
  "AAABDCZB" varchar(4) NOT NULL default '0000',
  "AAABDCZC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO","AAABLUSC","AAABDDBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDDBO","AAABKMJV","AAABDDBQ","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDCYI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDDBY","AAABDDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDCZM" varchar(2) NOT NULL default ' ',
  "AAABDCZN" varchar(32) NOT NULL default ' ',
  "AAABDCZP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDCZX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKOGG" varchar(2) NOT NULL default ' ',
  "AAABCUWM" varchar(32) NOT NULL default ' ',
  "AAABCUWN" varchar(1) NOT NULL default ' ',
  "AAABCUWO" varchar(20) NOT NULL default ' ',
  "AAABCUWP" varchar(4) NOT NULL default '0000',
  "AAABCUWQ" varchar(1) NOT NULL default ' ',
  "AAABCUWR" varchar(1) NOT NULL default ' ',
  "AAABCUWS" varchar(8) NOT NULL default ' ',
  "AAABCUWT" varchar(8) NOT NULL default ' ',
  "AAABCUWU" varchar(1) NOT NULL default ' ',
  "AAABCUWV" varchar(1) NOT NULL default ' ',
  "AAABCUWW" varchar(1) NOT NULL default ' ',
  "AAABCUWX" varchar(1) NOT NULL default ' ',
  "AAABCUWY" varchar(1) NOT NULL default ' ',
  "AAABCUWZ" varchar(8) NOT NULL default ' ',
  "AAABCUXA" varchar(8) NOT NULL default ' ',
  "AAABCUXB" varchar(8) NOT NULL default ' ',
  "AAABCUXC" varchar(8) NOT NULL default ' ',
  "AAABCUXD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKOGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAABKOGG" varchar(2) NOT NULL default ' ',
  "AAABILRT" varchar(2) NOT NULL default ' ',
  "AAABHLPB" varchar(2) NOT NULL default '00',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABCUXO" varchar(2) NOT NULL default '00',
  "AAABMKWZ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFLE","AAABKOGG","AAABILRT","AAABHLPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDKXS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABDKXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDOSD" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDOSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBR","AAABDDBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  "AAABDDAO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDDBR","AAABDDBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABCVAB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABDDBY","AAABCVAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCUZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABCVAB" varchar(8) NOT NULL default ' ',
  "AAABCUZW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABDDBY","AAABCVAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKOGG" varchar(2) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABKOGG","AAABDDBY","AAABDDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(2) NOT NULL default ' ',
  "AAABCVCZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHATI","AAABCVCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(2) NOT NULL default ' ',
  "AAABCVCZ" varchar(8) NOT NULL default ' ',
  "AAABECYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABHATI","AAABCVCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(2) NOT NULL default ' ',
  "AAABCVCZ" varchar(8) NOT NULL default ' ',
  "AAABCVGH" varchar(8) NOT NULL default '00000000',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABCVBP" varchar(1) NOT NULL default ' ',
  "AAABCVBQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHATI","AAABCVCZ","AAABCVGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(2) NOT NULL default ' ',
  "AAABCVCZ" varchar(8) NOT NULL default ' ',
  "AAABCVGH" varchar(8) NOT NULL default '00000000',
  "AAABECYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABHATI","AAABCVCZ","AAABCVGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABHATI" varchar(2) NOT NULL default ' ',
  "AAABCVCZ" varchar(8) NOT NULL default ' ',
  "AAABCVGH" varchar(8) NOT NULL default '00000000',
  "AAABCVGI" varchar(4) NOT NULL default '0000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABKPRZ" varchar(1) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDCZB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABHATI","AAABCVCZ","AAABCVGH","AAABCVGI"),
  KEY "AAABCVDC" ("AAABMRIL","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVDT" varchar(12) NOT NULL default ' ',
  "AAABCVDI" varchar(32) NOT NULL default ' ',
  "AAABCVDJ" varchar(255) NOT NULL default ' ',
  "AAABCVDK" varchar(10) NOT NULL default ' ',
  "AAABCVDL" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVDR" varchar(8) NOT NULL default '00000000',
  "AAABCVDS" varchar(6) NOT NULL default '000000',
  "AAABCVDT" varchar(12) NOT NULL default ' ',
  "AAABCVDU" varchar(30) NOT NULL default ' ',
  "AAABCVDV" varchar(30) NOT NULL default ' ',
  "AAABCVDW" varchar(1) NOT NULL default ' ',
  "AAABCVDX" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABKNCK" varchar(70) NOT NULL default ' ',
  "AAABCVEA" varchar(70) NOT NULL default ' ',
  "AAABCVEB" decimal(15,0) NOT NULL default '0',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLEHI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABCVJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABIJQN" varchar(4) NOT NULL default '0000',
  "AAABCVEK" VARBINARY(255),
  "AAABCVEL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU","AAABIJQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABCVHD" varchar(2) NOT NULL default '00',
  "AAABCVHE" varchar(2) NOT NULL default '00',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVFD" ("AAABMRIL","AAABKRQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABCVHD" varchar(2) NOT NULL default '00',
  "AAABCVHE" varchar(2) NOT NULL default '00',
  "AAABKNTE" varchar(12) NOT NULL default '000000000000',
  "AAABCVFN" varchar(1) NOT NULL default ' ',
  "AAABKZYE" varchar(6) NOT NULL default ' ',
  "AAABCVFP" varchar(8) NOT NULL default '00000000',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVFX" ("AAABMRIL","AAABKNTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABCVHD" varchar(2) NOT NULL default '00',
  "AAABCVHE" varchar(2) NOT NULL default '00',
  "AAABCVGY" varchar(18) NOT NULL default '000000000000000000',
  "AAABCVGH" varchar(8) NOT NULL default '00000000',
  "AAABCVGI" varchar(4) NOT NULL default '0000',
  "AAABCVGJ" tinyint(3) unsigned NOT NULL default '0',
  "AAABCVGK" varchar(3) NOT NULL default '000',
  "AAABCVGL" varchar(2) NOT NULL default '00',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  "AAABCVGS" varchar(4) NOT NULL default ' ',
  "AAABCVGT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVGW" ("AAABMRIL","AAABCVGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABCVHD" varchar(2) NOT NULL default '00',
  "AAABCVHE" varchar(2) NOT NULL default '00',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCEY" varchar(10) NOT NULL default ' ',
  "AAABCVHT" varchar(15) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVHR" ("AAABMRIL","AAABCVHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  "AAABIMBK" varchar(40) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVII" ("AAABMRIL","AAABKRQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAABCVIU" varchar(4) NOT NULL default '0000',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVJC" ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRF","AAABIMRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVJU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABCVJL" varchar(2) NOT NULL default ' ',
  "AAABCVJX" varchar(12) NOT NULL default '000000000000',
  "AAABKNTE" varchar(12) NOT NULL default '000000000000',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVJS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVJU"),
  KEY "AAABCVJV" ("AAABMRIL","AAABCVJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABCVKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  "AAABCVKK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABCVNO","AAABCVKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVKT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABCVNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  "AAABCVLC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABCVNO","AAABCVNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVOB" varchar(1) NOT NULL default ' ',
  "AAABCVOC" varchar(1) NOT NULL default ' ',
  "AAABCVOD" varchar(1) NOT NULL default ' ',
  "AAABCVOE" varchar(1) NOT NULL default ' ',
  "AAABCVOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABEDAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVLY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINAW","AAABCVNO","AAABEDAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVMN" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABEDAO","AAABCVNR","AAABCVMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABEDAO","AAABCVNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  "AAABCVNC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABEDAO","AAABCVNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNO" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABCVNQ" varchar(5) NOT NULL default '00000',
  "AAABCVNR" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABCVNO","AAABEDAO","AAABCVNQ","AAABCVNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVNX" varchar(2) NOT NULL default ' ',
  "AAABCVOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVOH" varchar(8) NOT NULL default ' ',
  "AAABCVOB" varchar(1) NOT NULL default ' ',
  "AAABCVOC" varchar(1) NOT NULL default ' ',
  "AAABCVOD" varchar(1) NOT NULL default ' ',
  "AAABCVOE" varchar(1) NOT NULL default ' ',
  "AAABCVOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVOI" (
  "AAABKRRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKRRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVOL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRRG" varchar(1) NOT NULL default ' ',
  "AAABCVOO" varchar(10) NOT NULL default ' ',
  "AAABCVOP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKRRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVPE" varchar(3) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABCVPG" varchar(10) NOT NULL default ' ',
  "AAABLSTU" varchar(3) NOT NULL default '000',
  "AAABIHLK" varchar(2) NOT NULL default '00',
  "AAABCVOZ" varchar(1) NOT NULL default ' ',
  "AAABCVPA" varchar(1) NOT NULL default ' ',
  "AAABCVPB" varchar(1) NOT NULL default ' ',
  "AAABCVPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVPE","AAABHFFQ","AAABCVPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABCVPK" varchar(40) NOT NULL default ' ',
  "AAABCVPL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCVPU" varchar(4) NOT NULL default ' ',
  "AAABHFFQ" varchar(30) NOT NULL default ' ',
  "AAABCVPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCVPU","AAABHFFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default '0000',
  "AAABGPOF" decimal(2,0) NOT NULL default '0',
  "AAABGPOG" decimal(2,0) NOT NULL default '0',
  "AAABGPOH" decimal(2,0) NOT NULL default '0',
  "AAABGPOI" decimal(2,0) NOT NULL default '0',
  "AAABCVQI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIKF","AAABMNLX","AAABMNLY","AAABKRBW","AAABKQWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAABDOLV" varchar(1) NOT NULL default ' ',
  "AAABLOOI" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABDOLY" varchar(10) NOT NULL default ' ',
  "AAABDOLZ" varchar(4) NOT NULL default ' ',
  "AAABDOMA" varchar(4) NOT NULL default ' ',
  "AAABDOMB" varchar(4) NOT NULL default ' ',
  "AAABDOMC" varchar(10) NOT NULL default ' ',
  "AAABKQLH" varchar(3) NOT NULL default ' ',
  "AAABKQJS" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABDOMJ" varchar(2) NOT NULL default ' ',
  "AAABKOEW" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGJBT" varchar(1) NOT NULL default ' ',
  "AAABDOMN" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDONB" varchar(2) NOT NULL default '00',
  "AAABKNRS" varchar(3) NOT NULL default ' ',
  "AAABEMIH" varchar(3) NOT NULL default ' ',
  "AAABLRBI" varchar(3) NOT NULL default ' ',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABKODF" varchar(5) NOT NULL default ' ',
  "AAABCVRS" varchar(1) NOT NULL default ' ',
  "AAABKQJX" decimal(13,3) NOT NULL default '0.000',
  "AAABKOCM" varchar(4) NOT NULL default ' ',
  "AAABCVRV" varchar(4) NOT NULL default '0000',
  "AAABCVRW" varchar(4) NOT NULL default '0000',
  "AAABCVRX" varchar(4) NOT NULL default '0000',
  "AAABCVRY" varchar(4) NOT NULL default '0000',
  "AAABCVRZ" varchar(3) NOT NULL default ' ',
  "AAABCVSA" decimal(5,2) NOT NULL default '0.00',
  "AAABCVSB" varchar(4) NOT NULL default '0000',
  "AAABDOMS" varchar(6) NOT NULL default ' ',
  "AAABDOMT" varchar(6) NOT NULL default ' ',
  "AAABDOMU" varchar(6) NOT NULL default ' ',
  "AAABDOMV" varchar(6) NOT NULL default ' ',
  "AAABCVSG" varchar(4) NOT NULL default ' ',
  "AAABCVSH" varchar(12) NOT NULL default ' ',
  "AAABDOMX" varchar(6) NOT NULL default ' ',
  "AAABDOMW" varchar(6) NOT NULL default ' ',
  "AAABCVSK" varchar(2) NOT NULL default ' ',
  "AAABCVSL" varchar(4) NOT NULL default ' ',
  "AAABCVSM" varchar(4) NOT NULL default ' ',
  "AAABGJBU" varchar(1) NOT NULL default ' ',
  "AAABDTRQ" varchar(1) NOT NULL default ' ',
  "AAABCVSP" varchar(1) NOT NULL default ' ',
  "AAABDOMD" varchar(10) NOT NULL default ' ',
  "AAABDOME" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABDQTT" varchar(12) NOT NULL default ' ',
  "AAABCVSU" varchar(12) NOT NULL default ' ',
  "AAABLURA" varchar(3) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABDOOB" varchar(2) NOT NULL default ' ',
  "AAABDOOC" varchar(2) NOT NULL default ' ',
  "AAABDOOD" varchar(2) NOT NULL default ' ',
  "AAABCVTA" varchar(1) NOT NULL default ' ',
  "AAABDDUF" varchar(3) NOT NULL default '000',
  "AAABKOCY" varchar(4) NOT NULL default ' ',
  "AAABIIVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBW","AAABHQFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABHQFG" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRBW","AAABHQFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVTR" (
  "AAABCVUA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCVUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVTU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCVUA" varchar(1) NOT NULL default ' ',
  "AAABCVTX" varchar(10) NOT NULL default ' ',
  "AAABCVTY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCVUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVUB" (
  "AAABKNOL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVUE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNOL" varchar(1) NOT NULL default ' ',
  "AAABCVUH" varchar(10) NOT NULL default ' ',
  "AAABCVUI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKNOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVUL" (
  "AAABCVUT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCVUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVUO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCVUT" varchar(2) NOT NULL default ' ',
  "AAABCVUR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABCVUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABDREL" varchar(6) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBW","AAABDREL","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABDREL" varchar(6) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBW","AAABDREL","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVVS" (
  "AAABMLBW" varchar(1) NOT NULL default ' ',
  "AAABCVWD" varchar(10) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLBW","AAABCVWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVVY" (
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABMLBW" varchar(1) NOT NULL default ' ',
  "AAABCVWG" varchar(4) NOT NULL default '0000',
  "AAABJUQY" varchar(3) NOT NULL default '000',
  "AAABCVWD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABKQWU","AAABMLBW","AAABCVWG","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVWI" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHFMC" varchar(1) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABHFME" varchar(1) NOT NULL default ' ',
  "AAABHFMF" varchar(30) NOT NULL default ' ',
  "AAABHFMG" varchar(30) NOT NULL default ' ',
  "AAABHFMH" varchar(1) NOT NULL default ' ',
  "AAABHFMI" varchar(18) NOT NULL default ' ',
  "AAABHFMJ" varchar(1) NOT NULL default ' ',
  "AAABHFMK" varchar(30) NOT NULL default ' ',
  "AAABHFML" varchar(30) NOT NULL default ' ',
  "AAABHFMM" varchar(30) NOT NULL default ' ',
  "AAABHFMN" varchar(6) NOT NULL default '000000',
  "AAABHFSY" varchar(30) NOT NULL default ' ',
  "AAABHFMP" varchar(20) NOT NULL default ' ',
  "AAABHFMQ" varchar(20) NOT NULL default ' ',
  "AAABHFMR" varchar(30) NOT NULL default ' ',
  "AAABHFMS" varchar(3) NOT NULL default ' ',
  "AAABHFMT" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  "AAABHFMV" varchar(4) NOT NULL default ' ',
  "AAABHFMW" varchar(30) NOT NULL default ' ',
  "AAABHFMX" varchar(1) NOT NULL default ' ',
  "AAABHFMY" varchar(1) NOT NULL default ' ',
  "AAABHFMZ" varchar(1) NOT NULL default ' ',
  "AAABHFNA" varchar(1) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABHFND" varchar(1) NOT NULL default ' ',
  "AAABHFNE" varchar(3) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  "AAABHFNH" varchar(1) NOT NULL default ' ',
  "AAABHFNI" varchar(1) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABJLRA" varchar(3) NOT NULL default '000',
  "AAABJFAA" varchar(5) NOT NULL default ' ',
  "AAABHFNM" varchar(30) NOT NULL default ' ',
  "AAABHFNN" varchar(30) NOT NULL default ' ',
  "AAABHFNO" varchar(30) NOT NULL default ' ',
  "AAABHFNP" varchar(1) NOT NULL default ' ',
  "AAABHFNQ" varchar(5) NOT NULL default ' ',
  "AAABHFNR" varchar(3) NOT NULL default ' ',
  "AAABHFNS" varchar(30) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  "AAABHFNV" varchar(31) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLECD","AAABIHHB"),
  KEY "AAABCVYK" ("AAABHFMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVYM" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABGYQM","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVYT" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABJMGZ" varchar(4) NOT NULL default ' ',
  "AAABEPKH" varchar(30) NOT NULL default ' ',
  "AAABEPKI" varchar(30) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABEPQQ","AAABJMGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCVZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBHQ" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBHQ","AAABLTXI"),
  KEY "AAABCWAP" ("AAABMRIL","AAABLECD","AAABDBHQ","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBHQ" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABFITE" varchar(4) NOT NULL default ' ',
  "AAABCWBL" varchar(2) NOT NULL default '00',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIYCS","AAABLTXI"),
  KEY "AAABCWCC" ("AAABMRIL","AAABLECD","AAABIYCS","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZUH" varchar(12) NOT NULL default '000000000000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZUH","AAABIYCS","AAABLTXI"),
  KEY "AAABCWDF" ("AAABMRIL","AAABLECD","AAABCZUH","AAABIYCS","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFITE" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCWDR" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFITE","AAABLTXI"),
  KEY "AAABCWEI" ("AAABMRIL","AAABLECD","AAABFITE","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFITE" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABFITE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWFH" varchar(4) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCWFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBUA" varchar(5) NOT NULL default ' ',
  "AAABDBQE" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBUA","AAABDBQE","AAABLTXI"),
  KEY "AAABCWGG" ("AAABMRIL","AAABLECD","AAABDBUA","AAABDBQE","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYQA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWHE" (
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABCXTG" varchar(4) NOT NULL default '0000',
  "AAABCXTH" varchar(30) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCYQA" varchar(2) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCYQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVHD" varchar(1) NOT NULL default ' ',
  "AAABMECK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWIE" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABCWIH" varchar(6) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABCWJS" varchar(3) NOT NULL default ' ',
  "AAABCWJT" varchar(1) NOT NULL default ' ',
  "AAABCWIL" varchar(4) NOT NULL default ' ',
  "AAABLEML" varchar(4) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABHGFM" varchar(14) NOT NULL default '00000000000000',
  "AAABCWJW" varchar(20) NOT NULL default ' ',
  "AAABCWIQ" varchar(20) NOT NULL default ' ',
  "AAABCWJX" varchar(22) NOT NULL default ' ',
  "AAABCWKL" varchar(6) NOT NULL default ' ',
  "AAABJBGU" varchar(4) NOT NULL default ' ',
  "AAABCWIU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWIW" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABCWIZ" varchar(20) NOT NULL default ' ',
  "AAABLMHC" varchar(14) NOT NULL default '00000000000000',
  "AAABCWJZ" varchar(22) NOT NULL default ' ',
  "AAABCWJC" varchar(14) NOT NULL default '00000000000000',
  "AAABCWJD" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWJG" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJLQK" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWJL" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWJO" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABCWJR" varchar(8) NOT NULL default ' ',
  "AAABCWJS" varchar(3) NOT NULL default ' ',
  "AAABCWJT" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABHGFM" varchar(14) NOT NULL default '00000000000000',
  "AAABCWJW" varchar(20) NOT NULL default ' ',
  "AAABCWJX" varchar(22) NOT NULL default ' ',
  "AAABLMHC" varchar(14) NOT NULL default '00000000000000',
  "AAABCWJZ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWKB" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAABJBGU" varchar(10) NOT NULL default ' ',
  "AAABLEML" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABFVDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWKI" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWKK" varchar(4) NOT NULL default ' ',
  "AAABCWKL" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWKN" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDABU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWKS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWLF" varchar(4) NOT NULL default ' ',
  "AAABCWLW" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWLF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWMA" varchar(3) NOT NULL default ' ',
  "AAABCWMB" varchar(3) NOT NULL default ' ',
  "AAABCWLW" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWMA","AAABCWMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABCWMA" varchar(3) NOT NULL default ' ',
  "AAABCWMB" varchar(3) NOT NULL default ' ',
  "AAABCWLW" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLPNX","AAABCWMA","AAABCWMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWNK" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABDBPB" varchar(4) NOT NULL default ' ',
  "AAABKAOU" varchar(4) NOT NULL default ' ',
  "AAABCWMJ" varchar(1) NOT NULL default ' ',
  "AAABDGUR" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWNK","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWNK" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCWNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKWSS","AAABKVKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBMB","AAABHDVT","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABDKYU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCWSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABCWPN" varchar(12) NOT NULL default ' ',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABCWSB" decimal(15,2) NOT NULL default '0.00',
  "AAABCWSC" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABCWSF" varchar(3) NOT NULL default ' ',
  "AAABCWSG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWSJ","AAABGBAU","AAABLRRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default ' ',
  "AAABCWQQ" decimal(13,3) NOT NULL default '0.000',
  "AAABCWQR" decimal(13,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABCWSF" varchar(3) NOT NULL default ' ',
  "AAABCWSG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWSJ","AAABGBAU","AAABLRRK","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default ' ',
  "AAABCWRJ" varchar(1) NOT NULL default ' ',
  "AAABCWRK" varchar(2) NOT NULL default ' ',
  "AAABCWRL" varchar(20) NOT NULL default ' ',
  "AAABCWRM" varchar(20) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWSJ","AAABGBAU","AAABLRRK","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWSJ" varchar(4) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABCWSL" varchar(100) NOT NULL default ' ',
  "AAABCWSB" decimal(15,2) NOT NULL default '0.00',
  "AAABCWSC" decimal(15,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABCWSF" varchar(3) NOT NULL default ' ',
  "AAABCWSG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWSJ","AAABGBAU","AAABCWSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWSM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCWSY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCWSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWSR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWSY" varchar(3) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABCWSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWTG" varchar(3) NOT NULL default ' ',
  "AAABCWTD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWUB" varchar(3) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWUB" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCWUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXNF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXNF" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCXNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABDTFE" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBMB","AAABDTFE","AAABLTXI"),
  KEY "AAABCWVQ" ("AAABMRIL","AAABLECD","AAABDBMB","AAABDTFE","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCWZQ" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCWZQ","AAABLTXI"),
  KEY "AAABCWWS" ("AAABMRIL","AAABLECD","AAABCWZQ","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCWZQ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCWZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABCWZQ" varchar(3) NOT NULL default ' ',
  "AAABKWUH" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBNT","AAABCWZQ","AAABKWUH","AAABLTXI"),
  KEY "AAABCWYH" ("AAABMRIL","AAABLECD","AAABDBNT","AAABCWZQ","AAABKWUH","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABCWZQ" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBNT","AAABCWZQ","AAABLTXI"),
  KEY "AAABCWZM" ("AAABMRIL","AAABLECD","AAABDBNT","AAABCWZQ","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCWZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAAO" varchar(5) NOT NULL default ' ',
  "AAABCWZZ" varchar(10) NOT NULL default ' ',
  "AAABIMCJ" varchar(12) NOT NULL default '000000000000',
  "AAABCXAB" varchar(8) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBNT","AAABLTXI"),
  KEY "AAABCXAS" ("AAABMRIL","AAABLECD","AAABDBNT","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXAY" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDBNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXBD" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS","AAABDBNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXBL" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABCXTU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDBNT","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIFGN" varchar(16) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIFGN" varchar(16) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABIFGN" varchar(16) NOT NULL default ' ',
  "AAABCXEE" varchar(4) NOT NULL default '0000',
  "AAABCXEF" varchar(10) NOT NULL default ' ',
  "AAABCXDY" varchar(6) NOT NULL default '000000',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABIFGN","AAABCXEE","AAABCXEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIFGN" varchar(16) NOT NULL default ' ',
  "AAABCXEE" varchar(4) NOT NULL default '0000',
  "AAABCXEF" varchar(10) NOT NULL default ' ',
  "AAABCXDY" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABIFGN","AAABCXEE","AAABCXEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXEO" varchar(10) NOT NULL default ' ',
  "AAABMJWH" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXEO","AAABMJWH","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXFA" varchar(2) NOT NULL default ' ',
  "AAABDAUM" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNRG" decimal(5,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXFA","AAABDAUM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAXN" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABCXGL" varchar(30) NOT NULL default ' ',
  "AAABCXGM" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABCXFX" varchar(30) NOT NULL default ' ',
  "AAABCXFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAXN","AAABMQIJ","AAABCXGL","AAABCXGM","AAABLTXI"),
  KEY "AAABCXGG" ("AAABMRIL","AAABLECD","AAABDAXN","AAABMQIJ","AAABCXGL","AAABCXGM","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAXN" varchar(2) NOT NULL default ' ',
  "AAABCXHA" varchar(5) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABCXGV" varchar(1) NOT NULL default ' ',
  "AAABCXGW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAXN","AAABCXHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXKI" varchar(4) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCXHI" varchar(2) NOT NULL default '00',
  "AAABCXIR" varchar(1) NOT NULL default ' ',
  "AAABCXHK" decimal(10,5) NOT NULL default '0.00000',
  "AAABCXHL" decimal(10,5) NOT NULL default '0.00000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXKI","AAABDBMB","AAABLCCC","AAABLTXI"),
  KEY "AAABCXIE" ("AAABMRIL","AAABLECD","AAABCXKI","AAABDBMB","AAABLCCC","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXKI" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCXIR" varchar(1) NOT NULL default ' ',
  "AAABCXIS" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXKI","AAABLTXI"),
  KEY "AAABCXJJ" ("AAABMRIL","AAABLECD","AAABCXKI","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXJX" varchar(4) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABCXJU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXJX","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXKI" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCXKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXMD" varchar(4) NOT NULL default ' ',
  "AAABCXLJ" varchar(3) NOT NULL default ' ',
  "AAABCXLF" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXMD","AAABCXLJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXMD" varchar(4) NOT NULL default ' ',
  "AAABCXLN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCXMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXMD" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABCXMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXLY" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXMD" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABCXMD","AAABMNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABMLTD" varchar(7) NOT NULL default '0000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKWSS","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXNF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXNF" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCXNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCC","AAABDBMB","AAABLTXI"),
  KEY "AAABCXOD" ("AAABMRIL","AAABLECD","AAABLCCC","AAABDBMB","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABCXOS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXPG" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXPQ" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABCXTG" varchar(4) NOT NULL default '0000',
  "AAABCXTH" varchar(30) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABCXPQ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCXSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCXSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABCXSJ" varchar(30) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ","AAABCXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABCXSJ" varchar(30) NOT NULL default ' ',
  "AAABCXSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ","AAABCXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXSZ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCXSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXTA" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAJM" varchar(30) NOT NULL default ' ',
  "AAABCZNS" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABCXTG" varchar(4) NOT NULL default '0000',
  "AAABCXTH" varchar(30) NOT NULL default ' ',
  "AAABCXTU" varchar(32) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDAJM","AAABCZNS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXTP" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCXTU" varchar(32) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABCXTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYPX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCYPX" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCYPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYID" varchar(1) NOT NULL default ' ',
  "AAABCYIE" varchar(1) NOT NULL default ' ',
  "AAABCYIF" varchar(1) NOT NULL default ' ',
  "AAABCYIG" varchar(1) NOT NULL default ' ',
  "AAABCYIH" varchar(1) NOT NULL default ' ',
  "AAABCYII" varchar(1) NOT NULL default ' ',
  "AAABCYIJ" varchar(1) NOT NULL default ' ',
  "AAABCYIK" varchar(1) NOT NULL default ' ',
  "AAABCYIL" varchar(2) NOT NULL default '00',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXVO" (
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABCXVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXVS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXWH" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABDBJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXWN" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXWV" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABDBJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXXB" (
  "AAABCXXM" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABCXXF" varchar(1) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABCXXI" varchar(1) NOT NULL default ' ',
  "AAABCXXJ" varchar(1) NOT NULL default ' ',
  "AAABCXXK" varchar(3) NOT NULL default ' ',
  "AAABCXXL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCXXM","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXXV" (
  "AAABMMVV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAABGGSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYFE" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABHDVT","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABHDVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCXZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABCYFF" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYFA" varchar(2) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABHDVT","AAABCYFF","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABCYFF" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABCZUH" varchar(12) NOT NULL default '000000000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABHDVT","AAABCYFF","AAABMLZS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABCYFF" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDAQJ" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABHDVT","AAABCYFF","AAABMLZS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYFA" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default ' ',
  "AAABCYCZ" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYFA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHDVT" varchar(8) NOT NULL default ' ',
  "AAABCYFF" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABHDVT","AAABCYFF","AAABMLZS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCYFA" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCYFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYFB" (
  "AAABCYFE" varchar(1) NOT NULL default ' ',
  "AAABCYFF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCYFE","AAABCYFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCYGA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKWSS","AAABCYGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABCYGA" varchar(4) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABKWSS","AAABCYGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYGI" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCYGQ" varchar(1) NOT NULL default ' ',
  "AAABCYGL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYGN" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCYGQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYHN" (
  "AAABCYHQ" varchar(30) NOT NULL default ' ',
  "AAABCYHR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCYHQ","AAABCYHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYHS" (
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYIC" varchar(5) NOT NULL default ' ',
  "AAABCYID" varchar(1) NOT NULL default ' ',
  "AAABCYIE" varchar(1) NOT NULL default ' ',
  "AAABCYIF" varchar(1) NOT NULL default ' ',
  "AAABCYIG" varchar(1) NOT NULL default ' ',
  "AAABCYIH" varchar(1) NOT NULL default ' ',
  "AAABCYII" varchar(1) NOT NULL default ' ',
  "AAABCYIJ" varchar(1) NOT NULL default ' ',
  "AAABCYIK" varchar(1) NOT NULL default ' ',
  "AAABCYIL" varchar(2) NOT NULL default '00',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLTXI"),
  KEY "AAABCYJB" ("AAABMRIL","AAABLECD","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYJG" (
  "AAABEZNY" varchar(30) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABCYJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYJL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEZNY" varchar(30) NOT NULL default ' ',
  "AAABLFZO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEZNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYJR" (
  "AAABCYJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABCYJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYJU" (
  "AAABEZNY" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCYJX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEZNY","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYKA" (
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX","AAABGBAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYKF" (
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  "AAABGBAU" varchar(30) NOT NULL default ' ',
  "AAABLRRK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX","AAABGBAU","AAABLRRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLNTL" varchar(1) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABCYKQ" varchar(32) NOT NULL default ' ',
  "AAABCYKR" varchar(32) NOT NULL default ' ',
  "AAABCYKS" varchar(32) NOT NULL default ' ',
  "AAABCYKT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLNTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default ' ',
  "AAABCYLC" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMLDY","AAABLTKC","AAABLSIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYLP" varchar(1) NOT NULL default ' ',
  "AAABLEJW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYLT" varchar(1) NOT NULL default ' ',
  "AAABCYLU" varchar(1) NOT NULL default ' ',
  "AAABCYLV" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDANC" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYMD" varchar(1) NOT NULL default ' ',
  "AAABCYME" varchar(4) NOT NULL default '0000',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDANC","AAABLTXI"),
  KEY "AAABCYMW" ("AAABMRIL","AAABLECD","AAABDANC","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDANC" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDANC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKWUH" varchar(1) NOT NULL default ' ',
  "AAABCYOP" varchar(1) NOT NULL default ' ',
  "AAABCYOQ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCYNT" varchar(2) NOT NULL default '00',
  "AAABCYNU" varchar(30) NOT NULL default ' ',
  "AAABCYNV" varchar(8) NOT NULL default ' ',
  "AAABCZXH" varchar(1) NOT NULL default '0',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABCYNY" varchar(30) NOT NULL default ' ',
  "AAABCYNZ" varchar(2) NOT NULL default '00',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKWUH","AAABCYOP","AAABCYOQ","AAABLTXI"),
  KEY "AAABCYOS" ("AAABMRIL","AAABLECD","AAABKWUH","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYPO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCYPO" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCYPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCYRG" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABEPOT" varchar(1) NOT NULL default ' ',
  "AAABDBEC" varchar(10) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAABCYPX" varchar(20) NOT NULL default ' ',
  "AAABCYPY" varchar(10) NOT NULL default ' ',
  "AAABCYPZ" varchar(4) NOT NULL default ' ',
  "AAABCYQA" varchar(2) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCYRG","AAABLTXI"),
  KEY "AAABCYQR" ("AAABMRIL","AAABLECD","AAABCYRG","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCYRG" varchar(4) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCYRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCYSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCYSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCYSJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCYSJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCYSJ" varchar(1) NOT NULL default ' ',
  "AAABCYSK" varchar(2) NOT NULL default ' ',
  "AAABCYRY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCYSJ","AAABCYSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCYSJ" varchar(1) NOT NULL default ' ',
  "AAABCYSK" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCYSJ","AAABCYSK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYSM" (
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABCYXV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABCZAP","AAABLJJN","AAABCYXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYSV" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABCYXV" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAABCYZC" varchar(30) NOT NULL default ' ',
  "AAABCYTX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLJJN","AAABCYXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYTG" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABCYXV" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS","AAABLJJN","AAABCYXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYTQ" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAABCYZC" varchar(30) NOT NULL default ' ',
  "AAABCYTX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLFYC","AAABCZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYUB" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLFYC","AAABCZAP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYUL" (
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABCYUO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABCZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYUR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLFYC","AAABCZAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLTWR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYVX" (
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYWA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYWG" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABCYXV" varchar(2) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABLFYC","AAABCZAP","AAABLJJN","AAABCYXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABCZAP" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABCYXV" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABCYXD" varchar(2) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABCZAP","AAABLJJN","AAABCYXV","AAABLTWR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDBPU" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLJJN","AAABLTWR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYYZ" (
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCYZC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYZE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLJJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCYZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABCZAP" varchar(40) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLTWR","AAABCZAP","AAABLTXI"),
  KEY "AAABCZAK" ("AAABMRIL","AAABLECD","AAABLFYC","AAABLTWR","AAABCZAP","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBUB","AAABLTXI"),
  KEY "AAABCZBO" ("AAABMRIL","AAABLECD","AAABDBUB","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAABCZCA" varchar(30) NOT NULL default ' ',
  "AAABCZCB" varchar(30) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLJJN","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZCS" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABCZCU" varchar(1) NOT NULL default ' ',
  "AAABCZCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZCX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZDD" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABCZHD" varchar(30) NOT NULL default ' ',
  "AAABCZHE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMPMH","AAABCZHD","AAABCZHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABCZHD" varchar(30) NOT NULL default ' ',
  "AAABCZHE" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZDU" varchar(1) NOT NULL default ' ',
  "AAABCZDV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMPMH","AAABLHPM","AAABCZHD","AAABCZHE","AAABLTXI"),
  KEY "AAABCZEQ" ("AAABMRIL","AAABLECD","AAABMPMH","AAABCZHD","AAABCZHE","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABCZHD" varchar(30) NOT NULL default ' ',
  "AAABCZHE" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABMPMH","AAABLHPM","AAABCZHD","AAABCZHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABLHPM" varchar(4) NOT NULL default ' ',
  "AAABCZHD" varchar(30) NOT NULL default ' ',
  "AAABCZHE" varchar(30) NOT NULL default ' ',
  "AAABCZHF" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZFX" varchar(30) NOT NULL default ' ',
  "AAABCZHN" varchar(30) NOT NULL default ' ',
  "AAABCZFZ" varchar(2) NOT NULL default ' ',
  "AAABCZGA" decimal(7,3) NOT NULL default '0.000',
  "AAABCZGB" decimal(7,3) NOT NULL default '0.000',
  "AAABCZGC" varchar(30) NOT NULL default ' ',
  "AAABCZGD" varchar(1) NOT NULL default ' ',
  "AAABCZGE" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMPMH","AAABLHPM","AAABCZHD","AAABCZHE","AAABCZHF","AAABLTXI"),
  KEY "AAABCZGZ" ("AAABMRIL","AAABLECD","AAABMPMH","AAABCZHD","AAABCZHE","AAABCZHF","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZHI" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABCZHN" varchar(30) NOT NULL default ' ',
  "AAABCZHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABCZHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZHO" (
  "AAABJZVA" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCZHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZVA","AAABMODI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZHU" (
  "AAABJZVA" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABCZHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJZVA","AAABMODI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZIA" (
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABCZIC" varchar(150) NOT NULL default ' ',
  "AAABCZID" varchar(150) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABCZIJ" varchar(1) NOT NULL default ' ',
  "AAABCZIK" varchar(1) NOT NULL default ' ',
  "AAABCZIL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABCZIY" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABCZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZIZ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLDNU" varchar(1) NOT NULL default ' ',
  "AAABCZJC" varchar(1) NOT NULL default ' ',
  "AAABCZJD" varchar(1) NOT NULL default ' ',
  "AAABCZJE" varchar(1) NOT NULL default ' ',
  "AAABCZJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZJH" (
  "AAABDELY" varchar(42) NOT NULL default ' ',
  "AAABKNCF" varchar(6) NOT NULL default ' ',
  "AAABCZJK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDELY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZJM" (
  "AAABCZKY" varchar(4) NOT NULL default '0000',
  "AAABCZJO" varchar(46) NOT NULL default ' ',
  "AAABCZJP" varchar(46) NOT NULL default ' ',
  "AAABCZJQ" varchar(46) NOT NULL default ' ',
  "AAABCZJR" varchar(46) NOT NULL default ' ',
  "AAABCZJS" varchar(46) NOT NULL default ' ',
  "AAABCZJT" varchar(46) NOT NULL default ' ',
  "AAABCZJU" varchar(46) NOT NULL default ' ',
  "AAABCZJV" int(11) NOT NULL default '0',
  "AAABHGII" int(11) NOT NULL default '0',
  "AAABCZJX" varchar(30) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABCZJZ" varchar(12) NOT NULL default ' ',
  "AAABLOZE" int(11) NOT NULL default '0',
  "AAABIIZX" int(11) NOT NULL default '0',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  "AAABCZKD" varchar(1) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  "AAABHVRP" int(11) NOT NULL default '0',
  "AAABCZKG" varchar(46) NOT NULL default ' ',
  "AAABCZKH" varchar(1) NOT NULL default ' ',
  "AAABCZKI" varchar(1) NOT NULL default ' ',
  "AAABLTUD" int(11) NOT NULL default '0',
  "AAABJSDF" int(11) NOT NULL default '0',
  "AAABCZKL" varchar(1) NOT NULL default ' ',
  "AAABCZKM" varchar(1) NOT NULL default ' ',
  "AAABCZKN" varchar(1) NOT NULL default ' ',
  "AAABCZKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCZKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZKQ" (
  "AAABCZKY" varchar(4) NOT NULL default '0000',
  "AAABCZKS" int(11) NOT NULL default '0',
  "AAABCZKT" int(11) NOT NULL default '0',
  "AAABCZKU" varchar(4) NOT NULL default ' ',
  "AAABCZKV" varchar(4) NOT NULL default ' ',
  "AAABCZKW" varchar(1) NOT NULL default ' ',
  "AAABJEXI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABCZKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFSXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABFSXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFSXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABFSXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZMH" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAJM" varchar(30) NOT NULL default ' ',
  "AAABCZNS" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDAJM","AAABCZNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZMP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAAO" varchar(5) NOT NULL default ' ',
  "AAABDBHY" varchar(12) NOT NULL default '000000000000',
  "AAABDBRR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLECD","AAABDAAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZMY" (
  "AAABCZNE" varchar(30) NOT NULL default ' ',
  "AAABCZNF" varchar(1) NOT NULL default ' ',
  "AAABCZNM" varchar(30) NOT NULL default ' ',
  "AAABCZNH" varchar(1) NOT NULL default ' ',
  "AAABIAET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCZNE","AAABCZNF","AAABCZNM","AAABCZNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZNI" (
  "AAABCZNM" varchar(30) NOT NULL default ' ',
  "AAABDAJM" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABCZNM","AAABDAJM","AAABIRTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZNP" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAAO" varchar(5) NOT NULL default ' ',
  "AAABCZNS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLECD","AAABDAAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZNV" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAAO" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS","AAABDAAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCZOR" varchar(3) NOT NULL default ' ',
  "AAABCZOP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZSF" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZSF","AAABLTXI"),
  KEY "AAABCZPN" ("AAABMRIL","AAABLECD","AAABCZSF","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZSE" varchar(4) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZSE","AAABLTYA","AAABLTXI"),
  KEY "AAABCZQQ" ("AAABMRIL","AAABLECD","AAABCZSE","AAABLTYA","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZSE" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZSE","AAABLTXI"),
  KEY "AAABCZRS" ("AAABMRIL","AAABLECD","AAABCZSE","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZTK" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZWP" varchar(4) NOT NULL default ' ',
  "AAABCZSE" varchar(4) NOT NULL default ' ',
  "AAABCZSF" varchar(4) NOT NULL default ' ',
  "AAABCZSG" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZTK","AAABLTXI"),
  KEY "AAABCZSX" ("AAABMRIL","AAABLECD","AAABCZTK","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZUG" varchar(2) NOT NULL default ' ',
  "AAABCZUH" varchar(12) NOT NULL default '000000000000',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZTK" varchar(10) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZUG","AAABCZUH","AAABMNRG","AAABLTXI"),
  KEY "AAABCZUD" ("AAABMRIL","AAABLECD","AAABCZUG","AAABCZUH","AAABMNRG","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBQH" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZUQ" varchar(2) NOT NULL default '00',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBQH","AAABLTXI"),
  KEY "AAABCZVH" ("AAABMRIL","AAABLECD","AAABDBQH","AAABDBUY","AAABDBUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABCZWP" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZVS" varchar(1) NOT NULL default ' ',
  "AAABCZVT" varchar(1) NOT NULL default ' ',
  "AAABCZVU" varchar(1) NOT NULL default ' ',
  "AAABDBHY" varchar(12) NOT NULL default '000000000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABCZWP","AAABLTXI"),
  KEY "AAABCZWM" ("AAABMRIL","AAABLECD","AAABCZWP","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBQH" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZXG" varchar(8) NOT NULL default ' ',
  "AAABCZXH" varchar(1) NOT NULL default '0',
  "AAABCZXI" varchar(8) NOT NULL default ' ',
  "AAABCZXJ" varchar(8) NOT NULL default ' ',
  "AAABCZXK" varchar(8) NOT NULL default ' ',
  "AAABCZXL" varchar(8) NOT NULL default ' ',
  "AAABCZXM" varchar(30) NOT NULL default ' ',
  "AAABCZXN" varchar(30) NOT NULL default ' ',
  "AAABCZXO" varchar(30) NOT NULL default ' ',
  "AAABLILW" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLTXI"),
  KEY "AAABCZYF" ("AAABMRIL","AAABLECD","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABCZYR" varchar(1) NOT NULL default ' ',
  "AAABCZYS" varchar(4) NOT NULL default '0000',
  "AAABCZYT" varchar(4) NOT NULL default '0000',
  "AAABCZYU" varchar(1) NOT NULL default ' ',
  "AAABCZYV" varchar(30) NOT NULL default ' ',
  "AAABCZYW" varchar(30) NOT NULL default ' ',
  "AAABCZYX" varchar(2) NOT NULL default ' ',
  "AAABCZYY" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLJJN","AAABLTWR","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABCZZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAAH" varchar(8) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAAH","AAABLDTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAAH" varchar(8) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAAH","AAABLDTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAAJ" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAAO" varchar(5) NOT NULL default ' ',
  "AAABDAAM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABDAAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAAP" (
  "AAABDABU" varchar(6) NOT NULL default ' ',
  "AAABMMVV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDABU","AAABMMVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAAU" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABDABU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAAY" (
  "AAABDABU" varchar(6) NOT NULL default ' ',
  "AAABDABA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDABC" (
  "AAABDABU" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDABU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDABI" (
  "AAABDABU" varchar(6) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDABO" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABDABU","AAABMNML","AAABDABO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDABP" (
  "AAABDABU" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABDAJM" varchar(40) NOT NULL default ' ',
  "AAABDAIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDABU","AAABMOHM","AAABDAJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDABX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBPT" varchar(4) NOT NULL default ' ',
  "AAABDACB" varchar(30) NOT NULL default ' ',
  "AAABDACC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDACG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFTRT" varchar(7) NOT NULL default ' ',
  "AAABDAEG" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDACM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFTRT","AAABDAEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDACR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABFTRT" varchar(7) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDACW" varchar(1) NOT NULL default ' ',
  "AAABDACX" int(11) NOT NULL default '0',
  "AAABDACY" varchar(8) NOT NULL default '00000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABFTRT","AAABLTXI"),
  KEY "AAABDADP" ("AAABMRIL","AAABLECD","AAABFTRT","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDADV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTRT" varchar(7) NOT NULL default ' ',
  "AAABDAEG" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABFTRT","AAABDAEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAEH" (
  "AAABDMOI" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKWJY" varchar(2) NOT NULL default ' ',
  "AAABDBGS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAEU" (
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAEZ" (
  "AAABDMOI" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABDAGC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABKWJY","AAABDAGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAFG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDMOI" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABDAGC" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDMOI","AAABKWJY","AAABDAGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAFQ" (
  "AAABDMOI" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABDAGC" varchar(1) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABDAFW" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default '00',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMOI","AAABKWJY","AAABDAGC","AAABDAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABDBQH" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBTU" varchar(4) NOT NULL default ' ',
  "AAABDASH" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAWX","AAABDAWW","AAABDBQH","AAABLTXI"),
  KEY "AAABDAHF" ("AAABMRIL","AAABLECD","AAABDAWX","AAABDAWW","AAABDBQH","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABDBQH" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDAWX","AAABDAWW","AAABDBQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAIB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDMOI" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDMOI","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAIJ" (
  "AAABDAJM" varchar(30) NOT NULL default ' ',
  "AAABDAJJ" varchar(1) NOT NULL default ' ',
  "AAABDAJK" varchar(30) NOT NULL default ' ',
  "AAABDAIN" varchar(1) NOT NULL default '0',
  "AAABDAIO" varchar(1) NOT NULL default ' ',
  "AAABDAIP" varchar(1) NOT NULL default ' ',
  "AAABDAIQ" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(1) NOT NULL default ' ',
  "AAABDAIS" varchar(2) NOT NULL default '00',
  "AAABDAIT" varchar(3) NOT NULL default '000',
  "AAABDAIU" varchar(4) NOT NULL default '0000',
  "AAABDAIV" varchar(1) NOT NULL default '0',
  "AAABIRTT" varchar(8) NOT NULL default ' ',
  "AAABIIUD" varchar(30) NOT NULL default ' ',
  "AAABDAIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDAJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAJA" (
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABDAJM" varchar(40) NOT NULL default ' ',
  "AAABDAJJ" varchar(1) NOT NULL default ' ',
  "AAABDAJK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABDAJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAJH" (
  "AAABDAJM" varchar(30) NOT NULL default ' ',
  "AAABDAJJ" varchar(1) NOT NULL default ' ',
  "AAABDAJK" varchar(30) NOT NULL default ' ',
  "AAABDAJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDAJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAJN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(1) NOT NULL default '0',
  "AAABDAJQ" varchar(4) NOT NULL default ' ',
  "AAABFKOX" varchar(30) NOT NULL default ' ',
  "AAABDAJS" varchar(4) NOT NULL default ' ',
  "AAABDAJT" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDALA" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAMH" varchar(5) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDALA","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDALA" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDALA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDALC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAMH" varchar(5) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDALH" varchar(1) NOT NULL default ' ',
  "AAABDALI" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAMH","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDALZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAMH" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAMH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAQJ" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAPB" varchar(1) NOT NULL default ' ',
  "AAABDAOS" varchar(4) NOT NULL default ' ',
  "AAABDAMQ" varchar(1) NOT NULL default ' ',
  "AAABDAMR" varchar(5) NOT NULL default ' ',
  "AAABMMJT" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(1) NOT NULL default '0',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABDBHQ" varchar(4) NOT NULL default ' ',
  "AAABDBNS" varchar(1) NOT NULL default ' ',
  "AAABDAMX" varchar(1) NOT NULL default ' ',
  "AAABDAMY" varchar(1) NOT NULL default ' ',
  "AAABDAMZ" varchar(1) NOT NULL default ' ',
  "AAABDANA" varchar(1) NOT NULL default ' ',
  "AAABDARZ" varchar(4) NOT NULL default ' ',
  "AAABDANC" varchar(4) NOT NULL default ' ',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABDBHY" varchar(12) NOT NULL default '000000000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDANR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAQJ","AAABLTXI"),
  KEY "AAABDANW" ("AAABMRIL","AAABLECD","AAABDAQJ","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAOS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAOS" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDAOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAOT" (
  "AAABDAPB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDAPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAOW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAPB" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDAPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBPS" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAPH" varchar(1) NOT NULL default ' ',
  "AAABDAPI" varchar(1) NOT NULL default ' ',
  "AAABDAPJ" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBPS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAQJ" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDAQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDARZ" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAQP" varchar(1) NOT NULL default ' ',
  "AAABDAQQ" int(11) NOT NULL default '0',
  "AAABDAQR" int(11) NOT NULL default '0',
  "AAABDAQS" varchar(10) NOT NULL default ' ',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDARZ","AAABLTXI"),
  KEY "AAABDARK" ("AAABMRIL","AAABLECD","AAABDARZ","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDARQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDARZ" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDARZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDASA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBTU" varchar(4) NOT NULL default ' ',
  "AAABDASH" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAWX","AAABDAWW","AAABLTXI"),
  KEY "AAABDASZ" ("AAABMRIL","AAABLECD","AAABDAWX","AAABDAWW","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDATG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDAWX","AAABDAWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDATS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAUL" varchar(2) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNRG" decimal(5,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAUL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAUL" varchar(2) NOT NULL default ' ',
  "AAABDAUM" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNRG" decimal(5,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAUL","AAABDAUM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABKWUH" varchar(1) NOT NULL default ' ',
  "AAABDAUU" varchar(1) NOT NULL default ' ',
  "AAABDAUV" varchar(1) NOT NULL default ' ',
  "AAABDAUW" varchar(3) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCC","AAABLTXI"),
  KEY "AAABDAVN" ("AAABMRIL","AAABLECD","AAABLCCC","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDAWW","AAABDAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDAWW" varchar(2) NOT NULL default ' ',
  "AAABDAWX" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDAWW","AAABDAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAWY" (
  "AAABDAXN" varchar(2) NOT NULL default ' ',
  "AAABDAXA" varchar(16) NOT NULL default ' ',
  "AAABDAXB" varchar(30) NOT NULL default ' ',
  "AAABDAXC" varchar(30) NOT NULL default ' ',
  "AAABDAXD" varchar(1) NOT NULL default ' ',
  "AAABDAXE" varchar(1) NOT NULL default ' ',
  "AAABDAXF" varchar(30) NOT NULL default ' ',
  "AAABDAXG" varchar(30) NOT NULL default ' ',
  "AAABDAXH" varchar(30) NOT NULL default ' ',
  "AAABDAXI" varchar(30) NOT NULL default ' ',
  "AAABDAXJ" varchar(30) NOT NULL default ' ',
  "AAABDAXK" varchar(30) NOT NULL default ' ',
  "AAABDAXL" varchar(30) NOT NULL default ' ',
  "AAABDAXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDAXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDAZP" varchar(32) NOT NULL default ' ',
  "AAABLRSJ" varchar(10) NOT NULL default ' ',
  "AAABDAYH" varchar(32) NOT NULL default ' ',
  "AAABDAYI" varchar(32) NOT NULL default ' ',
  "AAABDAYJ" varchar(32) NOT NULL default ' ',
  "AAABDAYK" varchar(32) NOT NULL default ' ',
  "AAABDAYL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBUB","AAABLTXI"),
  KEY "AAABDAYQ" ("AAABMRIL","AAABLECD","AAABDBUB","AAABDBUY","AAABDBUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDAZP" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDAZA" varchar(30) NOT NULL default ' ',
  "AAABDAZB" varchar(16) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDAZP","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDAZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLJJN","AAABLTXI"),
  KEY "AAABDBAQ" ("AAABMRIL","AAABLECD","AAABLFYC","AAABLJJN","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLFYC" varchar(2) NOT NULL default ' ',
  "AAABLTWR" varchar(5) NOT NULL default ' ',
  "AAABLJJN" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLFYC","AAABLTWR","AAABLJJN","AAABLTXI"),
  KEY "AAABDBBX" ("AAABMRIL","AAABLECD","AAABLFYC","AAABLTWR","AAABLJJN","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABKAOU" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBCK" varchar(3) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKAOU","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKAOU" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABKAOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBEC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBEC" varchar(10) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBEC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBED" (
  "AAABDBEK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBEG" (
  "AAABDBEK" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBEJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBEK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBFS" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBHY" varchar(12) NOT NULL default '000000000000',
  "AAABDBES" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBFS","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBFS" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBGB" varchar(2) NOT NULL default ' ',
  "AAABDBFX" varchar(1) NOT NULL default ' ',
  "AAABDBFY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABDBGI" varchar(40) NOT NULL default ' ',
  "AAABDBGJ" varchar(14) NOT NULL default ' ',
  "AAABDBGK" varchar(1) NOT NULL default ' ',
  "AAABDBGL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBGS" varchar(6) NOT NULL default '000000',
  "AAABDBGT" varchar(1) NOT NULL default ' ',
  "AAABDBGU" varchar(1) NOT NULL default ' ',
  "AAABDBGV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBGY" (
  "AAABDBHP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBHB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBHP" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBHH" (
  "AAABDBHP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBJG" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBHP" varchar(2) NOT NULL default ' ',
  "AAABDBHQ" varchar(4) NOT NULL default ' ',
  "AAABHWQS" varchar(20) NOT NULL default ' ',
  "AAABDBHS" decimal(8,3) NOT NULL default '0.000',
  "AAABDBHT" decimal(15,2) NOT NULL default '0.00',
  "AAABDBHU" decimal(15,2) NOT NULL default '0.00',
  "AAABDBPX" varchar(4) NOT NULL default ' ',
  "AAABDBHW" varchar(1) NOT NULL default ' ',
  "AAABDBHX" varchar(1) NOT NULL default ' ',
  "AAABDBHY" varchar(12) NOT NULL default '000000000000',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBIL" decimal(15,2) NOT NULL default '0.00',
  "AAABDBIM" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBJG","AAABLTXI"),
  KEY "AAABDBIR" ("AAABMRIL","AAABLECD","AAABDBJG","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBJG" varchar(4) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBJH" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABDBJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBJN" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMRHS","AAABHMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBJV" (
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABDBJY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBKB" (
  "AAABDBKG" varchar(30) NOT NULL default ' ',
  "AAABDBKH" varchar(30) NOT NULL default ' ',
  "AAABDBKE" varchar(1) NOT NULL default ' ',
  "AAABDBKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBKG","AAABDBKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDBKU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBKU" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBKV" (
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBLA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBMB","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBMB" varchar(3) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABDBLY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBMB","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBME" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBMO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKWSS" varchar(4) NOT NULL default ' ',
  "AAABGNDC" varchar(4) NOT NULL default '0000',
  "AAABGNCZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKWSS","AAABGNDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDBNK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDBNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBNK" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDBNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBPB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABFTRT" varchar(7) NOT NULL default ' ',
  "AAABDBNS" varchar(1) NOT NULL default ' ',
  "AAABDBNT" varchar(3) NOT NULL default ' ',
  "AAABDBNU" varchar(3) NOT NULL default ' ',
  "AAABDBNV" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBPB","AAABLTXI"),
  KEY "AAABDBOM" ("AAABMRIL","AAABLECD","AAABDBPB","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBPB" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBPX" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  "AAABDBPQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBPX" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(20) NOT NULL default ' ',
  "AAABDBPQ" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDBPS" varchar(4) NOT NULL default ' ',
  "AAABDBPT" varchar(4) NOT NULL default ' ',
  "AAABDBPU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBUA" varchar(5) NOT NULL default ' ',
  "AAABDBRR" varchar(3) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBQE" varchar(4) NOT NULL default ' ',
  "AAABDBQF" varchar(30) NOT NULL default ' ',
  "AAABDYAB" varchar(1) NOT NULL default ' ',
  "AAABDBQH" varchar(4) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBUA","AAABDBRR","AAABLTXI"),
  KEY "AAABDBQZ" ("AAABMRIL","AAABLECD","AAABDBUA","AAABDBRR","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBUA" varchar(5) NOT NULL default ' ',
  "AAABDBRR" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBUA","AAABDBRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBTU" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBRX" varchar(1) NOT NULL default ' ',
  "AAABDBRY" varchar(1) NOT NULL default ' ',
  "AAABDBRZ" varchar(1) NOT NULL default ' ',
  "AAABDBSA" varchar(1) NOT NULL default ' ',
  "AAABDBSB" varchar(1) NOT NULL default ' ',
  "AAABDBSC" varchar(1) NOT NULL default ' ',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBTU","AAABLTXI"),
  KEY "AAABDBST" ("AAABMRIL","AAABLECD","AAABDBTU","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBTK" varchar(30) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBTH" (
  "AAABDBTK" varchar(30) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABDBTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBTU" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMMJT" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABDBUA" varchar(5) NOT NULL default ' ',
  "AAABDBUB" varchar(10) NOT NULL default ' ',
  "AAABDMCO" varchar(1) NOT NULL default '0',
  "AAABDBUD" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUE" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUF" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUG" varchar(14) NOT NULL default '00000000000000',
  "AAABDBUY" varchar(1) NOT NULL default ' ',
  "AAABDBUX" varchar(1) NOT NULL default ' ',
  "AAABDBUJ" varchar(20) NOT NULL default ' ',
  "AAABDBUK" varchar(12) NOT NULL default ' ',
  "AAABDBUL" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMMJT","AAABLTXI"),
  KEY "AAABDBUT" ("AAABMRIL","AAABLECD","AAABMMJT","AAABDBUX","AAABDBUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBUZ" (
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBXK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBXX","AAABDBXY","AAABDBXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBVH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBXX","AAABDBXY","AAABDBXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBVR" (
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXM" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBXK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBXX","AAABDBXM","AAABDBXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBVZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXM" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBXX","AAABDBXM","AAABDBXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBWJ" (
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBXX","AAABDBXY","AAABDBXZ","AAABDBYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBWT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBXX","AAABDBXY","AAABDBXZ","AAABDBYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBXF" (
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXM" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABDBXK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDBXX","AAABDBXM","AAABDBXZ","AAABDBYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBXP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBXX" varchar(1) NOT NULL default ' ',
  "AAABDBXY" varchar(1) NOT NULL default ' ',
  "AAABDBXZ" varchar(1) NOT NULL default ' ',
  "AAABDBYA" varchar(1) NOT NULL default ' ',
  "AAABLFZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDBXX","AAABDBXY","AAABDBXZ","AAABDBYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABDBZB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABDBZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDBZB" varchar(3) NOT NULL default ' ',
  "AAABDBYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABMRHS","AAABDBZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCTO" varchar(2) NOT NULL default ' ',
  "AAABEEBM" varchar(8) NOT NULL default ' ',
  "AAABEEBN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCTO" varchar(2) NOT NULL default ' ',
  "AAABGRFI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDCTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDBZV" varchar(8) NOT NULL default '00000000',
  "AAABMKAI" varchar(4) NOT NULL default '0000',
  "AAABDBZX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDBZV","AAABMKAI","AAABDBZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDBZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCAD" varchar(2) NOT NULL default '00',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCAE" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABDCAI" varchar(32) NOT NULL default ' ',
  "AAABDCAJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCAL" (
  "AAABLUSV" varchar(1) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABGBTN" varchar(6) NOT NULL default '000000',
  "AAABDCAP" varchar(1) NOT NULL default ' ',
  "AAABDCAQ" varchar(2) NOT NULL default ' ',
  "AAABDCAR" varchar(4) NOT NULL default '0000',
  "AAABDCAS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLE" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKNLE" varchar(3) NOT NULL default ' ',
  "AAABECYQ" varchar(3) NOT NULL default ' ',
  "AAABDKKP" varchar(38) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKNLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMGU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABFSUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABIIUH" varchar(4) NOT NULL default '0000',
  "AAABDCBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABIIUG","AAABIIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCCO" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKMIH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCCO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCFQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDCFQ" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDCFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCDD" (
  "AAABMRFO" varchar(20) NOT NULL default ' ',
  "AAABDCDI" varchar(20) NOT NULL default ' ',
  "AAABMLIB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO","AAABDCDI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEST" varchar(8) NOT NULL default ' ',
  "AAABDCDM" varchar(1) NOT NULL default ' ',
  "AAABDCDN" varchar(1) NOT NULL default ' ',
  "AAABDCDO" varchar(1) NOT NULL default ' ',
  "AAABDCDP" varchar(1) NOT NULL default ' ',
  "AAABDCDQ" varchar(1) NOT NULL default ' ',
  "AAABDCDR" varchar(1) NOT NULL default ' ',
  "AAABDCDS" varchar(1) NOT NULL default ' ',
  "AAABDCDT" varchar(1) NOT NULL default ' ',
  "AAABKQOF" varchar(1) NOT NULL default ' ',
  "AAABDCDV" varchar(2) NOT NULL default '00',
  "AAABDCDW" varchar(1) NOT NULL default '0',
  "AAABDCDX" varchar(2) NOT NULL default '00',
  "AAABDCDY" varchar(2) NOT NULL default '00',
  "AAABDCDZ" decimal(1,0) NOT NULL default '0',
  "AAABDCEA" varchar(1) NOT NULL default ' ',
  "AAABDCEB" varchar(1) NOT NULL default ' ',
  "AAABDCEC" varchar(1) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABDCEE" varchar(1) NOT NULL default ' ',
  "AAABDCEF" varchar(1) NOT NULL default ' ',
  "AAABDCEG" varchar(4) NOT NULL default ' ',
  "AAABDCEH" varchar(1) NOT NULL default ' ',
  "AAABDCEI" varchar(3) NOT NULL default '000',
  "AAABDCEJ" varchar(1) NOT NULL default ' ',
  "AAABDCEK" varchar(3) NOT NULL default '000',
  "AAABDCEL" varchar(1) NOT NULL default ' ',
  "AAABDCEM" varchar(1) NOT NULL default ' ',
  "AAABKJMK" varchar(1) NOT NULL default ' ',
  "AAABDCEO" varchar(1) NOT NULL default ' ',
  "AAABIJJA" varchar(1) NOT NULL default ' ',
  "AAABDCEQ" varchar(1) NOT NULL default ' ',
  "AAABDCER" varchar(1) NOT NULL default ' ',
  "AAABDCES" varchar(1) NOT NULL default ' ',
  "AAABDCET" decimal(3,0) NOT NULL default '0',
  "AAABDCEU" varchar(1) NOT NULL default ' ',
  "AAABDCEV" varchar(1) NOT NULL default ' ',
  "AAABDCEW" varchar(1) NOT NULL default ' ',
  "AAABDCEX" varchar(1) NOT NULL default ' ',
  "AAABDCEY" varchar(1) NOT NULL default ' ',
  "AAABJVHF" varchar(1) NOT NULL default ' ',
  "AAABDCFA" varchar(1) NOT NULL default ' ',
  "AAABDCFB" varchar(1) NOT NULL default ' ',
  "AAABDCFC" varchar(1) NOT NULL default ' ',
  "AAABDCFD" varchar(1) NOT NULL default ' ',
  "AAABDCFE" varchar(1) NOT NULL default ' ',
  "AAABDCFF" varchar(1) NOT NULL default ' ',
  "AAABDCFG" varchar(1) NOT NULL default ' ',
  "AAABDCFH" varchar(4) NOT NULL default ' ',
  "AAABDCFI" varchar(1) NOT NULL default ' ',
  "AAABDCFJ" varchar(4) NOT NULL default ' ',
  "AAABIKDN" varchar(14) NOT NULL default ' ',
  "AAABDCFL" varchar(1) NOT NULL default ' ',
  "AAABDCFM" varchar(1) NOT NULL default ' ',
  "AAABDCFN" varchar(1) NOT NULL default ' ',
  "AAABDCFO" varchar(1) NOT NULL default ' ',
  "AAABDCFP" varchar(1) NOT NULL default ' ',
  "AAABDCFQ" varchar(40) NOT NULL default ' ',
  "AAABDCFR" varchar(40) NOT NULL default ' ',
  "AAABDCFS" varchar(1) NOT NULL default ' ',
  "AAABDCFT" varchar(1) NOT NULL default ' ',
  "AAABDCFU" varchar(1) NOT NULL default ' ',
  "AAABDCFV" varchar(1) NOT NULL default ' ',
  "AAABDCFW" varchar(1) NOT NULL default ' ',
  "AAABDCFX" varchar(1) NOT NULL default ' ',
  "AAABDCFY" varchar(1) NOT NULL default ' ',
  "AAABDCFZ" varchar(1) NOT NULL default ' ',
  "AAABDCGA" varchar(1) NOT NULL default ' ',
  "AAABDCGB" varchar(2) NOT NULL default '00',
  "AAABDCGC" varchar(2) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAABDCGE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCGT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCGT" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNCK" varchar(4) NOT NULL default ' ',
  "AAABKYOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABKZYE" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDCHF" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(5) NOT NULL default ' ',
  "AAABGFUB" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABKZYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKNCK" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEST" varchar(8) NOT NULL default ' ',
  "AAABDCIF" tinyint(3) unsigned NOT NULL default '0',
  "AAABDCIG" varchar(1) NOT NULL default ' ',
  "AAABDCIH" varchar(1) NOT NULL default ' ',
  "AAABDCII" varchar(1) NOT NULL default ' ',
  "AAABDCIJ" varchar(1) NOT NULL default ' ',
  "AAABDCIK" varchar(1) NOT NULL default ' ',
  "AAABDCIL" varchar(1) NOT NULL default ' ',
  "AAABDCIM" tinyint(3) unsigned NOT NULL default '0',
  "AAABDCIN" varchar(1) NOT NULL default ' ',
  "AAABDCIO" varchar(1) NOT NULL default ' ',
  "AAABDCIP" varchar(1) NOT NULL default ' ',
  "AAABDCIQ" varchar(1) NOT NULL default ' ',
  "AAABDCIR" varchar(1) NOT NULL default ' ',
  "AAABDCIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCJL" varchar(30) NOT NULL default ' ',
  "AAABFEAO" varchar(30) NOT NULL default ' ',
  "AAABDCIZ" varchar(2) NOT NULL default ' ',
  "AAABDCJA" varchar(30) NOT NULL default ' ',
  "AAABDCJB" varchar(30) NOT NULL default ' ',
  "AAABDCJC" varchar(30) NOT NULL default ' ',
  "AAABDCJD" varchar(30) NOT NULL default ' ',
  "AAABDCJE" varchar(30) NOT NULL default ' ',
  "AAABDCJF" varchar(30) NOT NULL default ' ',
  "AAABDCJG" varchar(30) NOT NULL default ' ',
  "AAABDCJH" varchar(30) NOT NULL default ' ',
  "AAABDCJI" varchar(30) NOT NULL default ' ',
  "AAABDCJJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVFN" varchar(8) NOT NULL default '00000000',
  "AAABDVFH" varchar(40) NOT NULL default ' ',
  "AAABIHDR" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVFN"),
  UNIQUE KEY "AAABDCJU" ("AAABMRIL","AAABDVFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEUWV" varchar(30) NOT NULL default ' ',
  "AAABDVFN" varchar(8) NOT NULL default '00000000',
  "AAABLXIS" varchar(75) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEUWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABDCLY" varchar(1) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLTXG","AAABDCLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABDCLH" varchar(1) NOT NULL default ' ',
  "AAABDCLI" varchar(1) NOT NULL default ' ',
  "AAABDCLJ" varchar(3) NOT NULL default '000',
  "AAABDCLK" varchar(3) NOT NULL default '000',
  "AAABDCLL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABDCLY" varchar(1) NOT NULL default ' ',
  "AAABDCLZ" varchar(30) NOT NULL default ' ',
  "AAABDCLU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLTXG","AAABDCLY","AAABDCLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABDCOY" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABEDDS","AAABDCOY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCOY" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABDCMP" varchar(1) NOT NULL default ' ',
  "AAABECJH" varchar(3) NOT NULL default ' ',
  "AAABDCNN" decimal(8,3) NOT NULL default '0.000',
  "AAABDCNO" decimal(8,3) NOT NULL default '0.000',
  "AAABKQPS" decimal(8,3) NOT NULL default '0.000',
  "AAABHTLA" decimal(8,3) NOT NULL default '0.000',
  "AAABIDCA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDCOY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCNG" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABDCOY" varchar(8) NOT NULL default ' ',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABDCPY" varchar(8) NOT NULL default ' ',
  "AAABDCNN" decimal(8,3) NOT NULL default '0.000',
  "AAABDCNO" decimal(8,3) NOT NULL default '0.000',
  "AAABKQPS" decimal(8,3) NOT NULL default '0.000',
  "AAABHTLA" decimal(8,3) NOT NULL default '0.000',
  "AAABDCNR" varchar(1) NOT NULL default ' ',
  "AAABIZMY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCSJ","AAABDCOY","AAABEDDS","AAABDCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABDCOY" varchar(8) NOT NULL default ' ',
  "AAABDCPY" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABDCOH" varchar(18) NOT NULL default ' ',
  "AAABDCOI" varchar(6) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABDCOK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDDS","AAABDCOY","AAABDCPY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABDCOY" varchar(8) NOT NULL default ' ',
  "AAABJPEE" tinyint(3) unsigned NOT NULL default '0',
  "AAABJHFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABDCOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABDCPY" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABEDDS","AAABDCPY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCPY" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDCPY","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCPY" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABDCQX" varchar(8) NOT NULL default ' ',
  "AAABDCQD" varchar(40) NOT NULL default ' ',
  "AAABDCQE" varchar(4) NOT NULL default ' ',
  "AAABDCQF" varchar(40) NOT NULL default ' ',
  "AAABDCQG" varchar(4) NOT NULL default ' ',
  "AAABDCQH" varchar(1) NOT NULL default ' ',
  "AAABHTWY" varchar(20) NOT NULL default ' ',
  "AAABDCQJ" varchar(1) NOT NULL default ' ',
  "AAABDCRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCSJ","AAABDCQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABDCQX" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCSJ","AAABDCQX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABHDLL" varchar(2) NOT NULL default ' ',
  "AAABLGAP" varchar(40) NOT NULL default ' ',
  "AAABMFSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDDS","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEDDS" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEDDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABDCSK" varchar(5) NOT NULL default ' ',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABDCRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCSJ","AAABDCSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDCSJ" varchar(10) NOT NULL default ' ',
  "AAABDCSK" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDCSJ","AAABDCSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCUN" varchar(13) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABETTM" varchar(2) NOT NULL default ' ',
  "AAABETTC" varchar(8) NOT NULL default ' ',
  "AAABGVPH" double NOT NULL default '0',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABGVPT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDCUN","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCUN" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABDCTO" varchar(2) NOT NULL default ' ',
  "AAABDCTP" varchar(2) NOT NULL default '00',
  "AAABDCUU" varchar(13) NOT NULL default ' ',
  "AAABEEOX" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255),
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABEEPJ" varchar(8) NOT NULL default '00000000',
  "AAABEEPK" varchar(8) NOT NULL default '00000000',
  "AAABGWBS" varchar(2) NOT NULL default '00',
  "AAABGWBU" varchar(2) NOT NULL default '00',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABGSOH" varchar(2) NOT NULL default ' ',
  "AAABEEPO" varchar(18) NOT NULL default ' ',
  "AAABEEQI" varchar(1) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCUN"),
  KEY "AAABDCUO" ("AAABMRIL","AAABLAXK"(255)),
  KEY "AAABDCUR" ("AAABMRIL","AAABJGWD","AAABDCUU","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCUW" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(16) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCVC" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUNJ" varchar(16) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDCXX" varchar(1) NOT NULL default ' ',
  "AAABGIVE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDCXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCZM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDCZM" varchar(2) NOT NULL default ' ',
  "AAABGIVE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDCZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDCWS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCWP" (
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDCWS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDCXG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDCXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDCXG" varchar(5) NOT NULL default ' ',
  "AAABDCXD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDCXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABKMJW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCXO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDCXQ" varchar(1) NOT NULL default ' ',
  "AAABDCXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDCXX" varchar(1) NOT NULL default ' ',
  "AAABDCXY" varchar(1) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBY","AAABDDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDCYI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDBY","AAABDDBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABKPRZ" varchar(1) NOT NULL default ' ',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHBE" varchar(3) NOT NULL default ' ',
  "AAABDHBG" varchar(3) NOT NULL default ' ',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDSTR" varchar(4) NOT NULL default ' ',
  "AAABDCZB" varchar(4) NOT NULL default '0000',
  "AAABDCZC" varchar(1) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBY","AAABDDBO","AAABLUSC","AAABDDBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDCZM" varchar(2) NOT NULL default ' ',
  "AAABDCZN" varchar(32) NOT NULL default ' ',
  "AAABDCZX" varchar(40) NOT NULL default ' ',
  "AAABDCZP" varchar(1) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDCZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDCZX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBR","AAABDDBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  "AAABDDAO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDBR","AAABDDBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBY","AAABDDBO","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDDBO" varchar(8) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDDBQ" varchar(4) NOT NULL default '0000',
  "AAABDDBR" varchar(4) NOT NULL default ' ',
  "AAABDDBK" varchar(30) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBY","AAABDDBO","AAABLUSC","AAABDDBQ","AAABDDBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDBY" varchar(2) NOT NULL default ' ',
  "AAABDKXS" varchar(10) NOT NULL default ' ',
  "AAABDDBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDBY","AAABDKXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDCE" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDDCH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCJ" (
  "AAABDDCL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDDCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCM" (
  "AAABLGEF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCP" (
  "AAABLGEF" varchar(3) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEF","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABDXAX" varchar(12) NOT NULL default ' ',
  "AAABDDCZ" varchar(3) NOT NULL default '000',
  "AAABDDDA" varchar(1) NOT NULL default '0',
  "AAABDDDB" varchar(1) NOT NULL default '0',
  "AAABDDDC" varchar(1) NOT NULL default '0',
  "AAABDDDD" varchar(1) NOT NULL default ' ',
  "AAABDDDE" varchar(1) NOT NULL default ' ',
  "AAABIZKF" varchar(1) NOT NULL default ' ',
  "AAABDDDG" varchar(1) NOT NULL default '0',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKQLH" varchar(3) NOT NULL default ' ',
  "AAABKQJS" varchar(3) NOT NULL default ' ',
  "AAABDDDK" decimal(7,1) NOT NULL default '0.0',
  "AAABDDDL" decimal(5,1) NOT NULL default '0.0',
  "AAABDPHX" varchar(4) NOT NULL default ' ',
  "AAABDPHY" varchar(12) NOT NULL default ' ',
  "AAABDDDO" varchar(10) NOT NULL default ' ',
  "AAABDDDP" varchar(10) NOT NULL default ' ',
  "AAABDDDQ" varchar(10) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABJHFD" varchar(1) NOT NULL default ' ',
  "AAABDXHF" varchar(1) NOT NULL default ' ',
  "AAABDDDU" varchar(1) NOT NULL default ' ',
  "AAABDDDV" varchar(1) NOT NULL default ' ',
  "AAABDDDW" varchar(1) NOT NULL default ' ',
  "AAABDDDX" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABDDDZ" varchar(1) NOT NULL default '0',
  "AAABDDEA" varchar(1) NOT NULL default ' ',
  "AAABDDEB" varchar(1) NOT NULL default ' ',
  "AAABDDEC" varchar(3) NOT NULL default ' ',
  "AAABDDED" varchar(3) NOT NULL default ' ',
  "AAABDDEE" varchar(1) NOT NULL default ' ',
  "AAABDDEF" varchar(1) NOT NULL default ' ',
  "AAABEPUI" varchar(1) NOT NULL default ' ',
  "AAABDDEH" varchar(1) NOT NULL default ' ',
  "AAABDDEI" varchar(20) NOT NULL default ' ',
  "AAABDDEJ" varchar(20) NOT NULL default ' ',
  "AAABDDEK" varchar(20) NOT NULL default ' ',
  "AAABDDEL" varchar(20) NOT NULL default ' ',
  "AAABDDEM" varchar(20) NOT NULL default ' ',
  "AAABDWSK" varchar(1) NOT NULL default ' ',
  "AAABDDEO" varchar(2) NOT NULL default '00',
  "AAABDDEP" int(11) NOT NULL default '0',
  "AAABDDEQ" varchar(2) NOT NULL default '00',
  "AAABDDER" varchar(1) NOT NULL default ' ',
  "AAABDDES" decimal(3,0) NOT NULL default '0',
  "AAABKRBV" varchar(1) NOT NULL default ' ',
  "AAABDDEU" varchar(12) NOT NULL default ' ',
  "AAABDDEV" varchar(12) NOT NULL default ' ',
  "AAABDDEW" varchar(12) NOT NULL default ' ',
  "AAABDDEX" varchar(12) NOT NULL default ' ',
  "AAABDDEY" varchar(3) NOT NULL default ' ',
  "AAABDDEZ" varchar(1) NOT NULL default ' ',
  "AAABDDFA" varchar(1) NOT NULL default ' ',
  "AAABDDFB" varchar(12) NOT NULL default ' ',
  "AAABDDFC" varchar(12) NOT NULL default ' ',
  "AAABDDFD" varchar(1) NOT NULL default ' ',
  "AAABDDFE" varchar(1) NOT NULL default ' ',
  "AAABDDFF" varchar(12) NOT NULL default ' ',
  "AAABDDFG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDFJ" (
  "AAABDDGJ" varchar(30) NOT NULL default ' ',
  "AAABDDFL" varchar(30) NOT NULL default ' ',
  "AAABDDFM" varchar(30) NOT NULL default ' ',
  "AAABDDFN" varchar(30) NOT NULL default ' ',
  "AAABDDFO" varchar(30) NOT NULL default ' ',
  "AAABDDFP" varchar(30) NOT NULL default ' ',
  "AAABDDFQ" varchar(30) NOT NULL default ' ',
  "AAABDDFR" varchar(30) NOT NULL default ' ',
  "AAABDDFS" varchar(30) NOT NULL default ' ',
  "AAABHHLQ" varchar(30) NOT NULL default ' ',
  "AAABDPBX" varchar(30) NOT NULL default ' ',
  "AAABDDFV" varchar(30) NOT NULL default ' ',
  "AAABDDFW" varchar(30) NOT NULL default ' ',
  "AAABDDFX" varchar(30) NOT NULL default ' ',
  "AAABDDFY" varchar(1) NOT NULL default ' ',
  "AAABDDFZ" varchar(1) NOT NULL default ' ',
  "AAABDDGA" varchar(1) NOT NULL default ' ',
  "AAABDDGB" varchar(1) NOT NULL default ' ',
  "AAABDDGC" varchar(1) NOT NULL default ' ',
  "AAABDDGD" varchar(1) NOT NULL default ' ',
  "AAABDDGE" varchar(1) NOT NULL default ' ',
  "AAABDDGF" varchar(1) NOT NULL default ' ',
  "AAABDDGG" varchar(1) NOT NULL default ' ',
  "AAABDDGH" varchar(1) NOT NULL default ' ',
  "AAABDDGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDDGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUVS" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABDDGW" varchar(40) NOT NULL default ' ',
  "AAABDDGX" varchar(14) NOT NULL default ' ',
  "AAABDDGY" varchar(40) NOT NULL default ' ',
  "AAABDDGZ" varchar(14) NOT NULL default ' ',
  "AAABDDHA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABDDLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABDORR" varchar(1) NOT NULL default ' ',
  "AAABILPS" varchar(12) NOT NULL default ' ',
  "AAABIMEG" varchar(12) NOT NULL default ' ',
  "AAABGBSI" varchar(8) NOT NULL default '00000000',
  "AAABDDHU" varchar(8) NOT NULL default '00000000',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABDDIE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDLC","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABDDIN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXXG","AAABMABE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDDJD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABDDJN" varchar(5) NOT NULL default '00000',
  "AAABDDJO" varchar(5) NOT NULL default '00000',
  "AAABDDJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ","AAABLECD","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDMN" varchar(3) NOT NULL default ' ',
  "AAABDDLW" varchar(4) NOT NULL default '0000',
  "AAABDDKF" varchar(2) NOT NULL default '00',
  "AAABDDKA" varchar(50) NOT NULL default ' ',
  "AAABDDKB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDMN","AAABDDLW","AAABDDKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDMN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDMN" varchar(3) NOT NULL default ' ',
  "AAABDDKP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDLC" varchar(1) NOT NULL default ' ',
  "AAABINFJ" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABDDLG" varchar(55) NOT NULL default ' ',
  "AAABDDLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDLC","AAABINFJ","AAABLECD","AAABLTXI","AAABDDLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABDDLL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDMN" varchar(3) NOT NULL default ' ',
  "AAABDDLW" varchar(4) NOT NULL default '0000',
  "AAABDDLT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDMN","AAABDDLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(3) NOT NULL default '000',
  "AAABDDMG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDND" varchar(10) NOT NULL default ' ',
  "AAABDDMN" varchar(3) NOT NULL default ' ',
  "AAABDDMO" varchar(1) NOT NULL default ' ',
  "AAABDDMP" varchar(1) NOT NULL default ' ',
  "AAABDDMQ" varchar(1) NOT NULL default ' ',
  "AAABDDMR" varchar(1) NOT NULL default ' ',
  "AAABDDMS" varchar(1) NOT NULL default ' ',
  "AAABDDMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDND" varchar(10) NOT NULL default ' ',
  "AAABDDNA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHWXD" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHWXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDNK" (
  "AAABIMHJ" varchar(20) NOT NULL default ' ',
  "AAABIMHK" varchar(40) NOT NULL default ' ',
  "AAABIMHF" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ","AAABIMHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDNQ" (
  "AAABIMHJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDNT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMHJ" varchar(20) NOT NULL default ' ',
  "AAABDDNW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDOR" varchar(2) NOT NULL default ' ',
  "AAABDDOJ" varchar(5) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABDDOE" varchar(1) NOT NULL default ' ',
  "AAABDDOF" varchar(1) NOT NULL default ' ',
  "AAABDWNR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDOR","AAABDDOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDOR" varchar(2) NOT NULL default ' ',
  "AAABDDOS" varchar(5) NOT NULL default ' ',
  "AAABLTIX" varchar(4) NOT NULL default ' ',
  "AAABDDOP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDOR","AAABDDOS","AAABLTIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDOU" (
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  "AAABDDPX" varchar(1) NOT NULL default '0',
  "AAABDDOX" varchar(1) NOT NULL default ' ',
  "AAABDDOY" varchar(1) NOT NULL default ' ',
  "AAABKQWU" varchar(1) NOT NULL default ' ',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABDDPB" varchar(1) NOT NULL default ' ',
  "AAABDDPC" varchar(1) NOT NULL default ' ',
  "AAABDDPD" varchar(10) NOT NULL default ' ',
  "AAABDDPE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDDVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDPG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  "AAABDDPJ" varchar(30) NOT NULL default ' ',
  "AAABDDPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDDVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDPN" (
  "AAABDDPX" varchar(1) NOT NULL default '0',
  "AAABDDPP" varchar(1) NOT NULL default ' ',
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABDDPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDPS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDPX" varchar(1) NOT NULL default '0',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDDPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDPY" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDQC" (
  "AAABDDQG" varchar(2) NOT NULL default ' ',
  "AAABDDQH" varchar(1) NOT NULL default ' ',
  "AAABDDQF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABDDQG","AAABDDQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEFI" varchar(2) NOT NULL default ' ',
  "AAABDDQL" varchar(1) NOT NULL default ' ',
  "AAABDDQM" varchar(12) NOT NULL default ' ',
  "AAABDDQN" varchar(1) NOT NULL default ' ',
  "AAABDDQO" decimal(3,0) NOT NULL default '0',
  "AAABDDQP" decimal(3,0) NOT NULL default '0',
  "AAABDDQQ" varchar(1) NOT NULL default ' ',
  "AAABDDQR" varchar(1) NOT NULL default ' ',
  "AAABDDQS" varchar(1) NOT NULL default ' ',
  "AAABDDQT" varchar(1) NOT NULL default ' ',
  "AAABDDQU" varchar(1) NOT NULL default ' ',
  "AAABDDQV" varchar(1) NOT NULL default ' ',
  "AAABDDQW" varchar(1) NOT NULL default ' ',
  "AAABDDQX" varchar(1) NOT NULL default ' ',
  "AAABDDQY" varchar(1) NOT NULL default ' ',
  "AAABLRBV" varchar(1) NOT NULL default ' ',
  "AAABDDRA" varchar(1) NOT NULL default ' ',
  "AAABDDRB" varchar(1) NOT NULL default ' ',
  "AAABDDRC" varchar(1) NOT NULL default ' ',
  "AAABDDRD" varchar(1) NOT NULL default ' ',
  "AAABDDRE" varchar(1) NOT NULL default ' ',
  "AAABDDRF" varchar(1) NOT NULL default ' ',
  "AAABDDRG" varchar(1) NOT NULL default ' ',
  "AAABDDRH" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABDDRJ" varchar(1) NOT NULL default ' ',
  "AAABDDRK" varchar(1) NOT NULL default ' ',
  "AAABDDRL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  "AAABDDRV" varchar(3) NOT NULL default '000',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDVQ","AAABDDRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDRW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDDWG" varchar(4) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABDDWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDSU" varchar(2) NOT NULL default '00',
  "AAABDDSF" varchar(1) NOT NULL default ' ',
  "AAABDPJQ" varchar(1) NOT NULL default ' ',
  "AAABDPJR" varchar(1) NOT NULL default ' ',
  "AAABDPJS" varchar(1) NOT NULL default ' ',
  "AAABDPJT" varchar(1) NOT NULL default ' ',
  "AAABDDSK" varchar(1) NOT NULL default ' ',
  "AAABDDSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDSU" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDSS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDSU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEDU" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABDDTC" varchar(1) NOT NULL default ' ',
  "AAABDDTD" varchar(1) NOT NULL default ' ',
  "AAABDDXF" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEDU" varchar(3) NOT NULL default '000',
  "AAABDDTL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDEDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDUF" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABDDXZ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDUF" varchar(3) NOT NULL default '000',
  "AAABDDUC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABDDUJ" varchar(2) NOT NULL default '00',
  "AAABDDUK" varchar(4) NOT NULL default ' ',
  "AAABDDUL" varchar(40) NOT NULL default ' ',
  "AAABDDVY" varchar(8) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABMAYK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDVY" varchar(8) NOT NULL default ' ',
  "AAABDDVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDVY" varchar(8) NOT NULL default ' ',
  "AAABDDVQ" varchar(2) NOT NULL default '00',
  "AAABLWSY" varchar(1) NOT NULL default ' ',
  "AAABDDVJ" varchar(1) NOT NULL default ' ',
  "AAABDDVK" varchar(1) NOT NULL default ' ',
  "AAABDDVL" varchar(1) NOT NULL default ' ',
  "AAABDDVM" varchar(1) NOT NULL default ' ',
  "AAABDDVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDVY","AAABDDVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDVY" varchar(8) NOT NULL default ' ',
  "AAABDDVV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDVZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDWG" varchar(4) NOT NULL default ' ',
  "AAABGMNJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABDDWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDXF" varchar(8) NOT NULL default ' ',
  "AAABDDWK" varchar(8) NOT NULL default ' ',
  "AAABDDWL" varchar(8) NOT NULL default ' ',
  "AAABDDWM" varchar(8) NOT NULL default ' ',
  "AAABDDWN" varchar(8) NOT NULL default ' ',
  "AAABDDWO" varchar(8) NOT NULL default ' ',
  "AAABDDWP" varchar(8) NOT NULL default ' ',
  "AAABDDWQ" varchar(8) NOT NULL default ' ',
  "AAABDDWR" varchar(8) NOT NULL default ' ',
  "AAABDDWS" varchar(8) NOT NULL default ' ',
  "AAABDDWT" varchar(8) NOT NULL default ' ',
  "AAABDDWU" varchar(8) NOT NULL default ' ',
  "AAABDDWV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDXF" varchar(8) NOT NULL default ' ',
  "AAABDDXC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDDXZ" varchar(8) NOT NULL default ' ',
  "AAABDDXJ" varchar(8) NOT NULL default ' ',
  "AAABDDXK" varchar(8) NOT NULL default ' ',
  "AAABDDXL" varchar(8) NOT NULL default ' ',
  "AAABDDXM" varchar(8) NOT NULL default ' ',
  "AAABDDXN" varchar(8) NOT NULL default ' ',
  "AAABDDXO" varchar(8) NOT NULL default ' ',
  "AAABDDXP" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDDXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDXZ" varchar(8) NOT NULL default ' ',
  "AAABDDXW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDDXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABDDYD" varchar(110) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAYK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABDDZD" varchar(10) NOT NULL default ' ',
  "AAABDDYS" varchar(10) NOT NULL default ' ',
  "AAABDDYT" varchar(30) NOT NULL default ' ',
  "AAABDDYU" varchar(32) NOT NULL default ' ',
  "AAABDDYV" varchar(30) NOT NULL default ' ',
  "AAABDDYW" varchar(32) NOT NULL default ' ',
  "AAABLQPQ" varchar(15) NOT NULL default ' ',
  "AAABDDYY" varchar(30) NOT NULL default ' ',
  "AAABDDYZ" varchar(30) NOT NULL default ' ',
  "AAABDDZA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABDDZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDZE" (
  "AAABDEAL" varchar(25) NOT NULL default ' ',
  "AAABDDZG" varchar(25) NOT NULL default ' ',
  "AAABDDZH" varchar(1) NOT NULL default ' ',
  "AAABDDZI" varchar(30) NOT NULL default ' ',
  "AAABDDZJ" varchar(30) NOT NULL default ' ',
  "AAABDDZK" varchar(1) NOT NULL default ' ',
  "AAABMNMP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABDEAL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDZN" (
  "AAABDEAL" varchar(25) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDDZQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEAL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDDZT" (
  "AAABDEEE" varchar(10) NOT NULL default ' ',
  "AAABDEAL" varchar(25) NOT NULL default ' ',
  "AAABDEAM" varchar(3) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEP" varchar(4) NOT NULL default '0000',
  "AAABHVRR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEEE","AAABDEAL","AAABDEAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEAE" (
  "AAABDEEE" varchar(10) NOT NULL default ' ',
  "AAABDEAL" varchar(25) NOT NULL default ' ',
  "AAABDEAM" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEAJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEEE","AAABDEAL","AAABDEAM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEAO" (
  "AAABDEEE" varchar(10) NOT NULL default ' ',
  "AAABDEAQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABDEEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEAS" (
  "AAABDEEE" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEEE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEBA" varchar(2) NOT NULL default ' ',
  "AAABDEBB" varchar(1) NOT NULL default ' ',
  "AAABDEBC" varchar(2) NOT NULL default ' ',
  "AAABLOXS" varchar(4) NOT NULL default ' ',
  "AAABDEBE" varchar(16) NOT NULL default ' ',
  "AAABDEBF" varchar(1) NOT NULL default ' ',
  "AAABMCOT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXSJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZU" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLTHQ" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABDEBU" varchar(5) NOT NULL default ' ',
  "AAABDEBV" decimal(13,2) NOT NULL default '0.00',
  "AAABEPCK" varchar(1) NOT NULL default ' ',
  "AAABDEBX" varchar(1) NOT NULL default ' ',
  "AAABDEBY" varchar(1) NOT NULL default ' ',
  "AAABDEBZ" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(12) NOT NULL default ' ',
  "AAABDECB" varchar(8) NOT NULL default '00000000',
  "AAABDECC" varchar(6) NOT NULL default '000000',
  "AAABKZAY" varchar(12) NOT NULL default ' ',
  "AAABDECE" varchar(8) NOT NULL default '00000000',
  "AAABLMEU" varchar(6) NOT NULL default '000000',
  "AAABELGZ" varchar(6) NOT NULL default '000000',
  "AAABKQPO" varchar(8) NOT NULL default '00000000',
  "AAABKQPP" varchar(6) NOT NULL default '000000',
  "AAABDECJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDECM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCZU" varchar(10) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDECV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXSJ" varchar(10) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQPQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQPQ" varchar(15) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLQPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABDEDT" varchar(1) NOT NULL default ' ',
  "AAABDEDU" varchar(3) NOT NULL default '000',
  "AAABDEDV" varchar(8) NOT NULL default ' ',
  "AAABDEDW" varchar(12) NOT NULL default ' ',
  "AAABDEDX" varchar(1) NOT NULL default ' ',
  "AAABDEDY" varchar(1) NOT NULL default ' ',
  "AAABDEDZ" varchar(1) NOT NULL default ' ',
  "AAABDEEA" varchar(1) NOT NULL default ' ',
  "AAABDEEB" varchar(1) NOT NULL default ' ',
  "AAABDEEC" varchar(1) NOT NULL default ' ',
  "AAABDEED" varchar(1) NOT NULL default ' ',
  "AAABDEEE" varchar(10) NOT NULL default ' ',
  "AAABDEEF" varchar(1) NOT NULL default ' ',
  "AAABDEEG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEEY" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEEY","AAABHZCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEEY" varchar(1) NOT NULL default ' ',
  "AAABHZCZ" varchar(4) NOT NULL default ' ',
  "AAABLPUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDEEY","AAABHZCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEFA" (
  "AAABDEFI" varchar(2) NOT NULL default ' ',
  "AAABDEFC" varchar(1) NOT NULL default ' ',
  "AAABDEFD" varchar(1) NOT NULL default ' ',
  "AAABDEFE" varchar(1) NOT NULL default ' ',
  "AAABDEFF" varchar(1) NOT NULL default ' ',
  "AAABDEFG" varchar(1) NOT NULL default ' ',
  "AAABDEFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEFJ" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABDEFP" varchar(8) NOT NULL default '00000000',
  "AAABDEFQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEFS" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABDEGB" varchar(20) NOT NULL default ' ',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABILHV" varchar(1) NOT NULL default ' ',
  "AAABDEFZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABDEGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEGC" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABIJYD" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLJIM" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEGK" (
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABDEGN" varchar(1) NOT NULL default ' ',
  "AAABDEGO" varchar(1) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  "AAABDEGQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEGT" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABDEGW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEGZ" (
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJDRC" varchar(26) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABDEHU" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABDEHX" varchar(1) NOT NULL default ' ',
  "AAABDEHY" varchar(20) NOT NULL default ' ',
  "AAABLQHG" varchar(3) NOT NULL default ' ',
  "AAABDEIA" varchar(26) NOT NULL default ' ',
  "AAABLRQQ" varchar(14) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHIIY" varchar(1) NOT NULL default ' ',
  "AAABDEIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABJDRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEHQ" (
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJDRC" varchar(26) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABDEHU" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABDEHX" varchar(1) NOT NULL default ' ',
  "AAABDEHY" varchar(20) NOT NULL default ' ',
  "AAABLQHG" varchar(3) NOT NULL default ' ',
  "AAABDEIA" varchar(26) NOT NULL default ' ',
  "AAABLRQQ" varchar(14) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABHIIY" varchar(1) NOT NULL default ' ',
  "AAABDEIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABJDRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEIH" (
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  "AAABJDRC" varchar(26) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABJDRC","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEIQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEIS" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABDEQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABINFD","AAABLUXQ"),
  KEY "AAABDEJA" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEJC" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABDEJE" varchar(1) NOT NULL default ' ',
  "AAABDEJF" varchar(1) NOT NULL default ' ',
  "AAABDEOB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEJI" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABHYAF" varchar(10) NOT NULL default ' ',
  "AAABDEJQ" varchar(3) NOT NULL default ' ',
  "AAABHYAD" varchar(10) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABHYAF","AAABDEJQ","AAABHYAD"),
  KEY "AAABDEJS" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEKF" varchar(10) NOT NULL default ' ',
  "AAABDEKG" varchar(2) NOT NULL default ' ',
  "AAABDEKH" varchar(2) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABIJDD" varchar(3) NOT NULL default ' ',
  "AAABLQTX" varchar(3) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABFNXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEKF","AAABDEKG","AAABDEKH","AAABLMHB","AAABIJDD","AAABLQTX","AAABLQSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMJR" varchar(1) NOT NULL default ' ',
  "AAABDEKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMJR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMJR" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEKW" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMJR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDELA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMGU" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDELE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMGU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDELI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINNI" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(2) NOT NULL default '00',
  "AAABDELM" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABDELO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINNI","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDELS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDELY" varchar(3) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABDELW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDELY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDELZ" (
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABINFD","AAABLUXQ","AAABIANG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEMG" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABDEQF" varchar(4) NOT NULL default ' ',
  "AAABDZHO" varchar(10) NOT NULL default ' ',
  "AAABHYAG" varchar(10) NOT NULL default ' ',
  "AAABDEML" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABDEQF","AAABDZHO"),
  KEY "AAABDEMP" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDENC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDENC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDENC" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKMIH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDENC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDENE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABDENV" varchar(2) NOT NULL default ' ',
  "AAABDENI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABININ","AAABDENV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDENM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABININ" varchar(4) NOT NULL default ' ',
  "AAABDENV" varchar(2) NOT NULL default ' ',
  "AAABDENR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABININ","AAABDENV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDENW" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABDEQF" varchar(4) NOT NULL default ' ',
  "AAABLFZT" varchar(1) NOT NULL default ' ',
  "AAABDEOB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABINFD"),
  KEY "AAABDEOE" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEOG" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABINFD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABINFD","AAABMRHS"),
  KEY "AAABDEOO" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEOQ" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABDEPP" varchar(10) NOT NULL default ' ',
  "AAABGHGV" varchar(10) NOT NULL default ' ',
  "AAABEBZN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABDEPP","AAABGHGV","AAABEBZN"),
  KEY "AAABDEOZ" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEPB" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABDEPP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABDEPP"),
  KEY "AAABDEPG" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEPI" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDEPP" varchar(10) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMRHS","AAABDEPP"),
  KEY "AAABDEPQ" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEPS" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABDEQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABDEQF"),
  KEY "AAABDEPX" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEPZ" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABDEQF" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABDEQF","AAABMRHS"),
  KEY "AAABDEQH" ("AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEQJ" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABDEQM" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABJQWM" varchar(14) NOT NULL default '00000000000000',
  "AAABDEQP" varchar(14) NOT NULL default '00000000000000',
  "AAABDEQQ" varchar(14) NOT NULL default '00000000000000',
  "AAABDEQR" varchar(10) NOT NULL default ' ',
  "AAABDEQS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLTXI"),
  KEY "AAABDEQU" ("AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEQW" (
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQNK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTXI","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDERC" (
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMLPV" varchar(32) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABDERG" varchar(32) NOT NULL default ' ',
  "AAABDERH" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDERJ" (
  "AAABDERS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDERS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDERM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDERS" varchar(1) NOT NULL default ' ',
  "AAABDERP" varchar(1) NOT NULL default ' ',
  "AAABDERQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDERS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDERT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABDERW" varchar(1) NOT NULL default ' ',
  "AAABDERX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDESA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDESI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNLO" varchar(4) NOT NULL default ' ',
  "AAABKKDS" varchar(1) NOT NULL default ' ',
  "AAABDESM" varchar(1) NOT NULL default ' ',
  "AAABDESN" varchar(1) NOT NULL default ' ',
  "AAABDESO" varchar(1) NOT NULL default ' ',
  "AAABDESP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDESS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKNLO" varchar(4) NOT NULL default ' ',
  "AAABDESW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKNLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDETA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDETM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDETM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDETF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDETM" varchar(4) NOT NULL default ' ',
  "AAABHUBL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDETM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDETN" (
  "AAABDEVP" varchar(10) NOT NULL default ' ',
  "AAABDETP" varchar(3) NOT NULL default ' ',
  "AAABDETQ" varchar(1) NOT NULL default ' ',
  "AAABDETR" varchar(1) NOT NULL default ' ',
  "AAABDETS" varchar(1) NOT NULL default ' ',
  "AAABDETT" varchar(1) NOT NULL default ' ',
  "AAABDETU" varchar(1) NOT NULL default ' ',
  "AAABDETV" varchar(1) NOT NULL default ' ',
  "AAABDETW" varchar(1) NOT NULL default ' ',
  "AAABDETX" varchar(1) NOT NULL default ' ',
  "AAABDETY" varchar(1) NOT NULL default ' ',
  "AAABDETZ" varchar(1) NOT NULL default ' ',
  "AAABDEUA" varchar(1) NOT NULL default ' ',
  "AAABDEUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDEVP" varchar(10) NOT NULL default ' ',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDFDW" varchar(4) NOT NULL default '0000',
  "AAABLDQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDEVP","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEUM" (
  "AAABDEVP" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDEUP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDEVP","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEUS" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABDEUU" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABDEUW" varchar(26) NOT NULL default ' ',
  "AAABDEUX" varchar(30) NOT NULL default ' ',
  "AAABDEUY" varchar(30) NOT NULL default ' ',
  "AAABDEUZ" varchar(1) NOT NULL default ' ',
  "AAABDEVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEVC" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABDEVG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABDEVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEVH" (
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGCM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDEVP" varchar(10) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDEVR" varchar(10) NOT NULL default '0000000000',
  "AAABDEVS" varchar(10) NOT NULL default '0000000000',
  "AAABDEVT" varchar(1) NOT NULL default ' ',
  "AAABDEVU" varchar(1) NOT NULL default ' ',
  "AAABDEVV" varchar(1) NOT NULL default ' ',
  "AAABDGAQ" varchar(10) NOT NULL default ' ',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABDEVY" varchar(18) NOT NULL default ' ',
  "AAABDEVZ" varchar(18) NOT NULL default ' ',
  "AAABDEWA" varchar(1) NOT NULL default ' ',
  "AAABDEWB" varchar(30) NOT NULL default ' ',
  "AAABDEWC" varchar(1) NOT NULL default ' ',
  "AAABDEWD" varchar(1) NOT NULL default ' ',
  "AAABDEWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABFSDS" varchar(2) NOT NULL default '00',
  "AAABDEWL" varchar(2) NOT NULL default '00',
  "AAABDEWM" varchar(2) NOT NULL default '00',
  "AAABJHCT" varchar(2) NOT NULL default '00',
  "AAABDEWO" varchar(2) NOT NULL default '00',
  "AAABDEWP" varchar(2) NOT NULL default '00',
  "AAABDEWQ" varchar(2) NOT NULL default '00',
  "AAABIISW" varchar(2) NOT NULL default '00',
  "AAABDEWS" varchar(2) NOT NULL default '00',
  "AAABLUZL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDFDW" varchar(4) NOT NULL default '0000',
  "AAABLDQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDEXL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN","AAABDEXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABDEYE" varchar(4) NOT NULL default '0000',
  "AAABLUNJ" varchar(30) NOT NULL default ' ',
  "AAABLLIO" varchar(3) NOT NULL default ' ',
  "AAABFNHZ" int(11) NOT NULL default '0',
  "AAABDEXU" varchar(1) NOT NULL default ' ',
  "AAABDEXV" varchar(1) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLNE" varchar(1) NOT NULL default ' ',
  "AAABDEXY" varchar(1) NOT NULL default ' ',
  "AAABGMYX" varchar(1) NOT NULL default ' ',
  "AAABGUFU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN","AAABDHIL","AAABDEYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDGYZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABDEYR" varchar(1) NOT NULL default ' ',
  "AAABDEYS" varchar(1) NOT NULL default ' ',
  "AAABDEYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN","AAABDHIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEYX" (
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABDHLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEZA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABDEZD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDHLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABDEZK" int(11) NOT NULL default '0',
  "AAABDEZL" varchar(1) NOT NULL default ' ',
  "AAABDEZM" varchar(1) NOT NULL default ' ',
  "AAABDFLK" varchar(30) NOT NULL default ' ',
  "AAABDEZO" varchar(1) NOT NULL default ' ',
  "AAABLUZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLV","AAABKCHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDEZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLV","AAABKCHW","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABDFAH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHLV","AAABKCHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABDFAV" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABERXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDFBY" varchar(4) NOT NULL default '0000',
  "AAABDFBZ" varchar(4) NOT NULL default '0000',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  "AAABDFBI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABERXQ","AAABLPJD","AAABDFBY","AAABDFBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABDFBY" varchar(4) NOT NULL default '0000',
  "AAABDFBZ" varchar(4) NOT NULL default '0000',
  "AAABDFCA" varchar(10) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXQ","AAABDFBY","AAABDFBZ","AAABDFCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABDQUN" varchar(1) NOT NULL default ' ',
  "AAABDFCF" varchar(1) NOT NULL default ' ',
  "AAABDFLK" varchar(30) NOT NULL default ' ',
  "AAABDFJS" varchar(10) NOT NULL default ' ',
  "AAABDFJT" varchar(2) NOT NULL default ' ',
  "AAABDFJU" varchar(2) NOT NULL default ' ',
  "AAABDFFA" varchar(1) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDFCN" varchar(1) NOT NULL default ' ',
  "AAABDFCO" varchar(1) NOT NULL default ' ',
  "AAABDFCP" varchar(1) NOT NULL default ' ',
  "AAABDFCQ" varchar(1) NOT NULL default ' ',
  "AAABDFCR" varchar(30) NOT NULL default ' ',
  "AAABDFCS" varchar(30) NOT NULL default ' ',
  "AAABDFCT" varchar(1) NOT NULL default ' ',
  "AAABDFCU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGFS","AAABDHIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFDQ" varchar(10) NOT NULL default ' ',
  "AAABDFDR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFDQ","AAABDFDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDFDW" varchar(4) NOT NULL default '0000',
  "AAABLDQW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHIL","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABDFEF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFEV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFEV" varchar(10) NOT NULL default ' ',
  "AAABDFES" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFEV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDFFA" varchar(1) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABDFFC" varchar(45) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGHG","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABDFFR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABDFFY" varchar(4) NOT NULL default '0000',
  "AAABDFFZ" varchar(1) NOT NULL default ' ',
  "AAABDFGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAS","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABDFGO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJFM" varchar(10) NOT NULL default ' ',
  "AAABDFGV" varchar(132) NOT NULL default ' ',
  "AAABDFGW" varchar(132) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDFHW" varchar(10) NOT NULL default ' ',
  "AAABDFGZ" varchar(4) NOT NULL default '0000',
  "AAABDFHA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIJFM" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIJFM","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFHW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFHW" varchar(10) NOT NULL default ' ',
  "AAABDFHT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDFIA" varchar(10) NOT NULL default ' ',
  "AAABDFIB" varchar(1) NOT NULL default ' ',
  "AAABDFIC" varchar(1) NOT NULL default ' ',
  "AAABDFID" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDGEQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAABGRSI" varchar(6) NOT NULL default '000000',
  "AAABLTQA" varchar(2) NOT NULL default '00',
  "AAABDFIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGFS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHOV" varchar(10) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDFOI" varchar(1) NOT NULL default ' ',
  "AAABDFJH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  "AAABDFJN" varchar(1) NOT NULL default ' ',
  "AAABDFJO" varchar(40) NOT NULL default ' ',
  "AAABDFJP" varchar(8) NOT NULL default '00000000',
  "AAABDFJQ" varchar(10) NOT NULL default ' ',
  "AAABDFLK" varchar(30) NOT NULL default ' ',
  "AAABDFJS" varchar(10) NOT NULL default ' ',
  "AAABDFJT" varchar(2) NOT NULL default ' ',
  "AAABDFJU" varchar(2) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  "AAABDFKC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  "AAABDFKY" varchar(10) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDFKL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFKX","AAABDFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  "AAABDFKY" varchar(10) NOT NULL default ' ',
  "AAABDFKU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFKX","AAABDFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFLE" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDFLE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDFLR" varchar(10) NOT NULL default '0000000000',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDFLK" varchar(30) NOT NULL default ' ',
  "AAABDFLL" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABDFLN" varchar(1) NOT NULL default ' ',
  "AAABDFLO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGN","AAABDFLR","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFMJ" varchar(5) NOT NULL default ' ',
  "AAABDFLW" varchar(1) NOT NULL default ' ',
  "AAABDFLX" varchar(1) NOT NULL default ' ',
  "AAABDFLY" varchar(1) NOT NULL default ' ',
  "AAABDFLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFMJ" varchar(5) NOT NULL default ' ',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(10) NOT NULL default ' ',
  "AAABDFMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUNI" varchar(10) NOT NULL default ' ',
  "AAABDFMU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFNM" varchar(10) NOT NULL default ' ',
  "AAABDFNE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFNM","AAABDFNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFNM" varchar(10) NOT NULL default ' ',
  "AAABDFNJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABDFNQ" varchar(1) NOT NULL default ' ',
  "AAABDFOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDFOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAT","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDGKN" varchar(20) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDFOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAT","AAABLPJD","AAABDGKN","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAT","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABDFOZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFQB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFQB" varchar(2) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDGKN" varchar(20) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDFQB","AAABLPJD","AAABDGKN","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFQB" varchar(2) NOT NULL default ' ',
  "AAABDFPY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFQB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  "AAABDFQF" varchar(40) NOT NULL default ' ',
  "AAABDFQG" varchar(8) NOT NULL default '00000000',
  "AAABDFQH" varchar(10) NOT NULL default ' ',
  "AAABDFQI" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDFQL" varchar(132) NOT NULL default ' ',
  "AAABDFQM" varchar(132) NOT NULL default ' ',
  "AAABDFQN" varchar(132) NOT NULL default ' ',
  "AAABDFQO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXN","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  "AAABDFRC" varchar(80) NOT NULL default ' ',
  "AAABDFRD" varchar(132) NOT NULL default ' ',
  "AAABDFRE" varchar(132) NOT NULL default ' ',
  "AAABDFRF" varchar(132) NOT NULL default ' ',
  "AAABDFRG" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABERXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABDFRN" varchar(1) NOT NULL default '0',
  "AAABDFRO" varchar(1) NOT NULL default '0',
  "AAABDFRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABDFRW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFSX" varchar(10) NOT NULL default ' ',
  "AAABDFSD" varchar(30) NOT NULL default ' ',
  "AAABDFSE" varchar(30) NOT NULL default ' ',
  "AAABDFSF" varchar(30) NOT NULL default ' ',
  "AAABDFSG" varchar(1) NOT NULL default ' ',
  "AAABDFSH" varchar(1) NOT NULL default ' ',
  "AAABDFSI" varchar(1) NOT NULL default ' ',
  "AAABDFSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFSX" varchar(10) NOT NULL default ' ',
  "AAABDFSQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDFSX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDFTE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABDFTS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXX","AAABMAMP","AAABMQNB","AAABDFTS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGAQ" varchar(10) NOT NULL default ' ',
  "AAABDFUC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGAQ" varchar(10) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGAQ","AAABDHOZ","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGNH" varchar(10) NOT NULL default ' ',
  "AAABDFUT" varchar(30) NOT NULL default ' ',
  "AAABDFUU" varchar(30) NOT NULL default ' ',
  "AAABDGJU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGNH" varchar(10) NOT NULL default ' ',
  "AAABDFVC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFWL" varchar(2) NOT NULL default ' ',
  "AAABDFWM" varchar(10) NOT NULL default ' ',
  "AAABDGNH" varchar(10) NOT NULL default ' ',
  "AAABDFVL" varchar(30) NOT NULL default ' ',
  "AAABDFVM" varchar(30) NOT NULL default ' ',
  "AAABDFVN" varchar(30) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDFVP" varchar(10) NOT NULL default ' ',
  "AAABDFVQ" varchar(10) NOT NULL default ' ',
  "AAABDFVR" varchar(30) NOT NULL default ' ',
  "AAABDFVS" varchar(30) NOT NULL default ' ',
  "AAABDFVT" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDFVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFWL","AAABDFWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDFWL" varchar(2) NOT NULL default ' ',
  "AAABDFWM" varchar(10) NOT NULL default ' ',
  "AAABDFWI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDFWL","AAABDFWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJETS" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDFWS" varchar(10) NOT NULL default ' ',
  "AAABMKWZ" varchar(1) NOT NULL default ' ',
  "AAABDGNH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJETS","AAABLUZL","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJETS" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(30) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJETS","AAABLUZL","AAABLTUX","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFXK" (
  "AAABDFYD" varchar(10) NOT NULL default ' ',
  "AAABDFXM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFXO" (
  "AAABDFYF" varchar(10) NOT NULL default ' ',
  "AAABDFXQ" varchar(30) NOT NULL default ' ',
  "AAABDFXR" varchar(30) NOT NULL default ' ',
  "AAABDFXS" varchar(30) NOT NULL default ' ',
  "AAABDFXT" varchar(30) NOT NULL default ' ',
  "AAABDFXU" varchar(30) NOT NULL default ' ',
  "AAABDFXV" varchar(4) NOT NULL default '0000',
  "AAABDFXW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABDFYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFXY" (
  "AAABDFYD" varchar(10) NOT NULL default ' ',
  "AAABDFYE" varchar(4) NOT NULL default '0000',
  "AAABDFYF" varchar(10) NOT NULL default ' ',
  "AAABDFYC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABDFYD","AAABDFYE","AAABDFYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDFYR" varchar(15) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDFYR","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABERXO" varchar(5) NOT NULL default '00000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDFYY" varchar(30) NOT NULL default ' ',
  "AAABLENH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABERXO"),
  KEY "AAABDFZD" ("AAABMRIL","AAABERXO","AAABDHPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLUSV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDFZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(12) NOT NULL default ' ',
  "AAABERXO" varchar(5) NOT NULL default '00000',
  "AAABDFZW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGBR" varchar(10) NOT NULL default ' ',
  "AAABDGAD" varchar(200) NOT NULL default ' ',
  "AAABDGAE" varchar(1) NOT NULL default ' ',
  "AAABDGAF" varchar(1) NOT NULL default ' ',
  "AAABDGAG" varchar(1) NOT NULL default ' ',
  "AAABDGAH" varchar(1) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABDGAK" varchar(1) NOT NULL default ' ',
  "AAABDGAL" varchar(10) NOT NULL default ' ',
  "AAABDGAM" varchar(10) NOT NULL default ' ',
  "AAABDGAN" varchar(5) NOT NULL default ' ',
  "AAABDGAO" varchar(10) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDGAQ" varchar(10) NOT NULL default ' ',
  "AAABDHOW" varchar(10) NOT NULL default ' ',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAABDGAU" varchar(1) NOT NULL default ' ',
  "AAABDGAV" varchar(1) NOT NULL default ' ',
  "AAABDGAW" varchar(1) NOT NULL default ' ',
  "AAABDGAX" varchar(1) NOT NULL default ' ',
  "AAABDGAY" varchar(1) NOT NULL default ' ',
  "AAABDGAZ" varchar(1) NOT NULL default ' ',
  "AAABDGBA" varchar(1) NOT NULL default ' ',
  "AAABDGBB" varchar(1) NOT NULL default ' ',
  "AAABDGBC" varchar(1) NOT NULL default ' ',
  "AAABDGBD" varchar(1) NOT NULL default ' ',
  "AAABDGBE" varchar(1) NOT NULL default ' ',
  "AAABDGBF" varchar(1) NOT NULL default ' ',
  "AAABDGBG" varchar(1) NOT NULL default ' ',
  "AAABDGBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGBR" varchar(10) NOT NULL default ' ',
  "AAABDGBO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGBS" (
  "AAABDGKN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDGKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGBV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGKN" varchar(20) NOT NULL default ' ',
  "AAABDGBY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDGKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLXFQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDGCZ" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(132) NOT NULL default ' ',
  "AAABMNNA" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAABERXO","AAABDGCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHRE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMJWN","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGPG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGPG" varchar(20) NOT NULL default ' ',
  "AAABDGDT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABDGEA" varchar(3) NOT NULL default '000',
  "AAABDGEB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGGC" varchar(20) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGGC","AAABDGFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGGC" varchar(20) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABDGEQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGGC","AAABDGFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABDGEZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNO","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGGC" varchar(20) NOT NULL default ' ',
  "AAABDGFS" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDGGC","AAABDGFS","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGFU" (
  "AAABDGGC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABDGGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGFX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGGC" varchar(20) NOT NULL default ' ',
  "AAABDGGA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDGGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHKB" varchar(1) NOT NULL default ' ',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABMLXR" varchar(132) NOT NULL default ' ',
  "AAABLJPA" varchar(1) NOT NULL default ' ',
  "AAABDGHS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGIB" varchar(30) NOT NULL default ' ',
  "AAABMNJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABDGIF" varchar(10) NOT NULL default ' ',
  "AAABDGJU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABDGIN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGJU" varchar(10) NOT NULL default ' ',
  "AAABDGIU" varchar(30) NOT NULL default ' ',
  "AAABJLQM" varchar(30) NOT NULL default ' ',
  "AAABDGIW" varchar(30) NOT NULL default ' ',
  "AAABDGIX" varchar(30) NOT NULL default ' ',
  "AAABDGIY" varchar(30) NOT NULL default ' ',
  "AAABDGIZ" varchar(40) NOT NULL default ' ',
  "AAABDGJA" varchar(4) NOT NULL default ' ',
  "AAABDGJB" varchar(30) NOT NULL default ' ',
  "AAABDGJC" varchar(30) NOT NULL default ' ',
  "AAABDGJD" varchar(30) NOT NULL default ' ',
  "AAABDGJE" varchar(10) NOT NULL default ' ',
  "AAABDGJF" varchar(10) NOT NULL default ' ',
  "AAABDGJG" varchar(10) NOT NULL default ' ',
  "AAABDGJH" varchar(10) NOT NULL default ' ',
  "AAABDGJI" varchar(10) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDGJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGJU" varchar(10) NOT NULL default ' ',
  "AAABDGJR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDGJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGJV" (
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDGJX" varchar(20) NOT NULL default ' ',
  "AAABDGJY" varchar(20) NOT NULL default ' ',
  "AAABDGJZ" varchar(20) NOT NULL default ' ',
  "AAABDGKA" varchar(30) NOT NULL default ' ',
  "AAABDGKB" varchar(30) NOT NULL default ' ',
  "AAABDGKC" varchar(30) NOT NULL default ' ',
  "AAABDGKD" varchar(4) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDGKN" varchar(20) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABDGKN","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGKP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABDGKS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGKV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLIO" varchar(3) NOT NULL default ' ',
  "AAABDGKY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGLB" (
  "AAABLLIO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHKB" varchar(1) NOT NULL default ' ',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGOA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGOA" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGNA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGOA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGNZ" varchar(10) NOT NULL default ' ',
  "AAABDGNH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGNZ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDGNO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGNZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGNZ" varchar(10) NOT NULL default ' ',
  "AAABDGOA" varchar(10) NOT NULL default ' ',
  "AAABDGOB" varchar(10) NOT NULL default ' ',
  "AAABDGNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGNZ","AAABDGOA","AAABDGOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHKB" varchar(1) NOT NULL default ' ',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGQZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGPG" varchar(20) NOT NULL default ' ',
  "AAABDGPH" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDGQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGQZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABFOBJ" varchar(10) NOT NULL default ' ',
  "AAABDGPT" varchar(1) NOT NULL default ' ',
  "AAABDQUN" varchar(1) NOT NULL default ' ',
  "AAABDGPV" varchar(1) NOT NULL default ' ',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGPX" varchar(1) NOT NULL default ' ',
  "AAABDGPY" varchar(2) NOT NULL default '00',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGQZ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGQZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGQZ","AAABLTLR","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGQZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABJZBY" varchar(4) NOT NULL default '0000',
  "AAABDHSV" varchar(30) NOT NULL default ' ',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(180) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGQZ","AAABLTLR","AAABJZBY","AAABDHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABJAAI" varchar(132) NOT NULL default ' ',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABMLCZ","AAABDGVH","AAABMQII","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGTD" varchar(3) NOT NULL default ' ',
  "AAABDGSC" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABJAAI" varchar(132) NOT NULL default ' ',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGTD","AAABDGSC","AAABMLCZ","AAABDGVH","AAABMQII","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGTD" varchar(3) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABJAAI" varchar(132) NOT NULL default ' ',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGTD","AAABMLCZ","AAABDGVH","AAABMQII","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGTD" varchar(3) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABJAAI" varchar(132) NOT NULL default ' ',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDGTD","AAABDGVH","AAABMQII","AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  "AAABMLEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABDGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABDGTX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABIYJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDGUL" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLUFA" tinyint(3) unsigned NOT NULL default '0',
  "AAABMALF" tinyint(3) unsigned NOT NULL default '0',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABDGUI" varchar(64) NOT NULL default ' ',
  "AAABERXO" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDGUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGUM" (
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHDT" varchar(30) NOT NULL default ' ',
  "AAABDHOX" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUR" varchar(1) NOT NULL default ' ',
  "AAABDGUS" varchar(1) NOT NULL default ' ',
  "AAABDGUT" varchar(1) NOT NULL default ' ',
  "AAABDHOH" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUV" varchar(4) NOT NULL default '0000',
  "AAABDGUW" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGVB" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUY" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABDHPD"),
  KEY "AAABDGVA" ("AAABDGVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRNO" varchar(3) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRRB" varchar(10) NOT NULL default ' ',
  "AAABDGVY" varchar(10) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABDHEH" varchar(30) NOT NULL default ' ',
  "AAABDHEI" varchar(30) NOT NULL default ' ',
  "AAABDGVS" varchar(3) NOT NULL default ' ',
  "AAABDGVT" varchar(30) NOT NULL default ' ',
  "AAABDGVU" varchar(30) NOT NULL default ' ',
  "AAABDGVV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRRB","AAABDGVY","AAABEAOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHKB" varchar(1) NOT NULL default ' ',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDGWP" varchar(1) NOT NULL default ' ',
  "AAABDGWQ" varchar(1) NOT NULL default ' ',
  "AAABDGWR" varchar(1) NOT NULL default ' ',
  "AAABDGWS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGXF" (
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  "AAABDGXH" varchar(1) NOT NULL default ' ',
  "AAABDGXI" varchar(1) NOT NULL default ' ',
  "AAABDGXJ" varchar(1) NOT NULL default ' ',
  "AAABDGXK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGXM" (
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJCM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHCX" varchar(10) NOT NULL default '0000000000',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  "AAABMLXR" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHCX" varchar(10) NOT NULL default '0000000000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDHBP" varchar(30) NOT NULL default ' ',
  "AAABMJCM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDGYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHCM" varchar(10) NOT NULL default '0000000000',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDGYZ" varchar(40) NOT NULL default ' ',
  "AAABLXUH" varchar(10) NOT NULL default '0000000000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  "AAABDKDC" varchar(10) NOT NULL default ' ',
  "AAABDGZE" varchar(3) NOT NULL default ' ',
  "AAABDGZF" varchar(3) NOT NULL default ' ',
  "AAABDGZG" varchar(3) NOT NULL default ' ',
  "AAABILPS" varchar(12) NOT NULL default ' ',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAABMCWV" varchar(12) NOT NULL default ' ',
  "AAABLYIE" varchar(8) NOT NULL default '00000000',
  "AAABDGZL" varchar(8) NOT NULL default '00000000',
  "AAABIEHB" varchar(8) NOT NULL default '00000000',
  "AAABDGZN" varchar(5) NOT NULL default '00000',
  "AAABDGZO" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(10) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABDGZU" varchar(1) NOT NULL default ' ',
  "AAABDGZV" varchar(10) NOT NULL default ' ',
  "AAABDGZW" varchar(20) NOT NULL default ' ',
  "AAABDGZX" varchar(70) NOT NULL default ' ',
  "AAABDGZY" varchar(70) NOT NULL default ' ',
  "AAABDGZZ" varchar(70) NOT NULL default ' ',
  "AAABDHAA" varchar(8) NOT NULL default '00000000',
  "AAABDHAB" varchar(8) NOT NULL default '00000000',
  "AAABFUWK" varchar(2) NOT NULL default '00',
  "AAABDHAD" varchar(20) NOT NULL default ' ',
  "AAABEMIH" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABDHAG" varchar(3) NOT NULL default ' ',
  "AAABDHAH" varchar(1) NOT NULL default ' ',
  "AAABDHAI" varchar(1) NOT NULL default ' ',
  "AAABDHAJ" varchar(1) NOT NULL default ' ',
  "AAABDHAK" varchar(1) NOT NULL default ' ',
  "AAABDHAL" varchar(1) NOT NULL default ' ',
  "AAABDHAM" varchar(10) NOT NULL default ' ',
  "AAABDHAN" varchar(3) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABDHAP" varchar(1) NOT NULL default ' ',
  "AAABDHAQ" varchar(1) NOT NULL default ' ',
  "AAABDHAR" varchar(1) NOT NULL default ' ',
  "AAABDHAS" varchar(1) NOT NULL default ' ',
  "AAABDHAT" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(20) NOT NULL default ' ',
  "AAABDHAV" varchar(18) NOT NULL default ' ',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAABDHAX" varchar(1) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABDHAZ" varchar(1) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDHBB" varchar(4) NOT NULL default '0000',
  "AAABLUPE" varchar(30) NOT NULL default ' ',
  "AAABDHBD" double NOT NULL default '0',
  "AAABDHBE" varchar(3) NOT NULL default ' ',
  "AAABDHBF" double NOT NULL default '0',
  "AAABDHBG" varchar(3) NOT NULL default ' ',
  "AAABDHBH" varchar(1) NOT NULL default ' ',
  "AAABDHBI" double NOT NULL default '0',
  "AAABDHBJ" double NOT NULL default '0',
  "AAABDHBK" varchar(1) NOT NULL default ' ',
  "AAABDHBL" double NOT NULL default '0',
  "AAABDHBM" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABDSTR" varchar(4) NOT NULL default ' ',
  "AAABDHBP" varchar(30) NOT NULL default ' ',
  "AAABDHBQ" double NOT NULL default '0',
  "AAABDHBR" double NOT NULL default '0',
  "AAABDHBS" varchar(1) NOT NULL default ' ',
  "AAABDHBT" varchar(30) NOT NULL default ' ',
  "AAABKMHU" varchar(30) NOT NULL default ' ',
  "AAABDHBV" varchar(1) NOT NULL default ' ',
  "AAABDHBW" varchar(1) NOT NULL default ' ',
  "AAABDHBX" varchar(1) NOT NULL default ' ',
  "AAABDHBY" varchar(1) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABLRYP" varchar(8) NOT NULL default '00000000',
  "AAABDHCB" varchar(30) NOT NULL default ' ',
  "AAABDHCC" double NOT NULL default '0',
  "AAABDHCD" varchar(10) NOT NULL default '0000000000',
  "AAABDHCE" varchar(3) NOT NULL default '000',
  "AAABDHCF" varchar(4) NOT NULL default '0000',
  "AAABDHCG" varchar(1) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABDHCI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHCX" varchar(10) NOT NULL default '0000000000',
  "AAABLJKC" int(11) NOT NULL default '0',
  "AAABMMRH" varchar(156) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHED" varchar(10) NOT NULL default '0000000000',
  "AAABDHDT" varchar(30) NOT NULL default ' ',
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHDX" varchar(1) NOT NULL default ' ',
  "AAABDHDY" varchar(1) NOT NULL default ' ',
  "AAABDHDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLTD" varchar(10) NOT NULL default '0000000000',
  "AAABDHED" varchar(10) NOT NULL default '0000000000',
  "AAABDHDT" varchar(30) NOT NULL default ' ',
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHDX" varchar(1) NOT NULL default ' ',
  "AAABDHDY" varchar(1) NOT NULL default ' ',
  "AAABDHDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLTD","AAABDHED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHEE" (
  "AAABDHEH" varchar(30) NOT NULL default ' ',
  "AAABDHEI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDHEH","AAABDHEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHEJ" (
  "AAABDHEP" varchar(4) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDHEM" varchar(70) NOT NULL default ' ',
  "AAABDHEN" int(11) NOT NULL default '0',
  "AAABDHEO" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABDHEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHEW" varchar(3) NOT NULL default ' ',
  "AAABDHET" varchar(3) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHFV" varchar(5) NOT NULL default ' ',
  "AAABDHFA" varchar(15) NOT NULL default ' ',
  "AAABJDNK" varchar(30) NOT NULL default ' ',
  "AAABDHFC" varchar(1) NOT NULL default ' ',
  "AAABDHFD" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHFV" varchar(5) NOT NULL default ' ',
  "AAABDHFL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHFV" varchar(5) NOT NULL default ' ',
  "AAABDHGT" varchar(5) NOT NULL default ' ',
  "AAABDHFT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHFV","AAABDHGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHFX" (
  "AAABJDNK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJDNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHGA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJDNK" varchar(30) NOT NULL default ' ',
  "AAABDHGD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJDNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHGG" (
  "AAABDHGT" varchar(5) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABDHGJ" varchar(70) NOT NULL default ' ',
  "AAABDHGK" varchar(1) NOT NULL default ' ',
  "AAABDHGL" int(11) NOT NULL default '0',
  "AAABDHGM" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABDHGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHGO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHGT" varchar(5) NOT NULL default ' ',
  "AAABDHGR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABDHGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABDHSV" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABHZAW" varchar(20) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABJZBY" varchar(4) NOT NULL default '0000',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(180) NOT NULL default ' ',
  "AAABDHSS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEUZ","AAABDHSV","AAABMDEP","AAABHZAW","AAABLTLR","AAABJZBY"),
  KEY "AAABDHHP" ("AAABMRIL","AAABLTLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABDHHV" varchar(1) NOT NULL default ' ',
  "AAABDHHW" varchar(1) NOT NULL default ' ',
  "AAABDHHX" varchar(1) NOT NULL default ' ',
  "AAABDHHY" varchar(1) NOT NULL default ' ',
  "AAABDHHZ" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO","AAABDHIL","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDHIZ" varchar(40) NOT NULL default ' ',
  "AAABDHJA" varchar(4) NOT NULL default ' ',
  "AAABDHJB" varchar(30) NOT NULL default ' ',
  "AAABDHJC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDHIZ" varchar(40) NOT NULL default ' ',
  "AAABDHJA" varchar(4) NOT NULL default ' ',
  "AAABDHJB" varchar(30) NOT NULL default ' ',
  "AAABDHJC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABDHJS" varchar(20) NOT NULL default ' ',
  "AAABDHJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXI" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHJS" varchar(20) NOT NULL default ' ',
  "AAABDHJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDHKB" varchar(1) NOT NULL default ' ',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLUNJ" varchar(10) NOT NULL default ' ',
  "AAABMCNO" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHKM" varchar(1) NOT NULL default ' ',
  "AAABDHKN" varchar(1) NOT NULL default ' ',
  "AAABDHKO" varchar(1) NOT NULL default ' ',
  "AAABDHKP" varchar(1) NOT NULL default ' ',
  "AAABDHKQ" varchar(1) NOT NULL default ' ',
  "AAABDHKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHLC" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHLA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHLC","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHLE" (
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
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDHPA" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHMC" (
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
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABDHPA" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHMZ" (
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
  "AAABDHNP" varchar(1) NOT NULL default ' ',
  "AAABDHNQ" varchar(1) NOT NULL default '0',
  "AAABDHNR" varchar(1) NOT NULL default '0',
  "AAABDHNS" varchar(1) NOT NULL default ' ',
  "AAABDHNT" varchar(6) NOT NULL default '000000',
  "AAABDHNU" varchar(8) NOT NULL default '00000000',
  "AAABDHNV" varchar(1) NOT NULL default ' ',
  "AAABDHOF" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHOD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHOH" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAABDHOC" ("AAABDHOD"),
  KEY "AAABDHOE" ("AAABDHOF"),
  KEY "AAABDHOG" ("AAABDHOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHOI" (
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
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABDHPA" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHRL" varchar(10) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHRL","AAABDHQT","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHRL" varchar(10) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABDHRE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHRL","AAABDHQT","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHRL" varchar(10) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHRL","AAABDHQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHRL" varchar(10) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHRL","AAABDHQT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHQV" (
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKXRS","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHRA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABDHRE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKXRS","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHRI" (
  "AAABDHRL" varchar(10) NOT NULL default ' ',
  "AAABDHRK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABDHRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHSG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHSG" varchar(10) NOT NULL default ' ',
  "AAABJVTD" varchar(6) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHSG","AAABJVTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHSG" varchar(10) NOT NULL default ' ',
  "AAABDHSD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABDHSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABDHSV" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABMNMH" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(180) NOT NULL default ' ',
  "AAABDHSS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEUZ","AAABDHSV","AAABLPJD","AAABLTLR"),
  KEY "AAABDHSY" ("AAABMRIL","AAABLTLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABGUFU" varchar(10) NOT NULL default ' ',
  "AAABDHTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABDHTT" varchar(8) NOT NULL default '00000000',
  "AAABDHTU" varchar(12) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABDHTX" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHVJ" varchar(10) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHVJ","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHUM" varchar(10) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHUM","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABDHVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABDHVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHVJ" varchar(10) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHVJ","AAABESGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHWA" varchar(2) NOT NULL default ' ',
  "AAABDHVW" varchar(1) NOT NULL default ' ',
  "AAABDHVX" varchar(30) NOT NULL default ' ',
  "AAABDHVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXDU" varchar(8) NOT NULL default ' ',
  "AAABDHWL" varchar(4) NOT NULL default '0000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABDHWG" varchar(6) NOT NULL default ' ',
  "AAABDHWH" varchar(30) NOT NULL default ' ',
  "AAABDHWI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXDU","AAABDHWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGXDU" varchar(8) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGXDU","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFND" varchar(30) NOT NULL default ' ',
  "AAABIHDR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHXA" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABERRL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHXE" (
  "AAABIMRZ" varchar(20) NOT NULL default ' ',
  "AAABDHXK" varchar(30) NOT NULL default ' ',
  "AAABMNRG" tinyint(3) unsigned NOT NULL default '0',
  "AAABDHXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMRZ","AAABDHXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABDHYF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABDHYJ" varchar(1) NOT NULL default ' ',
  "AAABDHYA" varchar(1) NOT NULL default ' ',
  "AAABMLEL" int(11) NOT NULL default '0',
  "AAABIZSI" int(11) NOT NULL default '0',
  "AAABIZSJ" smallint(6) NOT NULL default '0',
  "AAABJVTD" varchar(6) NOT NULL default ' ',
  "AAABDHYF" varchar(3) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ","AAABDHYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNBZ" varchar(10) NOT NULL default ' ',
  "AAABDIBW" varchar(13) NOT NULL default ' ',
  "AAABDIBX" varchar(30) NOT NULL default ' ',
  "AAABJFBB" varchar(38) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOUB" varchar(7) NOT NULL default ' ',
  "AAABDICJ" varchar(4) NOT NULL default '0000',
  "AAABDHYW" varchar(30) NOT NULL default ' ',
  "AAABGNBZ" varchar(21) NOT NULL default ' ',
  "AAABDICD" varchar(3) NOT NULL default '000',
  "AAABDICF" varchar(3) NOT NULL default '000',
  "AAABJFBB" varchar(38) NOT NULL default ' ',
  "AAABHFFR" varchar(4) NOT NULL default ' ',
  "AAABDIBS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOUB","AAABDICJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHZG" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIYJK" varchar(5) NOT NULL default ' ',
  "AAABDSFP" varchar(2) NOT NULL default ' ',
  "AAABFLUQ" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABIYJK","AAABDSFP","AAABFLUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDHZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDIBI" varchar(3) NOT NULL default ' ',
  "AAABDHZT" varchar(1) NOT NULL default ' ',
  "AAABDHZU" varchar(1) NOT NULL default ' ',
  "AAABDHZV" varchar(1) NOT NULL default ' ',
  "AAABDHZW" varchar(2) NOT NULL default ' ',
  "AAABDHZX" varchar(5) NOT NULL default '00000',
  "AAABDHZY" varchar(3) NOT NULL default '000',
  "AAABDHZZ" varchar(3) NOT NULL default '000',
  "AAABDIAA" varchar(1) NOT NULL default ' ',
  "AAABDIAB" varchar(5) NOT NULL default '00000',
  "AAABDIAC" varchar(1) NOT NULL default ' ',
  "AAABDIAD" varchar(1) NOT NULL default ' ',
  "AAABDIAE" varchar(1) NOT NULL default ' ',
  "AAABDIAF" varchar(1) NOT NULL default ' ',
  "AAABDIAG" varchar(1) NOT NULL default ' ',
  "AAABDIAH" varchar(4) NOT NULL default ' ',
  "AAABDIAI" varchar(3) NOT NULL default ' ',
  "AAABDIAJ" varchar(3) NOT NULL default ' ',
  "AAABDIAK" varchar(1) NOT NULL default ' ',
  "AAABDIAL" varchar(1) NOT NULL default ' ',
  "AAABDIAM" varchar(1) NOT NULL default ' ',
  "AAABDIAN" varchar(1) NOT NULL default ' ',
  "AAABDIAO" varchar(1) NOT NULL default ' ',
  "AAABDIAP" varchar(1) NOT NULL default ' ',
  "AAABDIAQ" varchar(1) NOT NULL default ' ',
  "AAABDIAR" varchar(1) NOT NULL default ' ',
  "AAABDIAS" varchar(1) NOT NULL default ' ',
  "AAABDIAT" varchar(1) NOT NULL default ' ',
  "AAABDIAU" varchar(1) NOT NULL default ' ',
  "AAABDIAV" varchar(1) NOT NULL default ' ',
  "AAABDIAW" varchar(1) NOT NULL default ' ',
  "AAABDIAX" varchar(1) NOT NULL default ' ',
  "AAABDIAY" varchar(1) NOT NULL default ' ',
  "AAABDIAZ" varchar(1) NOT NULL default ' ',
  "AAABDIBA" varchar(1) NOT NULL default ' ',
  "AAABDIBB" varchar(1) NOT NULL default ' ',
  "AAABDIBC" varchar(1) NOT NULL default ' ',
  "AAABDIBD" varchar(1) NOT NULL default ' ',
  "AAABDIBE" varchar(1) NOT NULL default ' ',
  "AAABDIBF" varchar(1) NOT NULL default ' ',
  "AAABDIBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDIBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABDIBW" varchar(13) NOT NULL default ' ',
  "AAABDIBX" varchar(30) NOT NULL default ' ',
  "AAABDICF" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(38) NOT NULL default ' ',
  "AAABHFMF" varchar(10) NOT NULL default ' ',
  "AAABHFFR" varchar(4) NOT NULL default ' ',
  "AAABDIBS" varchar(5) NOT NULL default ' ',
  "AAABDIBT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGNBZ","AAABDIBW","AAABDIBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOUB" varchar(7) NOT NULL default ' ',
  "AAABDICJ" varchar(4) NOT NULL default '0000',
  "AAABGNBZ" varchar(30) NOT NULL default ' ',
  "AAABDICD" varchar(3) NOT NULL default '000',
  "AAABHTLA" varchar(70) NOT NULL default ' ',
  "AAABDICF" varchar(3) NOT NULL default '000',
  "AAABMDXM" varchar(38) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOUB","AAABDICJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDICK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQYU" varchar(1) NOT NULL default ' ',
  "AAABMNPV" varchar(1) NOT NULL default '0',
  "AAABDICO" varchar(1) NOT NULL default ' ',
  "AAABDICP" varchar(1) NOT NULL default ' ',
  "AAABDICQ" varchar(1) NOT NULL default ' ',
  "AAABDICR" varchar(1) NOT NULL default ' ',
  "AAABDICS" varchar(1) NOT NULL default ' ',
  "AAABDICT" varchar(1) NOT NULL default ' ',
  "AAABDICU" varchar(1) NOT NULL default ' ',
  "AAABDICV" varchar(1) NOT NULL default ' ',
  "AAABDICW" varchar(1) NOT NULL default ' ',
  "AAABLTXN" varchar(1) NOT NULL default ' ',
  "AAABDICY" varchar(1) NOT NULL default ' ',
  "AAABDICZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFN" varchar(8) NOT NULL default '00000000',
  "AAABKRKS" varchar(25) NOT NULL default ' ',
  "AAABDIDG" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFP" varchar(8) NOT NULL default '00000000',
  "AAABKRFQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABDIDY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKRFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJDPM" varchar(7) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLCKD" varchar(2) NOT NULL default ' ',
  "AAABKRBS" varchar(1) NOT NULL default '0',
  "AAABKRBT" varchar(1) NOT NULL default '0',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABKRBX" varchar(7) NOT NULL default ' ',
  "AAABKRBV" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(1) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABDIEO" tinyint(3) unsigned NOT NULL default '0',
  "AAABKRGB" varchar(1) NOT NULL default '0',
  "AAABKRGC" varchar(1) NOT NULL default ' ',
  "AAABKRBB" varchar(8) NOT NULL default ' ',
  "AAABKRBC" varchar(8) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAABDIEY" varchar(6) NOT NULL default ' ',
  "AAABDIEZ" varchar(4) NOT NULL default ' ',
  "AAABDIFA" varchar(12) NOT NULL default ' ',
  "AAABDIFB" varchar(4) NOT NULL default ' ',
  "AAABDIFC" varchar(12) NOT NULL default ' ',
  "AAABDIFD" varchar(4) NOT NULL default ' ',
  "AAABDIFE" varchar(12) NOT NULL default ' ',
  "AAABKRHM" varchar(16) NOT NULL default ' ',
  "AAABKRHN" varchar(16) NOT NULL default ' ',
  "AAABKRHO" varchar(16) NOT NULL default ' ',
  "AAABKRHP" varchar(16) NOT NULL default ' ',
  "AAABLCNC" varchar(7) NOT NULL default ' ',
  "AAABKRBA" varchar(1) NOT NULL default ' ',
  "AAABKRBZ" varchar(6) NOT NULL default ' ',
  "AAABKRBU" varchar(3) NOT NULL default ' ',
  "AAABDIFN" varchar(1) NOT NULL default ' ',
  "AAABDIFO" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKQEX" varchar(1) NOT NULL default ' ',
  "AAABKRCK" varchar(1) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABKRHT" varchar(1) NOT NULL default ' ',
  "AAABDIFY" varchar(1) NOT NULL default ' ',
  "AAABKRFV" varchar(2) NOT NULL default ' ',
  "AAABDIGA" varchar(12) NOT NULL default ' ',
  "AAABKRHV" varchar(7) NOT NULL default ' ',
  "AAABDIGC" varchar(10) NOT NULL default ' ',
  "AAABDIGD" varchar(10) NOT NULL default ' ',
  "AAABKRHW" varchar(6) NOT NULL default ' ',
  "AAABDIGF" varchar(12) NOT NULL default ' ',
  "AAABKRCP" varchar(12) NOT NULL default ' ',
  "AAABDIGH" varchar(1) NOT NULL default ' ',
  "AAABDIGI" varchar(1) NOT NULL default ' ',
  "AAABDIGJ" varchar(1) NOT NULL default ' ',
  "AAABDONM" varchar(7) NOT NULL default ' ',
  "AAABKRCR" varchar(6) NOT NULL default ' ',
  "AAABDIGM" varchar(10) NOT NULL default ' ',
  "AAABDIGN" varchar(10) NOT NULL default ' ',
  "AAABDIGO" varchar(10) NOT NULL default ' ',
  "AAABKWYM" varchar(6) NOT NULL default ' ',
  "AAABKRIB" varchar(1) NOT NULL default ' ',
  "AAABDIGR" varchar(3) NOT NULL default '000',
  "AAABDIGS" varchar(3) NOT NULL default '000',
  "AAABDIGT" varchar(3) NOT NULL default '000',
  "AAABDIGU" varchar(1) NOT NULL default ' ',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABDIGW" varchar(7) NOT NULL default ' ',
  "AAABDIGX" varchar(1) NOT NULL default ' ',
  "AAABDOOL" varchar(7) NOT NULL default ' ',
  "AAABDIGZ" varchar(7) NOT NULL default ' ',
  "AAABDIHA" varchar(1) NOT NULL default ' ',
  "AAABLXVG" varchar(1) NOT NULL default ' ',
  "AAABKRCZ" varchar(7) NOT NULL default ' ',
  "AAABDIHD" varchar(2) NOT NULL default ' ',
  "AAABDIHE" varchar(2) NOT NULL default ' ',
  "AAABDIHF" varchar(2) NOT NULL default ' ',
  "AAABKRDA" varchar(1) NOT NULL default ' ',
  "AAABKRDB" varchar(1) NOT NULL default ' ',
  "AAABKRIL" varchar(6) NOT NULL default ' ',
  "AAABKRDF" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(2) NOT NULL default ' ',
  "AAABLKXQ" varchar(4) NOT NULL default ' ',
  "AAABKRDU" varchar(1) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABKRDL" varchar(8) NOT NULL default ' ',
  "AAABKRDM" varchar(2) NOT NULL default '00',
  "AAABEDTX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJDPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDPM" varchar(7) NOT NULL default ' ',
  "AAABFBAZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJDPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDIIT" varchar(5) NOT NULL default ' ',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABDIII" varchar(1) NOT NULL default ' ',
  "AAABDIIJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDIIT" varchar(5) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDIIT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFL" varchar(2) NOT NULL default '00',
  "AAABGLFM" varchar(4) NOT NULL default ' ',
  "AAABKRFT" varchar(4) NOT NULL default ' ',
  "AAABDIJU" varchar(1) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABKRXG" varchar(1) NOT NULL default ' ',
  "AAABDIJC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFL","AAABGLFM","AAABKRFT","AAABDIJU","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFL" varchar(2) NOT NULL default ' ',
  "AAABGLFM" varchar(4) NOT NULL default ' ',
  "AAABKRFT" varchar(4) NOT NULL default ' ',
  "AAABDIJU" varchar(1) NOT NULL default ' ',
  "AAABDIJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFL","AAABGLFM","AAABKRFT","AAABDIJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABDIKA" varchar(1) NOT NULL default ' ',
  "AAABDIKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABLZUS","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABDIKT" varchar(4) NOT NULL default ' ',
  "AAABLOAI" varchar(20) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDIKN" decimal(15,2) NOT NULL default '0.00',
  "AAABDIPC" varchar(12) NOT NULL default ' ',
  "AAABDIKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABDIKT","AAABLOAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABDIPO" varchar(8) NOT NULL default ' ',
  "AAABDIVA" varchar(4) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABDILD" varchar(8) NOT NULL default '00000000',
  "AAABDIPP" varchar(1) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABKGOS","AAABDIPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDILL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABDILQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLPMM","AAABDIWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDILV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABDIWH" varchar(10) NOT NULL default ' ',
  "AAABDIWI" varchar(3) NOT NULL default '000',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVHQ" varchar(35) NOT NULL default ' ',
  "AAABLVHK" varchar(10) NOT NULL default ' ',
  "AAABLVHL" varchar(10) NOT NULL default ' ',
  "AAABJWRJ" varchar(16) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABLCJJ" varchar(16) NOT NULL default ' ',
  "AAABLCJK" varchar(11) NOT NULL default ' ',
  "AAABLCED" varchar(1) NOT NULL default ' ',
  "AAABLCJL" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABJWRW" varchar(2) NOT NULL default ' ',
  "AAABJWRX" varchar(1) NOT NULL default ' ',
  "AAABLXWO" varchar(1) NOT NULL default ' ',
  "AAABJWRV" varchar(16) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRLR" varchar(2) NOT NULL default ' ',
  "AAABJRLS" varchar(2) NOT NULL default ' ',
  "AAABLCEO" varchar(1) NOT NULL default ' ',
  "AAABJRLU" varchar(18) NOT NULL default ' ',
  "AAABJRLV" varchar(18) NOT NULL default ' ',
  "AAABJRLW" varchar(20) NOT NULL default ' ',
  "AAABJRLX" varchar(10) NOT NULL default ' ',
  "AAABJRLY" varchar(30) NOT NULL default ' ',
  "AAABJRLZ" varchar(30) NOT NULL default ' ',
  "AAABLVHD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABKGOS","AAABDIWH","AAABDIWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDINR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDINX" varchar(2) NOT NULL default ' ',
  "AAABDINY" varchar(2) NOT NULL default ' ',
  "AAABDINZ" varchar(2) NOT NULL default ' ',
  "AAABDIOA" varchar(2) NOT NULL default ' ',
  "AAABDIOB" varchar(2) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABDIOD" varchar(40) NOT NULL default ' ',
  "AAABDIOE" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABDIOG" varchar(1) NOT NULL default ' ',
  "AAABDIOH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABDIWH" varchar(10) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDIOS" decimal(15,2) NOT NULL default '0.00',
  "AAABDIOT" decimal(15,2) NOT NULL default '0.00',
  "AAABDIOU" decimal(13,2) NOT NULL default '0.00',
  "AAABDIOV" decimal(15,2) NOT NULL default '0.00',
  "AAABDIOW" decimal(13,2) NOT NULL default '0.00',
  "AAABDIOX" varchar(35) NOT NULL default ' ',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABDIOZ" varchar(16) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABDIQF" varchar(2) NOT NULL default ' ',
  "AAABDIPC" varchar(12) NOT NULL default ' ',
  "AAABDIPD" varchar(1) NOT NULL default ' ',
  "AAABDIRG" decimal(7,3) NOT NULL default '0.000',
  "AAABDIQM" varchar(10) NOT NULL default ' ',
  "AAABMJVD" varchar(100) NOT NULL default ' ',
  "AAABMJVE" varchar(30) NOT NULL default ' ',
  "AAABDIPI" varchar(1) NOT NULL default ' ',
  "AAABDIPJ" varchar(13) NOT NULL default '0000000000000',
  "AAABDIPK" varchar(14) NOT NULL default ' ',
  "AAABENNS" varchar(15) NOT NULL default ' ',
  "AAABENNT" varchar(18) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAABDIPO" varchar(8) NOT NULL default ' ',
  "AAABDIPP" varchar(1) NOT NULL default ' ',
  "AAABDIPQ" varchar(10) NOT NULL default ' ',
  "AAABDIPR" decimal(9,5) NOT NULL default '0.00000',
  "AAABJKXJ" varchar(4) NOT NULL default ' ',
  "AAABJKRT" varchar(4) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAABMCUB" varchar(3) NOT NULL default ' ',
  "AAABMCUC" varchar(3) NOT NULL default ' ',
  "AAABDIPX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABKGOS","AAABDIWH"),
  KEY "AAABDIQD" ("AAABMRIL","AAABDIQF","AAABGVKJ","AAABLPMM","AAABDIWF"),
  KEY "AAABDIQJ" ("AAABLPMM","AAABDIWF","AAABDIQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABDIWH" varchar(10) NOT NULL default ' ',
  "AAABDIWI" varchar(3) NOT NULL default '000',
  "AAABGVGA" varchar(2) NOT NULL default ' ',
  "AAABDIVA" varchar(4) NOT NULL default ' ',
  "AAABDIUJ" varchar(1) NOT NULL default ' ',
  "AAABGVKJ" varchar(8) NOT NULL default '00000000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABDIQZ" decimal(15,2) NOT NULL default '0.00',
  "AAABDIRA" decimal(15,2) NOT NULL default '0.00',
  "AAABDIRB" decimal(13,2) NOT NULL default '0.00',
  "AAABDIRC" decimal(15,2) NOT NULL default '0.00',
  "AAABDIRD" decimal(13,2) NOT NULL default '0.00',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABDIRG" decimal(7,3) NOT NULL default '0.000',
  "AAABDIRH" varchar(50) NOT NULL default ' ',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKRSG" varchar(4) NOT NULL default '0000',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABKRSZ" varchar(1) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROS" varchar(4) NOT NULL default '0000',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCWV" varchar(6) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABDISR" varchar(1) NOT NULL default ' ',
  "AAABMCWX" varchar(3) NOT NULL default ' ',
  "AAABMCXS" varchar(3) NOT NULL default ' ',
  "AAABMCWY" varchar(10) NOT NULL default ' ',
  "AAABDISV" varchar(2) NOT NULL default ' ',
  "AAABDISW" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABJRZB" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMDAK" varchar(2) NOT NULL default ' ',
  "AAABMDAL" varchar(50) NOT NULL default ' ',
  "AAABKLYS" varchar(13) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABJFWS" varchar(15) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABHZQZ" varchar(10) NOT NULL default ' ',
  "AAABDITP" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABKGOS","AAABDIWH","AAABDIWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDITW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAABGCHL" varchar(14) NOT NULL default ' ',
  "AAABMPMH" varchar(5) NOT NULL default ' ',
  "AAABDIUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIVA" varchar(4) NOT NULL default ' ',
  "AAABDIUJ" varchar(1) NOT NULL default ' ',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPMG" varchar(2) NOT NULL default ' ',
  "AAABDIUM" varchar(1) NOT NULL default ' ',
  "AAABDIUN" varchar(1) NOT NULL default ' ',
  "AAABDIUO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIVA" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDIUX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIVA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABDIWF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABDIWH" varchar(10) NOT NULL default ' ',
  "AAABDIWI" varchar(3) NOT NULL default '000',
  "AAABDIWJ" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAABJFYN" varchar(6) NOT NULL default '000000',
  "AAABLTNT" varchar(2) NOT NULL default ' ',
  "AAABLTNU" varchar(2) NOT NULL default ' ',
  "AAABLOGP" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGQ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGR" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGS" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHY" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHZ" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIA" decimal(15,2) NOT NULL default '0.00',
  "AAABLOIB" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHT" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHU" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHV" decimal(15,2) NOT NULL default '0.00',
  "AAABLOHW" decimal(15,2) NOT NULL default '0.00',
  "AAABLOGT" varchar(1) NOT NULL default ' ',
  "AAABLOHC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABDIWF","AAABKGOS","AAABDIWH","AAABDIWI","AAABDIWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABJIYN" varchar(1) NOT NULL default ' ',
  "AAABDIWP" varchar(1) NOT NULL default ' ',
  "AAABDIWQ" varchar(1) NOT NULL default ' ',
  "AAABDIWR" varchar(1) NOT NULL default ' ',
  "AAABKUHU" varchar(1) NOT NULL default ' ',
  "AAABDIWT" varchar(1) NOT NULL default ' ',
  "AAABDIWU" varchar(1) NOT NULL default ' ',
  "AAABDIWV" varchar(1) NOT NULL default ' ',
  "AAABDIWW" varchar(1) NOT NULL default ' ',
  "AAABDIWX" varchar(1) NOT NULL default ' ',
  "AAABDIWY" varchar(1) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABDIXA" varchar(1) NOT NULL default ' ',
  "AAABGCHR" varchar(1) NOT NULL default ' ',
  "AAABDIXC" varchar(2) NOT NULL default '00',
  "AAABDIXD" varchar(1) NOT NULL default ' ',
  "AAABDIXE" varchar(1) NOT NULL default ' ',
  "AAABDJLX" varchar(2) NOT NULL default '00',
  "AAABFSDS" varchar(1) NOT NULL default '0',
  "AAABDIXH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABDJIE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDJNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABDIXX" varchar(1) NOT NULL default ' ',
  "AAABDIXY" varchar(1) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABDIYA" varchar(2) NOT NULL default ' ',
  "AAABDIYB" varchar(2) NOT NULL default ' ',
  "AAABDJBV" varchar(2) NOT NULL default ' ',
  "AAABDIYD" varchar(2) NOT NULL default ' ',
  "AAABDIYE" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABDIYG" varchar(1) NOT NULL default ' ',
  "AAABDIYH" varchar(1) NOT NULL default ' ',
  "AAABDIYI" varchar(1) NOT NULL default ' ',
  "AAABDIYJ" varchar(4) NOT NULL default ' ',
  "AAABDIYK" varchar(1) NOT NULL default ' ',
  "AAABDIYL" varchar(1) NOT NULL default '0',
  "AAABGCHR" varchar(1) NOT NULL default ' ',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABDIYO" varchar(1) NOT NULL default '0',
  "AAABKWVJ" varchar(1) NOT NULL default ' ',
  "AAABDIYQ" varchar(1) NOT NULL default ' ',
  "AAABDIYR" varchar(1) NOT NULL default '0',
  "AAABDJMM" varchar(4) NOT NULL default ' ',
  "AAABDIYT" varchar(1) NOT NULL default ' ',
  "AAABDIYU" varchar(1) NOT NULL default ' ',
  "AAABDIYV" varchar(1) NOT NULL default ' ',
  "AAABDIYW" varchar(1) NOT NULL default ' ',
  "AAABDIYX" varchar(1) NOT NULL default ' ',
  "AAABDIYY" varchar(1) NOT NULL default ' ',
  "AAABDIYZ" varchar(1) NOT NULL default ' ',
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABDIZB" varchar(1) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABLXUP" varchar(4) NOT NULL default ' ',
  "AAABDIZE" varchar(4) NOT NULL default ' ',
  "AAABIINC" varchar(2) NOT NULL default '00',
  "AAABDIZG" varchar(1) NOT NULL default ' ',
  "AAABDIZH" varchar(1) NOT NULL default ' ',
  "AAABDIZI" varchar(1) NOT NULL default ' ',
  "AAABDIZJ" varchar(4) NOT NULL default ' ',
  "AAABDIZK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABDIZR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDIZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABDJOY" varchar(1) NOT NULL default ' ',
  "AAABDJOZ" varchar(1) NOT NULL default ' ',
  "AAABDJPA" varchar(1) NOT NULL default ' ',
  "AAABDJPB" varchar(1) NOT NULL default ' ',
  "AAABDJPC" varchar(1) NOT NULL default ' ',
  "AAABDJPD" varchar(1) NOT NULL default ' ',
  "AAABDJPE" varchar(1) NOT NULL default ' ',
  "AAABDJPF" varchar(1) NOT NULL default ' ',
  "AAABDJPG" varchar(1) NOT NULL default ' ',
  "AAABDJPH" varchar(1) NOT NULL default ' ',
  "AAABDJPI" varchar(1) NOT NULL default ' ',
  "AAABDJPJ" varchar(1) NOT NULL default ' ',
  "AAABDJPK" varchar(1) NOT NULL default ' ',
  "AAABDJPL" varchar(1) NOT NULL default ' ',
  "AAABDJPM" varchar(1) NOT NULL default ' ',
  "AAABDJPN" varchar(1) NOT NULL default ' ',
  "AAABDJPO" varchar(1) NOT NULL default ' ',
  "AAABDJPP" varchar(1) NOT NULL default ' ',
  "AAABDJPQ" varchar(1) NOT NULL default ' ',
  "AAABDJPR" varchar(1) NOT NULL default ' ',
  "AAABDJPS" varchar(1) NOT NULL default ' ',
  "AAABDJPT" varchar(1) NOT NULL default ' ',
  "AAABDJPU" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABDJPW" varchar(6) NOT NULL default ' ',
  "AAABDJPX" varchar(1) NOT NULL default ' ',
  "AAABDJPY" varchar(1) NOT NULL default ' ',
  "AAABDJPZ" varchar(1) NOT NULL default ' ',
  "AAABDJQA" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABDJQK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABDJBV" varchar(2) NOT NULL default ' ',
  "AAABDJBW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC","AAABMREX","AAABMCRJ","AAABMCVI","AAABLXUA","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLLWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  "AAABDJCV" decimal(5,3) NOT NULL default '0.000',
  "AAABDJCW" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABFTNB","AAABLLWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABDJHW" varchar(1) NOT NULL default ' ',
  "AAABDJHX" varchar(3) NOT NULL default ' ',
  "AAABDJHY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABDJIE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDJIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABDJEI" varchar(10) NOT NULL default ' ',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVV","AAABDJEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABDJEI" varchar(10) NOT NULL default ' ',
  "AAABDJEE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVV","AAABDJEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABDJEM" varchar(1) NOT NULL default ' ',
  "AAABDJEN" varchar(1) NOT NULL default ' ',
  "AAABDJEO" varchar(1) NOT NULL default ' ',
  "AAABDJEP" varchar(1) NOT NULL default ' ',
  "AAABDJEQ" varchar(1) NOT NULL default ' ',
  "AAABDJER" varchar(1) NOT NULL default ' ',
  "AAABDJES" varchar(1) NOT NULL default ' ',
  "AAABDJET" varchar(1) NOT NULL default ' ',
  "AAABDJEU" varchar(1) NOT NULL default ' ',
  "AAABDJEV" varchar(1) NOT NULL default ' ',
  "AAABDJEW" varchar(1) NOT NULL default ' ',
  "AAABDJEX" varchar(1) NOT NULL default ' ',
  "AAABDJEY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABDJFF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGLCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJFJ" (
  "AAABMNLV" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(2) NOT NULL default ' ',
  "AAABMNLY" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABDJFU" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNLV","AAABMNLX","AAABMNLY","AAABMDER","AAABDJFU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABDJGL" varchar(2) NOT NULL default ' ',
  "AAABDJGM" varchar(1) NOT NULL default ' ',
  "AAABDJGN" varchar(1) NOT NULL default ' ',
  "AAABDJGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLBY","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABDTOD" varchar(4) NOT NULL default ' ',
  "AAABDJGL" varchar(2) NOT NULL default ' ',
  "AAABDJGM" varchar(1) NOT NULL default ' ',
  "AAABDJGN" varchar(1) NOT NULL default ' ',
  "AAABDJGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABDJKE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGLBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDJKE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGLBY","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKRBW" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLBY","AAABMCVI","AAABKRBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJHT" (
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABDJHW" varchar(1) NOT NULL default ' ',
  "AAABDJHX" varchar(3) NOT NULL default ' ',
  "AAABDJHY" varchar(3) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDJIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJIB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDJIG" varchar(1) NOT NULL default ' ',
  "AAABDJIE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABDJIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJMM" varchar(4) NOT NULL default ' ',
  "AAABDJIK" varchar(1) NOT NULL default '0',
  "AAABDJIL" varchar(1) NOT NULL default '0',
  "AAABDJIM" varchar(1) NOT NULL default '0',
  "AAABDJIN" varchar(3) NOT NULL default '000',
  "AAABDJIO" varchar(3) NOT NULL default '000',
  "AAABDJIP" varchar(3) NOT NULL default '000',
  "AAABDJIQ" varchar(4) NOT NULL default ' ',
  "AAABDJIR" varchar(4) NOT NULL default ' ',
  "AAABDJIS" varchar(4) NOT NULL default ' ',
  "AAABDJIT" varchar(4) NOT NULL default ' ',
  "AAABDJIU" varchar(4) NOT NULL default ' ',
  "AAABDJIV" varchar(4) NOT NULL default ' ',
  "AAABDJIW" varchar(1) NOT NULL default ' ',
  "AAABDJIX" varchar(1) NOT NULL default ' ',
  "AAABDJIY" varchar(1) NOT NULL default ' ',
  "AAABDJIZ" varchar(2) NOT NULL default '00',
  "AAABDJJA" varchar(2) NOT NULL default '00',
  "AAABDJJB" varchar(2) NOT NULL default '00',
  "AAABDJJC" varchar(2) NOT NULL default '00',
  "AAABDJJD" varchar(2) NOT NULL default '00',
  "AAABDJJE" varchar(2) NOT NULL default '00',
  "AAABDJJF" varchar(1) NOT NULL default '0',
  "AAABDJJG" varchar(1) NOT NULL default '0',
  "AAABDJJH" varchar(1) NOT NULL default '0',
  "AAABDJJI" varchar(3) NOT NULL default '000',
  "AAABDJJJ" varchar(3) NOT NULL default '000',
  "AAABDJJK" varchar(3) NOT NULL default '000',
  "AAABDJJL" varchar(1) NOT NULL default ' ',
  "AAABDJJM" varchar(1) NOT NULL default ' ',
  "AAABDJJN" varchar(1) NOT NULL default ' ',
  "AAABDJJO" varchar(1) NOT NULL default ' ',
  "AAABDJJP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDJMM" varchar(4) NOT NULL default ' ',
  "AAABDJJW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDJMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGLBY" varchar(4) NOT NULL default ' ',
  "AAABDJKE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGLBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABLIQO" varchar(1) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABDJNS","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABDJKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABDJNS","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJLX" varchar(2) NOT NULL default '00',
  "AAABDJLG" varchar(1) NOT NULL default ' ',
  "AAABDJLH" varchar(1) NOT NULL default ' ',
  "AAABLFLE" varchar(1) NOT NULL default ' ',
  "AAABDJLJ" varchar(1) NOT NULL default ' ',
  "AAABDJLK" varchar(1) NOT NULL default ' ',
  "AAABDJLL" varchar(1) NOT NULL default ' ',
  "AAABDJLM" varchar(1) NOT NULL default ' ',
  "AAABDJLN" varchar(1) NOT NULL default ' ',
  "AAABDJLO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJLX" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJLX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJMG" varchar(2) NOT NULL default ' ',
  "AAABDJMH" varchar(3) NOT NULL default '000',
  "AAABDJMI" varchar(2) NOT NULL default ' ',
  "AAABDJME" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABDJMG","AAABDJMH","AAABDJMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIINC" varchar(2) NOT NULL default '00',
  "AAABDJMM" varchar(4) NOT NULL default ' ',
  "AAABDJMN" varchar(1) NOT NULL default ' ',
  "AAABDJMO" varchar(1) NOT NULL default ' ',
  "AAABDJMP" varchar(1) NOT NULL default ' ',
  "AAABDJMQ" varchar(1) NOT NULL default ' ',
  "AAABDJMR" varchar(1) NOT NULL default ' ',
  "AAABDJMS" varchar(1) NOT NULL default ' ',
  "AAABDJMT" varchar(1) NOT NULL default ' ',
  "AAABDJMU" varchar(1) NOT NULL default ' ',
  "AAABDJMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIINC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIINC" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIINC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAABDJNS" varchar(2) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABFSCT" varchar(3) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABDJNN" varchar(5) NOT NULL default ' ',
  "AAABDJNO" varchar(5) NOT NULL default ' ',
  "AAABDJNP" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSDE","AAABDJNS","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSDE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJON" varchar(4) NOT NULL default ' ',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  "AAABIEIT" varchar(2) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABDJOL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJON","AAABIEIX","AAABIEIT","AAABFTNB","AAABMCRJ","AAABMREX","AAABMCVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABDJOY" varchar(1) NOT NULL default ' ',
  "AAABDJOZ" varchar(1) NOT NULL default ' ',
  "AAABDJPA" varchar(1) NOT NULL default ' ',
  "AAABDJPB" varchar(1) NOT NULL default ' ',
  "AAABDJPC" varchar(1) NOT NULL default ' ',
  "AAABDJPD" varchar(1) NOT NULL default ' ',
  "AAABDJPE" varchar(1) NOT NULL default ' ',
  "AAABDJPF" varchar(1) NOT NULL default ' ',
  "AAABDJPG" varchar(1) NOT NULL default ' ',
  "AAABDJPH" varchar(1) NOT NULL default ' ',
  "AAABDJPI" varchar(1) NOT NULL default ' ',
  "AAABDJPJ" varchar(1) NOT NULL default ' ',
  "AAABDJPK" varchar(1) NOT NULL default ' ',
  "AAABDJPL" varchar(1) NOT NULL default ' ',
  "AAABDJPM" varchar(1) NOT NULL default ' ',
  "AAABDJPN" varchar(1) NOT NULL default ' ',
  "AAABDJPO" varchar(1) NOT NULL default ' ',
  "AAABDJPP" varchar(1) NOT NULL default ' ',
  "AAABDJPQ" varchar(1) NOT NULL default ' ',
  "AAABDJPR" varchar(1) NOT NULL default ' ',
  "AAABDJPS" varchar(1) NOT NULL default ' ',
  "AAABDJPT" varchar(1) NOT NULL default ' ',
  "AAABDJPU" varchar(1) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABDJPW" varchar(6) NOT NULL default ' ',
  "AAABDJPX" varchar(1) NOT NULL default ' ',
  "AAABDJPY" varchar(1) NOT NULL default ' ',
  "AAABDJPZ" varchar(1) NOT NULL default ' ',
  "AAABDJQA" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTNB","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABDJQK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCVI","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABDJQU" varchar(4) NOT NULL default ' ',
  "AAABDJQV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMREX","AAABLXUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDJSB" varchar(4) NOT NULL default ' ',
  "AAABDJSC" varchar(4) NOT NULL default ' ',
  "AAABDJSD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMREX","AAABDJSB","AAABDJSC","AAABDJSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABDJRZ" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABDJSB" varchar(4) NOT NULL default ' ',
  "AAABDJSC" varchar(4) NOT NULL default ' ',
  "AAABDJSD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMREX","AAABDJRZ","AAABMDBB","AAABDJSB","AAABDJSC","AAABDJSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABDJSU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABDJSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABDJSU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABDJSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  "AAABDJTA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFUMB","AAABFSFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABDJTR" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABKRXL" varchar(2) NOT NULL default '00',
  "AAABDJTM" varchar(10) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABFUMB","AAABDJTR","AAABMCXV","AAABKRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  "AAABDJTY" varchar(3) NOT NULL default '000',
  "AAABDJTZ" varchar(3) NOT NULL default '000',
  "AAABMCWX" varchar(2) NOT NULL default ' ',
  "AAABJCHR" varchar(1) NOT NULL default ' ',
  "AAABDJWA" varchar(1) NOT NULL default ' ',
  "AAABDJWB" varchar(1) NOT NULL default ' ',
  "AAABDJWC" varchar(1) NOT NULL default ' ',
  "AAABDJWD" varchar(1) NOT NULL default ' ',
  "AAABDJWE" varchar(1) NOT NULL default ' ',
  "AAABDJWF" varchar(1) NOT NULL default ' ',
  "AAABDJUI" varchar(1) NOT NULL default ' ',
  "AAABDJWG" varchar(1) NOT NULL default ' ',
  "AAABDJWH" varchar(1) NOT NULL default ' ',
  "AAABDJUL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMB","AAABFSFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABDJUS" varchar(1) NOT NULL default ' ',
  "AAABKRPZ" varchar(1) NOT NULL default ' ',
  "AAABDJUU" varchar(1) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABDJUW" varchar(6) NOT NULL default ' ',
  "AAABDJUX" varchar(1) NOT NULL default ' ',
  "AAABDJUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABDJVF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFUMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCWX" varchar(2) NOT NULL default ' ',
  "AAABMCQP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHSKS" varchar(2) NOT NULL default '00',
  "AAABJCHR" varchar(1) NOT NULL default ' ',
  "AAABDJWA" varchar(1) NOT NULL default ' ',
  "AAABDJWB" varchar(1) NOT NULL default ' ',
  "AAABDJWC" varchar(1) NOT NULL default ' ',
  "AAABDJWD" varchar(1) NOT NULL default ' ',
  "AAABDJWE" varchar(1) NOT NULL default ' ',
  "AAABDJWF" varchar(1) NOT NULL default ' ',
  "AAABDJWG" varchar(1) NOT NULL default ' ',
  "AAABDJWH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHSKS" varchar(2) NOT NULL default '00',
  "AAABDJWO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJXF" varchar(4) NOT NULL default ' ',
  "AAABDJWV" varchar(1) NOT NULL default ' ',
  "AAABDJWW" varchar(1) NOT NULL default ' ',
  "AAABDJWX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJXF" varchar(4) NOT NULL default ' ',
  "AAABDJXG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJXF","AAABDJXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJXU" varchar(4) NOT NULL default ' ',
  "AAABDJXO" varchar(3) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJXU","AAABDJXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABDJXU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAJY","AAABMREX","AAABMCVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJYP" varchar(3) NOT NULL default ' ',
  "AAABDJYC" varchar(2) NOT NULL default ' ',
  "AAABLUEO" varchar(6) NOT NULL default ' ',
  "AAABHEWX" varchar(1) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDJYP" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(18) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDJYP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKJW" varchar(1) NOT NULL default ' ',
  "AAABDKJX" varchar(1) NOT NULL default ' ',
  "AAABDKJY" varchar(1) NOT NULL default ' ',
  "AAABDKJZ" varchar(1) NOT NULL default ' ',
  "AAABDKKA" varchar(1) NOT NULL default ' ',
  "AAABHYAL" varchar(1) NOT NULL default ' ',
  "AAABDKAI" varchar(1) NOT NULL default ' ',
  "AAABDKAJ" varchar(1) NOT NULL default ' ',
  "AAABDJZD" varchar(1) NOT NULL default ' ',
  "AAABDJZE" varchar(1) NOT NULL default ' ',
  "AAABDJZF" varchar(1) NOT NULL default ' ',
  "AAABDJZG" varchar(1) NOT NULL default ' ',
  "AAABDKAK" varchar(1) NOT NULL default ' ',
  "AAABDKAL" varchar(1) NOT NULL default ' ',
  "AAABDJZJ" varchar(1) NOT NULL default ' ',
  "AAABDJZK" varchar(1) NOT NULL default ' ',
  "AAABDJZL" varchar(1) NOT NULL default ' ',
  "AAABDJZM" varchar(1) NOT NULL default ' ',
  "AAABDJZN" varchar(8) NOT NULL default '00000000',
  "AAABDJZO" varchar(1) NOT NULL default ' ',
  "AAABDJZP" varchar(1) NOT NULL default ' ',
  "AAABDJZQ" varchar(1) NOT NULL default ' ',
  "AAABDJZR" varchar(1) NOT NULL default ' ',
  "AAABDJZS" varchar(1) NOT NULL default ' ',
  "AAABDJZT" varchar(1) NOT NULL default ' ',
  "AAABDJZU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDJZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKKB" varchar(1) NOT NULL default ' ',
  "AAABFMFC" varchar(4) NOT NULL default '0000',
  "AAABFMFD" varchar(4) NOT NULL default '0000',
  "AAABFMFE" varchar(4) NOT NULL default '0000',
  "AAABDKKF" varchar(4) NOT NULL default '0000',
  "AAABDKMS" varchar(1) NOT NULL default ' ',
  "AAABMLEL" varchar(2) NOT NULL default '00',
  "AAABDKAI" varchar(1) NOT NULL default ' ',
  "AAABDKAJ" varchar(1) NOT NULL default ' ',
  "AAABDKAK" varchar(1) NOT NULL default ' ',
  "AAABDKAL" varchar(1) NOT NULL default ' ',
  "AAABDKKI" varchar(10) NOT NULL default ' ',
  "AAABDKAN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDKAV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDKBK" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKKP" varchar(30) NOT NULL default ' ',
  "AAABDKBU" varchar(15) NOT NULL default ' ',
  "AAABLPJD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCFR" varchar(30) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCFR","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKCJ" (
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABDKCM" varchar(30) NOT NULL default ' ',
  "AAABDKCN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIHK","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKDC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDKDC" varchar(10) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDKDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKDD" (
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABDKDG" varchar(30) NOT NULL default ' ',
  "AAABDKDH" varchar(20) NOT NULL default ' ',
  "AAABDKDI" varchar(30) NOT NULL default ' ',
  "AAABDKDJ" varchar(20) NOT NULL default ' ',
  "AAABDKDK" varchar(30) NOT NULL default ' ',
  "AAABDKDL" varchar(20) NOT NULL default ' ',
  "AAABDKDM" varchar(30) NOT NULL default ' ',
  "AAABDKDN" varchar(20) NOT NULL default ' ',
  "AAABDKDO" varchar(30) NOT NULL default ' ',
  "AAABDKDP" varchar(20) NOT NULL default ' ',
  "AAABDKDQ" varchar(30) NOT NULL default ' ',
  "AAABDKDR" varchar(20) NOT NULL default ' ',
  "AAABDKDS" varchar(30) NOT NULL default ' ',
  "AAABDKDT" varchar(20) NOT NULL default ' ',
  "AAABDKDU" varchar(30) NOT NULL default ' ',
  "AAABDKDV" varchar(20) NOT NULL default ' ',
  "AAABDKDW" varchar(30) NOT NULL default ' ',
  "AAABDKDX" varchar(20) NOT NULL default ' ',
  "AAABDKDY" varchar(30) NOT NULL default ' ',
  "AAABDKDZ" varchar(20) NOT NULL default ' ',
  "AAABDKEA" varchar(30) NOT NULL default ' ',
  "AAABDKEB" varchar(1) NOT NULL default ' ',
  "AAABDKEC" varchar(1) NOT NULL default ' ',
  "AAABDKED" varchar(1) NOT NULL default ' ',
  "AAABDKEE" varchar(1) NOT NULL default ' ',
  "AAABDKEF" varchar(1) NOT NULL default ' ',
  "AAABDKEG" varchar(1) NOT NULL default ' ',
  "AAABDKEH" varchar(1) NOT NULL default ' ',
  "AAABDKEI" varchar(1) NOT NULL default ' ',
  "AAABDKEJ" varchar(1) NOT NULL default ' ',
  "AAABDKEK" varchar(1) NOT NULL default ' ',
  "AAABDKEL" varchar(30) NOT NULL default ' ',
  "AAABDKEM" varchar(30) NOT NULL default ' ',
  "AAABDKEN" varchar(30) NOT NULL default ' ',
  "AAABDKEO" varchar(30) NOT NULL default ' ',
  "AAABDKEP" varchar(30) NOT NULL default ' ',
  "AAABDKEQ" varchar(30) NOT NULL default ' ',
  "AAABDKER" varchar(30) NOT NULL default ' ',
  "AAABDKES" varchar(30) NOT NULL default ' ',
  "AAABDKET" varchar(30) NOT NULL default ' ',
  "AAABDKEU" varchar(30) NOT NULL default ' ',
  "AAABDKEV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABEMIH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABEMIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABHBRF" varchar(1) NOT NULL default ' ',
  "AAABDPCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLRBI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLRBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABKQVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKNSA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABKNSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDKGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKHA" varchar(10) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDKHA" varchar(10) NOT NULL default ' ',
  "AAABDKGX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDKHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDKHE" varchar(20) NOT NULL default ' ',
  "AAABDKHF" varchar(1) NOT NULL default ' ',
  "AAABDKHG" int(11) NOT NULL default '0',
  "AAABDKHH" int(11) NOT NULL default '0',
  "AAABDKHI" varchar(1) NOT NULL default ' ',
  "AAABDKHJ" varchar(1) NOT NULL default ' ',
  "AAABDKHK" int(11) NOT NULL default '0',
  "AAABLQSM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDKHU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABDKHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABDKHY" varchar(7) NOT NULL default ' ',
  "AAABDKHZ" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKIC" (
  "AAABLEJI" varchar(40) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAABDKIJ" varchar(2) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABMNLW","AAABDKIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABMDER" varchar(1) NOT NULL default ' ',
  "AAABKQDW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABGIHK","AAABMDER","AAABKQDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABHSKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABHSKS" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX","AAABHSKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKJM" (
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTUX","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  "AAABDKJW" varchar(1) NOT NULL default ' ',
  "AAABDKJX" varchar(1) NOT NULL default ' ',
  "AAABDKJY" varchar(1) NOT NULL default ' ',
  "AAABDKJZ" varchar(1) NOT NULL default ' ',
  "AAABDKKA" varchar(1) NOT NULL default ' ',
  "AAABDKKB" varchar(1) NOT NULL default ' ',
  "AAABFMFC" varchar(4) NOT NULL default '0000',
  "AAABFMFD" varchar(4) NOT NULL default '0000',
  "AAABFMFE" varchar(4) NOT NULL default '0000',
  "AAABDKKF" varchar(4) NOT NULL default '0000',
  "AAABDKKG" varchar(1) NOT NULL default ' ',
  "AAABDKMS" varchar(1) NOT NULL default ' ',
  "AAABDKKI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKKP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGIHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABDKKX" varchar(1) NOT NULL default ' ',
  "AAABDKKY" varchar(1) NOT NULL default ' ',
  "AAABLLWS" varchar(1) NOT NULL default ' ',
  "AAABLUGE" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABGKPS" varchar(10) NOT NULL default ' ',
  "AAABDKZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDKML" varchar(2) NOT NULL default ' ',
  "AAABDKMM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTUX","AAABDKML","AAABDKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDKML" varchar(2) NOT NULL default ' ',
  "AAABDKMM" varchar(2) NOT NULL default ' ',
  "AAABDKMG" varchar(15) NOT NULL default ' ',
  "AAABDKMH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLTUX","AAABDKML","AAABDKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDKMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGIHK" varchar(30) NOT NULL default ' ',
  "AAABDKMW" varchar(30) NOT NULL default ' ',
  "AAABDKMX" varchar(30) NOT NULL default ' ',
  "AAABDKMS" varchar(1) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABGIHK","AAABDKMW","AAABDKMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
