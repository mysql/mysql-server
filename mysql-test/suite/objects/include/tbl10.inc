# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl10.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAHKSR" (
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLRST","AAABMKFH","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKSY" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(50) NOT NULL default ' ',
  "AAABLUMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKTH" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKTO" (
  "AAAAHKTS" varchar(4) NOT NULL default ' ',
  "AAABCPAO" varchar(100) NOT NULL default ' ',
  "AAABMLXR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHKTS","AAABCPAO","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFBF" varchar(5) NOT NULL default '00000',
  "AAABMCMG" varchar(2) NOT NULL default ' ',
  "AAABHFAX" varchar(8) NOT NULL default '00000000',
  "AAAAHKUA" varchar(27) NOT NULL default ' ',
  "AAABMQVX" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABHFBD" varchar(3) NOT NULL default ' ',
  "AAAAQWST" varchar(18) NOT NULL default ' ',
  "AAAAQWSU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFBF","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKUJ" (
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABCPAO" varchar(100) NOT NULL default ' ',
  "AAAAHKUS" varchar(10) NOT NULL default ' ',
  "AAABMECK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABMLQA","AAABCPAO","AAAAHKUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABLDNI" varchar(4) NOT NULL default '0000',
  "AAAAHKVB" varchar(8) NOT NULL default '00000000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAHKVD" varchar(25) NOT NULL default ' ',
  "AAABLOAS" varchar(16) NOT NULL default ' ',
  "AAAAHKVF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNYH","AAABLDNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABLDNI" varchar(4) NOT NULL default '0000',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEPCP" varchar(12) NOT NULL default ' ',
  "AAABEPCC" varchar(12) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHKVZ" varchar(50) NOT NULL default ' ',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABLFYC" varchar(5) NOT NULL default ' ',
  "AAABKGPK" varchar(20) NOT NULL default ' ',
  "AAABJFVF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNYH","AAABLDNI","AAABEPCO","AAABEPCP","AAABEPCC"),
  KEY "AAAAHKWO" ("AAABKGPK","AAABLFYC","AAABJFVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAAAHKXC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAAAHKXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABECAI" varchar(10) NOT NULL default ' ',
  "AAABEPFS" varchar(4) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAAAHKXJ" varchar(18) NOT NULL default ' ',
  "AAAAHKXK" varchar(1) NOT NULL default ' ',
  "AAAAHKXL" varchar(1) NOT NULL default ' ',
  "AAAAHKXM" varchar(1) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAAAHKXO" varchar(20) NOT NULL default ' ',
  "AAABECAM" varchar(12) NOT NULL default ' ',
  "AAABECAN" varchar(8) NOT NULL default '00000000',
  "AAABECAO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABECAW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAHKYI" varchar(1) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABECBP" varchar(12) NOT NULL default ' ',
  "AAABECBQ" varchar(8) NOT NULL default '00000000',
  "AAABECBR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABECCV" varchar(20) NOT NULL default ' ',
  "AAABECCW" varchar(1) NOT NULL default ' ',
  "AAABECCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABECDS" varchar(2) NOT NULL default '00',
  "AAABECDM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA","AAABECDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAAAHLBA" varchar(2) NOT NULL default '00',
  "AAAAHLAT" varchar(20) NOT NULL default ' ',
  "AAAAHLAU" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA","AAAAHLBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default '000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABLEPD" varchar(10) NOT NULL default ' ',
  "AAABLEPE" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA","AAABLEPD","AAABLEPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAAAQHTM" varchar(2) NOT NULL default '00',
  "AAABMDBA" varchar(3) NOT NULL default ' ',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAHLCN" varchar(1) NOT NULL default ' ',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABKPXV","AAAAQHTM","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAAAQHTM" varchar(2) NOT NULL default '00',
  "AAAAZWEQ" varchar(1) NOT NULL default ' ',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAHLDE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHLDF" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABKPXV","AAAAQHTM","AAAAZWEQ","AAAAZWEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAAAHLEK" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  "AAAAQHHQ" varchar(1) NOT NULL default ' ',
  "AAAAQHHR" varchar(1) NOT NULL default ' ',
  "AAAAQHHT" varchar(1) NOT NULL default ' ',
  "AAAAQHHS" varchar(1) NOT NULL default ' ',
  "AAAAQHHU" varchar(1) NOT NULL default ' ',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABLRBI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABKPXV","AAAAHLEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHLFC" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAAAHLFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAIEOT" varchar(4) NOT NULL default ' ',
  "AAAAIEMB" decimal(15,2) NOT NULL default '0.00',
  "AAAAIEMC" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAIEOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMREX","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAIEOT" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMREX","AAABMDBB","AAABMCSO","AAAAIEOT","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLGV" (
  "AAAAORWM" varchar(8) NOT NULL default '00000000',
  "AAABEPWA" varchar(10) NOT NULL default '0000000000',
  "AAAAHLMW" varchar(8) NOT NULL default '00000000',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  "AAAAHLMM" varchar(15) NOT NULL default ' ',
  "AAAAHLHC" varchar(15) NOT NULL default ' ',
  "AAAAHLHD" varchar(10) NOT NULL default '0000000000',
  "AAABKNCK" varchar(1) NOT NULL default ' ',
  "AAAAHLHF" varchar(1) NOT NULL default ' ',
  "AAABAOBD" varchar(1) NOT NULL default ' ',
  "AAAAHLHH" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAORWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAWSMO" varchar(2) NOT NULL default ' ',
  "AAABIZWM" varchar(10) NOT NULL default ' ',
  "AAAAHLJV" varchar(18) NOT NULL default ' ',
  "AAAAHLJX" varchar(30) NOT NULL default ' ',
  "AAAAHLJR" varchar(30) NOT NULL default ' ',
  "AAAAHLJT" varchar(15) NOT NULL default ' ',
  "AAAAUWPZ" varchar(8) NOT NULL default '00000000',
  "AAAAHLHU" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLHV" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLHW" decimal(13,3) NOT NULL default '0.000',
  "AAABLEVG" varchar(3) NOT NULL default ' ',
  "AAAAHLHY" varchar(1) NOT NULL default ' ',
  "AAAAHLHZ" varchar(8) NOT NULL default '00000000',
  "AAAAHLIA" varchar(22) NOT NULL default ' ',
  "AAAAHLIB" varchar(22) NOT NULL default ' ',
  "AAAAHLIC" varchar(2) NOT NULL default '00',
  "AAAAHLID" varchar(4) NOT NULL default '0000',
  "AAAAHLIE" varchar(30) NOT NULL default ' ',
  "AAAAHLIF" varchar(20) NOT NULL default ' ',
  "AAAAWSUD" varchar(1) NOT NULL default ' ',
  "AAAAWSJO" varchar(10) NOT NULL default ' ',
  "AAAAHLII" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIJ" varchar(3) NOT NULL default ' ',
  "AAAAHLIK" varchar(7) NOT NULL default '0000000',
  "AAAAHLIL" varchar(2) NOT NULL default '00',
  "AAAAHLIM" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIN" varchar(3) NOT NULL default ' ',
  "AAAAHLIO" varchar(30) NOT NULL default ' ',
  "AAAAHLIP" varchar(12) NOT NULL default ' ',
  "AAAAHLIQ" varchar(12) NOT NULL default ' ',
  "AAAAHLIR" varchar(12) NOT NULL default ' ',
  "AAAAHLIS" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIT" decimal(13,3) NOT NULL default '0.000',
  "AAABLEVJ" varchar(3) NOT NULL default ' ',
  "AAAAHLIV" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIW" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIX" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLIY" varchar(3) NOT NULL default ' ',
  "AAAAHLIZ" decimal(13,3) NOT NULL default '0.000',
  "AAAAIGQY" varchar(3) NOT NULL default ' ',
  "AAAAHLJB" varchar(2) NOT NULL default '00',
  "AAAAHLJC" decimal(9,3) NOT NULL default '0.000',
  "AAAATIWL" varchar(3) NOT NULL default ' ',
  "AAAAHLJE" varchar(1) NOT NULL default ' ',
  "AAAAHLJF" varchar(1) NOT NULL default ' ',
  "AAAAHLJG" varchar(1) NOT NULL default ' ',
  "AAAAHLJH" decimal(13,3) NOT NULL default '0.000',
  "AAAAHLJI" varchar(3) NOT NULL default ' ',
  "AAAAHLJJ" decimal(7,2) NOT NULL default '0.00',
  "AAAAHLJK" varchar(3) NOT NULL default ' ',
  "AAABLXMZ" varchar(8) NOT NULL default '00000000',
  "AAAAHLJM" varchar(10) NOT NULL default ' ',
  "AAAAHLJN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  KEY "AAAAHLJQ" ("AAAAHLJR"),
  KEY "AAAAHLJS" ("AAAAHLJT"),
  KEY "AAAAHLJU" ("AAAAHLJV"),
  KEY "AAAAHLJW" ("AAAAHLJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLJY" (
  "AAABEPWA" varchar(10) NOT NULL default '0000000000',
  "AAABKZAY" varchar(15) NOT NULL default ' ',
  "AAABKAFO" varchar(15) NOT NULL default ' ',
  "AAAATHRC" varchar(10) NOT NULL default '0000000000',
  "AAAAOTJH" varchar(20) NOT NULL default ' ',
  "AAABLYNE" varchar(7) NOT NULL default ' ',
  "AAABAXPS" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLKH" (
  "AAABLUSV" varchar(5) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAAAHLKW" varchar(3) NOT NULL default '000',
  "AAABIYEN" varchar(15) NOT NULL default ' ',
  "AAABIYEO" varchar(15) NOT NULL default ' ',
  "AAAAHLKN" varchar(1) NOT NULL default ' ',
  "AAAAHLKO" varchar(1) NOT NULL default ' ',
  "AAAAHLKP" varchar(1) NOT NULL default ' ',
  "AAAAHLKQ" varchar(1) NOT NULL default ' ',
  "AAAAHLKR" varchar(15) NOT NULL default ' ',
  "AAABLGEF" varchar(20) NOT NULL default ' ',
  "AAAAHLKT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIZCH","AAAAHLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLKX" (
  "AAABLUSV" varchar(5) NOT NULL default ' ',
  "AAAAHLKZ" varchar(15) NOT NULL default ' ',
  "AAAAOTZU" varchar(15) NOT NULL default ' ',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAAAIVLR" int(11) NOT NULL default '0',
  "AAABMMRH" varchar(1) NOT NULL default ' ',
  "AAAAHLLE" varchar(1) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABJABF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLLI" (
  "AAABLUSV" varchar(5) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABFEIS" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUSV","AAABIZCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLLO" (
  "AAABLUSV" varchar(5) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAAAHLLR" varchar(15) NOT NULL default ' ',
  "AAAAHLLS" varchar(15) NOT NULL default ' ',
  "AAAAHLLT" varchar(15) NOT NULL default ' ',
  "AAAAHLLU" varchar(15) NOT NULL default ' ',
  "AAAAHLLV" varchar(15) NOT NULL default ' ',
  "AAAAHLLW" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV","AAABIZCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLLZ" (
  "AAAAHLMI" varchar(5) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABLTWH" varchar(10) NOT NULL default ' ',
  "AAABINFJ" varchar(4) NOT NULL default '0000',
  "AAAAPOSO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAHLMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLMG" (
  "AAAAHLMW" varchar(8) NOT NULL default '00000000',
  "AAAAHLMI" varchar(5) NOT NULL default ' ',
  "AAABIZCH" varchar(8) NOT NULL default '00000000',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAAAHLMM" varchar(15) NOT NULL default ' ',
  "AAAAHLMN" varchar(15) NOT NULL default ' ',
  "AAAAHLMO" varchar(2) NOT NULL default ' ',
  "AAAAHLMP" varchar(5) NOT NULL default '00000',
  "AAABBNZI" varchar(5) NOT NULL default '00000',
  "AAAAHLMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAHLMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLMT" (
  "AAAAHLMW" varchar(8) NOT NULL default '00000000',
  "AAABEPWA" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAAHLMW","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAHLND" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHLOR" varchar(10) NOT NULL default ' ',
  "AAAAHLOS" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMREX","AAAAHLOR","AAAAHLOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAHLOM" varchar(16) NOT NULL default ' ',
  "AAAAHLON" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHLOR" varchar(10) NOT NULL default ' ',
  "AAAAHLOS" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAHLOM" varchar(16) NOT NULL default ' ',
  "AAAAHLON" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABMREX","AAAAHLOR","AAAAHLOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHLOV" varchar(1) NOT NULL default ' ',
  "AAAAHLOW" varchar(1) NOT NULL default ' ',
  "AAAAHLOX" varchar(16) NOT NULL default ' ',
  "AAAAHLOY" varchar(16) NOT NULL default ' ',
  "AAAAHLOZ" varchar(1) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAAAHLPB" varchar(1) NOT NULL default ' ',
  "AAAAHLPC" varchar(1) NOT NULL default ' ',
  "AAAAHLPD" varchar(30) NOT NULL default ' ',
  "AAAAHLPE" varchar(30) NOT NULL default ' ',
  "AAAAHLPF" varchar(30) NOT NULL default ' ',
  "AAAAHLPG" varchar(30) NOT NULL default ' ',
  "AAAAHLPH" varchar(30) NOT NULL default ' ',
  "AAAAHLPI" int(11) NOT NULL default '0',
  "AAAAHLPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAAAHLSG" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHLPX" varchar(16) NOT NULL default ' ',
  "AAAAHLPY" varchar(16) NOT NULL default ' ',
  "AAAAHLPZ" varchar(16) NOT NULL default ' ',
  "AAAAHLQA" varchar(16) NOT NULL default ' ',
  "AAAAHLQB" varchar(1) NOT NULL default ' ',
  "AAAAQVVY" varchar(1) NOT NULL default ' ',
  "AAAAHLQD" varchar(1) NOT NULL default ' ',
  "AAAAHLTE" varchar(30) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHMDF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHMDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHLSG" varchar(14) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHLSG","AAAAHMBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHLSG" varchar(14) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAAAHLRS" varchar(30) NOT NULL default ' ',
  "AAAAHLRT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHLSG","AAAAHMBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHLSG" varchar(14) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(5) NOT NULL default '00000',
  "AAAAHMBG" varchar(14) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHLSG","AAAAHMBU","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAAAHLSO" varchar(12) NOT NULL default ' ',
  "AAABMCNE" varchar(40) NOT NULL default ' ',
  "AAABMCNJ" varchar(1) NOT NULL default ' ',
  "AAAAHLSR" varchar(8) NOT NULL default '00000000',
  "AAAAHLSS" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLZDI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHLTD" varchar(30) NOT NULL default ' ',
  "AAAAHLTE" varchar(30) NOT NULL default ' ',
  "AAAAHLTF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHLWP" varchar(1) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABJRIY" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMDBA","AAABMDBB","AAABMPWO","AAABMREX","AAABMCPV","AAAAHMDG","AAAAHMBU"),
  KEY "AAAAHLUO" ("AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHLWP" varchar(1) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABJRIY" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMDBA","AAABMDBB","AAABMPWO","AAABMREX","AAABMCPV","AAAAHMDG","AAAAHMBU"),
  KEY "AAAAHLVV" ("AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAAHLWP" varchar(1) NOT NULL default ' ',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMDBA","AAABMDBB","AAABMPWO","AAABMREX","AAABMCPV","AAAAHMDG"),
  KEY "AAAAHLWZ" ("AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABJRIY" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABMDBA","AAABMPWO"),
  KEY "AAAAHLXT" ("AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABMCVR","AAABMLEK","AAABLTXE","AAABLTXF"),
  KEY "AAAAHLYW" ("AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAAAMTOD" varchar(30) NOT NULL default ' ',
  "AAAAHLZI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHMDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(10) NOT NULL default ' ',
  "AAAAHVLS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAHMDG","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHLZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMBG" varchar(14) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLZDI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHMBG","AAAAHMBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMBG" varchar(14) NOT NULL default ' ',
  "AAAAHMBT" varchar(30) NOT NULL default ' ',
  "AAAAHMAU" varchar(3) NOT NULL default '000',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHMAN" varchar(45) NOT NULL default ' ',
  "AAAAHMAO" varchar(45) NOT NULL default ' ',
  "AAAAHMAP" varchar(1) NOT NULL default ' ',
  "AAAAHMAQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHMBG","AAAAHMBT","AAAAHMAU","AAAAHMBU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHMBG" varchar(14) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAAAHMBC" varchar(30) NOT NULL default ' ',
  "AAAAHMBD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHMBG","AAAAHMBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMBI" (
  "AAAAHMBT" varchar(30) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHMBT","AAAAHMBU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMBP" (
  "AAAAHMBT" varchar(30) NOT NULL default ' ',
  "AAAAHMBU" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHMBT","AAAAHMBU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMBW" (
  "AAAAHMBY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHMBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAAAHMDF" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCQA" decimal(15,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(15,2) NOT NULL default '0.00',
  "AAABJRIY" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHMDG","AAABKRXN","AAAAHMDJ","AAABENPM","AAAAHMDF","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMDF" varchar(1) NOT NULL default ' ',
  "AAAAHMDG" varchar(16) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHMDJ" varchar(8) NOT NULL default '00000000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAHMDF","AAAAHMDG","AAABMREX","AAABMCPV","AAAAHMDJ","AAABENPM","AAABMCRE","AAABKRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAHMDV" varchar(14) NOT NULL default ' ',
  "AAAAHMDW" varchar(14) NOT NULL default ' ',
  "AAAAHMGD" varchar(16) NOT NULL default ' ',
  "AAAAHMDY" varchar(1) NOT NULL default ' ',
  "AAAAHMDZ" varchar(1) NOT NULL default ' ',
  "AAAAHMEA" varchar(4) NOT NULL default '0000',
  "AAAAHMEB" varchar(1) NOT NULL default ' ',
  "AAAAHMEC" varchar(4) NOT NULL default '0000',
  "AAAAHMED" varchar(1) NOT NULL default ' ',
  "AAAAHMEE" varchar(4) NOT NULL default '0000',
  "AAAAHMEF" varchar(1) NOT NULL default ' ',
  "AAAAHMEG" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAHMEK" varchar(1) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAAAHMEM" varchar(6) NOT NULL default ' ',
  "AAAAHMEN" varchar(6) NOT NULL default ' ',
  "AAAAHMEO" varchar(2) NOT NULL default ' ',
  "AAAAHMEP" varchar(2) NOT NULL default ' ',
  "AAAAHMEQ" varchar(1) NOT NULL default ' ',
  "AAAAHMER" varchar(2) NOT NULL default ' ',
  "AAAAHMES" varchar(6) NOT NULL default ' ',
  "AAAAHMET" varchar(8) NOT NULL default ' ',
  "AAAAHMEU" varchar(1) NOT NULL default ' ',
  "AAAAHMEV" varchar(1) NOT NULL default ' ',
  "AAABLMLR" varchar(1) NOT NULL default '0',
  "AAAAHMEX" varchar(1) NOT NULL default ' ',
  "AAAAHMEY" varchar(1) NOT NULL default ' ',
  "AAAAHMEZ" varchar(1) NOT NULL default ' ',
  "AAAAHMFA" varchar(6) NOT NULL default ' ',
  "AAAAHMFB" varchar(6) NOT NULL default ' ',
  "AAAAHMFC" varchar(1) NOT NULL default ' ',
  "AAAAHMFD" varchar(8) NOT NULL default '00000000',
  "AAAAHMFE" varchar(2) NOT NULL default ' ',
  "AAAAHMFF" varchar(2) NOT NULL default ' ',
  "AAAAHMFG" varchar(2) NOT NULL default ' ',
  "AAAAHMFH" varchar(1) NOT NULL default '0',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAABDQKJ" varchar(1) NOT NULL default ' ',
  "AAABGRYN" varchar(4) NOT NULL default ' ',
  "AAAAHMFL" varchar(2) NOT NULL default ' ',
  "AAAAHMFM" varchar(24) NOT NULL default ' ',
  "AAAAHMFN" varchar(24) NOT NULL default ' ',
  "AAAAHMFO" varchar(1) NOT NULL default ' ',
  "AAAAHMFP" varchar(1) NOT NULL default ' ',
  "AAAAHMFQ" varchar(4) NOT NULL default ' ',
  "AAAAHMFR" varchar(4) NOT NULL default ' ',
  "AAAAHMFS" varchar(4) NOT NULL default ' ',
  "AAAAHMFT" varchar(4) NOT NULL default ' ',
  "AAAAHMFU" varchar(1) NOT NULL default ' ',
  "AAAAHMFV" varchar(1) NOT NULL default ' ',
  "AAAAHMFW" varchar(8) NOT NULL default '00000000',
  "AAAAHMFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHMGD" varchar(16) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHMGF" varchar(14) NOT NULL default ' ',
  "AAAAHMGG" varchar(14) NOT NULL default ' ',
  "AAAAHMGT" varchar(12) NOT NULL default ' ',
  "AAAAHMGU" varchar(8) NOT NULL default '00000000',
  "AAAAHMPI" varchar(4) NOT NULL default '0000',
  "AAAAHMPJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHMGQ" varchar(14) NOT NULL default ' ',
  "AAAAHMGR" varchar(4) NOT NULL default '0000',
  "AAAAHMGS" varchar(3) NOT NULL default '000',
  "AAAAHMGT" varchar(12) NOT NULL default ' ',
  "AAAAHMGU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMHB" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHYVH" varchar(4) NOT NULL default ' ',
  "AAAAHMHI" varchar(2) NOT NULL default '00',
  "AAAAHMHJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAHYVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHMHP" varchar(4) NOT NULL default '0000',
  "AAAAHMHQ" varchar(4) NOT NULL default '0000',
  "AAAAHMHR" varchar(4) NOT NULL default '0000',
  "AAAAHMHS" varchar(4) NOT NULL default '0000',
  "AAAAHMHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHMHZ" varchar(1) NOT NULL default ' ',
  "AAAAHMIA" varchar(1) NOT NULL default ' ',
  "AAAAHMIB" varchar(1) NOT NULL default ' ',
  "AAAAHMIC" varchar(1) NOT NULL default ' ',
  "AAAAHMID" varchar(1) NOT NULL default ' ',
  "AAAAHMKC" varchar(1) NOT NULL default ' ',
  "AAAAHMIF" varchar(1) NOT NULL default ' ',
  "AAAAHMIG" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABIHEH" varchar(1) NOT NULL default ' ',
  "AAAAHMIN" varchar(4) NOT NULL default '0000',
  "AAAAHMIO" varchar(4) NOT NULL default '0000',
  "AAAAHMIP" varchar(1) NOT NULL default ' ',
  "AAAAHMIQ" varchar(1) NOT NULL default ' ',
  "AAAAHMIR" varchar(1) NOT NULL default ' ',
  "AAAAHMIS" varchar(1) NOT NULL default ' ',
  "AAAAHMIT" varchar(1) NOT NULL default ' ',
  "AAAAHPXO" varchar(1) NOT NULL default ' ',
  "AAAAHMIV" varchar(1) NOT NULL default ' ',
  "AAAAHMIW" varchar(1) NOT NULL default ' ',
  "AAAAHMIX" varchar(1) NOT NULL default ' ',
  "AAAAHMIY" varchar(1) NOT NULL default ' ',
  "AAAAHMIZ" varchar(1) NOT NULL default ' ',
  "AAAAHMJA" varchar(1) NOT NULL default ' ',
  "AAAAHMJB" varchar(1) NOT NULL default ' ',
  "AAAAHMJC" varchar(1) NOT NULL default ' ',
  "AAAAHMJD" varchar(1) NOT NULL default ' ',
  "AAAAHMJE" varchar(1) NOT NULL default ' ',
  "AAAAHMJF" varchar(1) NOT NULL default ' ',
  "AAAAHMJG" varchar(1) NOT NULL default ' ',
  "AAAAHMJH" varchar(1) NOT NULL default ' ',
  "AAAAHMJI" varchar(1) NOT NULL default ' ',
  "AAAAHMJJ" varchar(16) NOT NULL default ' ',
  "AAAAHMJK" varchar(24) NOT NULL default ' ',
  "AAAAHMJL" varchar(2) NOT NULL default '00',
  "AAAAHMJM" varchar(4) NOT NULL default '0000',
  "AAAAHMJN" varchar(2) NOT NULL default '00',
  "AAAAHMJO" varchar(20) NOT NULL default ' ',
  "AAAAHMJP" varchar(10) NOT NULL default ' ',
  "AAAAHMJQ" varchar(10) NOT NULL default ' ',
  "AAAAHMJR" varchar(2) NOT NULL default '00',
  "AAAAHMJS" varchar(1) NOT NULL default ' ',
  "AAAAHMJT" varchar(1) NOT NULL default ' ',
  "AAAAHMJU" varchar(1) NOT NULL default ' ',
  "AAAAHMJV" varchar(1) NOT NULL default ' ',
  "AAAAHMJW" varchar(1) NOT NULL default ' ',
  "AAAAHMJX" varchar(1) NOT NULL default ' ',
  "AAAAHMJY" varchar(1) NOT NULL default ' ',
  "AAAAHMJZ" varchar(1) NOT NULL default ' ',
  "AAAAHMKA" varchar(2) NOT NULL default ' ',
  "AAAAHMKB" varchar(1) NOT NULL default ' ',
  "AAAAHMKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABIHEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAPYHA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABLOLZ","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAHMKZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMLG" varchar(1) NOT NULL default ' ',
  "AAAAHMLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAAAHVNT" varchar(2) NOT NULL default '00',
  "AAAAHMMH" varchar(2) NOT NULL default '00',
  "AAAAHVNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABJLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAAAHVPJ" varchar(5) NOT NULL default ' ',
  "AAAAHVPK" varchar(20) NOT NULL default ' ',
  "AAAAHVPL" varchar(30) NOT NULL default ' ',
  "AAAAHVNT" varchar(2) NOT NULL default '00',
  "AAAAHMMH" varchar(2) NOT NULL default '00',
  "AAAAHVNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAABJLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAAAHMMT" varchar(2) NOT NULL default '00',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAAAHVPJ" varchar(5) NOT NULL default ' ',
  "AAAAHVPK" varchar(20) NOT NULL default ' ',
  "AAAAHVPL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAABJLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMNR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMNV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHWPE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABCPTJ" varchar(6) NOT NULL default ' ',
  "AAAAHMOI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMOQ" varchar(1) NOT NULL default ' ',
  "AAAAHMOR" varchar(1) NOT NULL default ' ',
  "AAAAHMOS" varchar(1) NOT NULL default ' ',
  "AAAAHMOT" varchar(1) NOT NULL default ' ',
  "AAAAIAKN" varchar(1) NOT NULL default ' ',
  "AAAAIAKA" varchar(1) NOT NULL default ' ',
  "AAAAIAKB" varchar(1) NOT NULL default ' ',
  "AAAAHMOX" varchar(1) NOT NULL default ' ',
  "AAAAHMOY" varchar(1) NOT NULL default ' ',
  "AAAAHMOZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABIHEH" varchar(1) NOT NULL default ' ',
  "AAAAHMPN" varchar(2) NOT NULL default ' ',
  "AAAAHMPI" varchar(4) NOT NULL default '0000',
  "AAAAHMPJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABIHEH","AAAAHMPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAANAVQ","AAABMCXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMQJ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAYVHW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAWMKU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHMRB" varchar(1) NOT NULL default ' ',
  "AAAAHMRC" varchar(1) NOT NULL default ' ',
  "AAAAHMRD" varchar(1) NOT NULL default ' ',
  "AAAAHMRE" varchar(1) NOT NULL default ' ',
  "AAAAHMRF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKRXK","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICNF" varchar(1) NOT NULL default ' ',
  "AAAAICLO" varchar(1) NOT NULL default ' ',
  "AAAAICNG" varchar(1) NOT NULL default ' ',
  "AAAAHMRR" varchar(1) NOT NULL default ' ',
  "AAAAICLP" varchar(1) NOT NULL default ' ',
  "AAAAHMRT" varchar(1) NOT NULL default ' ',
  "AAAAICLR" varchar(1) NOT NULL default ' ',
  "AAAAICLS" varchar(1) NOT NULL default ' ',
  "AAAAICLT" varchar(1) NOT NULL default ' ',
  "AAAAICLU" varchar(1) NOT NULL default ' ',
  "AAAAICLV" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLX" decimal(5,2) NOT NULL default '0.00',
  "AAAAHMSB" varchar(1) NOT NULL default ' ',
  "AAAAHMSC" decimal(5,2) NOT NULL default '0.00',
  "AAAAHMSD" decimal(15,2) NOT NULL default '0.00',
  "AAAAHMSE" varchar(1) NOT NULL default ' ',
  "AAAAHMSF" decimal(5,2) NOT NULL default '0.00',
  "AAAAHMSG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHMSH" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMG" varchar(24) NOT NULL default ' ',
  "AAAAICMH" varchar(16) NOT NULL default ' ',
  "AAAAICMI" varchar(10) NOT NULL default ' ',
  "AAAAICMJ" varchar(16) NOT NULL default ' ',
  "AAAAICML" varchar(1) NOT NULL default ' ',
  "AAAAICMM" varchar(1) NOT NULL default ' ',
  "AAAAICMN" varchar(1) NOT NULL default ' ',
  "AAAAICMO" varchar(1) NOT NULL default ' ',
  "AAAAICMP" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMR" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMS" varchar(1) NOT NULL default ' ',
  "AAAAICMT" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMU" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMV" varchar(1) NOT NULL default ' ',
  "AAAAICMW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMX" decimal(15,2) NOT NULL default '0.00',
  "AAAAICNA" varchar(24) NOT NULL default ' ',
  "AAAAICNB" varchar(16) NOT NULL default ' ',
  "AAAAICNC" varchar(10) NOT NULL default ' ',
  "AAAAICNE" varchar(16) NOT NULL default ' ',
  "AAAAICLM" varchar(10) NOT NULL default ' ',
  "AAAAICLN" varchar(10) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMTQ" varchar(20) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHMTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHMTV" varchar(1) NOT NULL default ' ',
  "AAAAHMTW" varchar(1) NOT NULL default '0',
  "AAAAHMTX" varchar(24) NOT NULL default ' ',
  "AAAAHMTY" varchar(16) NOT NULL default ' ',
  "AAAAHMTZ" varchar(10) NOT NULL default ' ',
  "AAAAHMUA" varchar(24) NOT NULL default ' ',
  "AAAAHMUB" varchar(16) NOT NULL default ' ',
  "AAAAHMUC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWCJG","AAABMCNU","AAABMDBB","AAAAICIH","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHMUW" varchar(11) NOT NULL default ' ',
  "AAAAHMUX" varchar(11) NOT NULL default ' ',
  "AAAAHMUY" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMRH" varchar(3) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAAAHMVF" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAIBQU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAIBQT","AAAAIBQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAIBQU" varchar(12) NOT NULL default ' ',
  "AAAAHMWA" varchar(50) NOT NULL default ' ',
  "AAAAHMWB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAIBQT","AAAAIBQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAAHZIY" varchar(10) NOT NULL default ' ',
  "AAAAZQWD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMLEQ","AAAAHZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHMWW" varchar(10) NOT NULL default ' ',
  "AAAAHMWX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOAH","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMXB" (
  "AAAAHMXK" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAHMXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMXE" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHMXK" varchar(3) NOT NULL default '000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAHMXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMXL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAHMXP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVNM" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVNM","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHMYC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAABMMJT" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAIVRT","AAAAIEJA","AAAAICIH","AAAAIEGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHMZN" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHWLG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAIBEA" varchar(5) NOT NULL default ' ',
  "AAAAIBEB" varchar(5) NOT NULL default ' ',
  "AAAAIBEC" varchar(5) NOT NULL default ' ',
  "AAAAIBED" varchar(5) NOT NULL default ' ',
  "AAAAIBEE" varchar(5) NOT NULL default ' ',
  "AAAAIBEF" varchar(5) NOT NULL default ' ',
  "AAAAIBEG" varchar(5) NOT NULL default ' ',
  "AAAAIBEH" varchar(5) NOT NULL default ' ',
  "AAAAIBEI" varchar(5) NOT NULL default ' ',
  "AAAAIBEJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHMZN","AAABMDBB","AAAAHWLG","AAABIHLK","AAABMCZG","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHMZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNAA" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAHNOV" varchar(16) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAIUKR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
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
  UNIQUE KEY "AAAAHNBT" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAAAHOFP","AAABMQHV","AAABMQCO"),
  KEY "AAAAHNCB" ("AAABMQHV","AAAAHOFP","AAABMQHY","AAABMPVB"),
  KEY "AAAAHNCG" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAAAHOFP","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNCO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAIUBZ" varchar(1) NOT NULL default ' ',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAIUBZ","AAAAIUDO","AAAAHZID","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNDD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAIUBZ" varchar(1) NOT NULL default ' ',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  "AAAAHNDJ" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAIUBV" varchar(1) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAIUBZ","AAAAIUDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAIUKR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHNEB" ("AAABMRIL","AAAAIUKP","AAAAIUKQ","AAAAIUKR","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  "AAAAIUCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAIUDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNER" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAHNFF" varchar(2) NOT NULL default '00',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  "AAAAIUDH" varchar(32) NOT NULL default ' ',
  "AAAAIUDI" varchar(32) NOT NULL default ' ',
  "AAAAHNEZ" varchar(1) NOT NULL default ' ',
  "AAAAIUDJ" varchar(32) NOT NULL default ' ',
  "AAAAIUDK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAHNFF","AAAAIUDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNFH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAHNFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIWOQ" varchar(16) NOT NULL default ' ',
  "AAAAIWOR" varchar(24) NOT NULL default ' ',
  "AAABANNN" varchar(16) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAHNGR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHNGH" ("AAABMRIL","AAABMCNU","AAABKJYV","AAAAIWOQ","AAAAIWOR","AAABANNN","AAAAIUQC","AAABKRUO","AAAAJVGP","AAAAHNGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNGS" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAHNOV" varchar(16) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAIUKR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAHNKH" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAAAHOFP","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAHNKQ" ("AAABMQHV","AAAAHOFP","AAABMQHY"),
  KEY "AAAAHNKU" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAAAHOFP","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNLC" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAHNOV" varchar(16) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAIUKR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAHNOM" ("AAABMQHU","AAAAHOFP","AAABMQHY","AAABMQHV","AAABMQHW"),
  KEY "AAAAHNOS" ("AAABMQHU","AAAAHOFP","AAAAHNOV","AAAAIWGG","AAAAHOFR","AAAAHOFS","AAAAHOFT","AAAAHXNT","AAAAIWGC","AAAAHXNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNPD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAIUDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNPM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUDW" varchar(2) NOT NULL default ' ',
  "AAAAIUDO" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUDW","AAAAIUDO","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNPX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAHNQQ" varchar(2) NOT NULL default ' ',
  "AAAAHNQR" varchar(2) NOT NULL default ' ',
  "AAAAHNQS" varchar(4) NOT NULL default '0000',
  "AAAAIUPG" varchar(22) NOT NULL default ' ',
  "AAAAHNQF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIUPD","AAAAHNQQ","AAAAHNQR","AAAAHNQS"),
  KEY "AAAAHNQM" ("AAABLUXQ","AAABCPQF","AAAAIUPG","AAAAHNQQ","AAAAHNQR","AAAAHNQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNQT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIWOQ" varchar(16) NOT NULL default ' ',
  "AAAAIWOR" varchar(24) NOT NULL default ' ',
  "AAABANNN" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLXTG"),
  UNIQUE KEY "AAAAHNRI" ("AAABLUXQ","AAABKJYV","AAAAIWOQ","AAAAIWOR","AAABANNN","AAABKRUO","AAAAJVGP","AAAAIUQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNRR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHNSC" varchar(4) NOT NULL default '0000',
  "AAAAHNSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNRY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHNSC" varchar(4) NOT NULL default '0000',
  "AAAAHNSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNSH" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAHOCO" varchar(2) NOT NULL default ' ',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAHOCQ" varchar(4) NOT NULL default ' ',
  "AAAAHOCR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
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
  "AAAAHOET" varchar(1) NOT NULL default '0',
  "AAAAHOAJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHNUE" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAAAHOFP","AAABMQHV","AAABMQCO"),
  KEY "AAAAHNUM" ("AAABMQHV","AAAAHOFP","AAABMQHY","AAABMPVB"),
  KEY "AAAAHNUR" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAAAHOFP","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAHOCO" varchar(2) NOT NULL default ' ',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAHOCQ" varchar(4) NOT NULL default ' ',
  "AAAAHOCR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHNVK" ("AAABMRIL","AAAAIUKP","AAAAHOCO","AAAAIUKQ","AAAAHOCQ","AAAAHOCR","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIWOQ" varchar(16) NOT NULL default ' ',
  "AAAAIWOR" varchar(24) NOT NULL default ' ',
  "AAABANNN" varchar(16) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHNWJ" ("AAABMCNU","AAABKJYV","AAAAIWOQ","AAAAIWOR","AAABANNN","AAAAIUQC","AAABMRIL","AAABKRUO","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHNWT" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAHOCO" varchar(2) NOT NULL default ' ',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAHOCQ" varchar(4) NOT NULL default ' ',
  "AAAAHOCR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
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
  "AAAAHOET" varchar(1) NOT NULL default '0',
  "AAAAHOAJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHOAM" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAAAHOFP","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAHOAV" ("AAABMQHU","AAAAHOFP","AAAAIWGG","AAAAHOFR","AAAAHOFS","AAAAHOFT","AAAAIWGC","AAAAHXNV","AAAAHXNT"),
  KEY "AAAAHOBF" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAAAHOFP","AAABMQHV","AAABMQDA"),
  KEY "AAAAHOBN" ("AAABMQHU","AAAAHOFP","AAAAIWGG","AAABMQHY","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOBT" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAAAHOFP" varchar(4) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAHOFR" varchar(16) NOT NULL default ' ',
  "AAAAHOFS" varchar(24) NOT NULL default ' ',
  "AAAAHOFT" varchar(16) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAIUKP" varchar(4) NOT NULL default '0000',
  "AAAAHOCO" varchar(2) NOT NULL default ' ',
  "AAAAIUKQ" varchar(1) NOT NULL default ' ',
  "AAAAHOCQ" varchar(4) NOT NULL default ' ',
  "AAAAHOCR" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAAAHOET" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAHOFF" ("AAABMQHU","AAAAHOFP","AAABMQHV","AAABMQHX","AAABMQHY","AAAAHOFR","AAAAHOFS"),
  KEY "AAAAHOFN" ("AAABMQHU","AAAAHOFP","AAAAIWGG","AAAAHOFR","AAAAHOFS","AAAAHOFT","AAAAIWGC","AAAAHXNV","AAAAHXNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHOGJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGR" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGS" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGT" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGV" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGW" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGX" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGY" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOGZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHA" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHC" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHD" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHF" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHH" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHR" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHS" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHT" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHV" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHW" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHX" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHY" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOHZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIA" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIC" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOID" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIF" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIH" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOII" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIR" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIS" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIT" decimal(15,2) NOT NULL default '0.00',
  "AAAAHOIU" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXO","AAABKRXF","AAABLOLZ","AAABIHLK","AAABKRXN","AAABMREX","AAABMCPV"),
  KEY "AAAAHOJG" ("AAABIHLK","AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABJKYU" decimal(13,2) NOT NULL default '0.00',
  "AAAAHOKH" decimal(13,2) NOT NULL default '0.00',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAAALBXS" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAVYWC" varchar(12) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMCMR","AAABKRUI","AAABMCZF","AAABMCZG","AAABMCSO","AAABMQVT","AAABMQVU","AAABMCRT","AAABMCRU","AAABMPTW","AAABMCQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMCQI" decimal(13,2) NOT NULL default '0.00',
  "AAABJKYU" decimal(13,2) NOT NULL default '0.00',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMCMR","AAABKRUI","AAABMCZF","AAABMCZG","AAABMCSO","AAABMQVT","AAABMQVU","AAABMCRT","AAABMCRU","AAABMPTW","AAABMCQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHOPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHOPY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHONI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHONM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAABMPTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHONT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABKRUI","AAABMCZF","AAABMCZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAAAHOPI" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPK" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPM" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPN" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPO" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPP" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPR" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPS" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAHOPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHOPY" varchar(4) NOT NULL default ' ',
  "AAAAHOPI" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPK" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPM" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPN" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPO" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPP" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPR" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPS" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPT" decimal(5,2) NOT NULL default '0.00',
  "AAAAHOPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAAAHOPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOPZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHPTN" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAAAHOQE" varchar(10) NOT NULL default ' ',
  "AAAAHOQF" varchar(10) NOT NULL default ' ',
  "AAAAHWFK" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAABKIFQ" varchar(8) NOT NULL default '00000000',
  "AAABAEUS" varchar(8) NOT NULL default '00000000',
  "AAAAHWFQ" varchar(20) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  "AAABFBEW" varchar(32) NOT NULL default ' ',
  "AAAAHWFT" varchar(1) NOT NULL default ' ',
  "AAAAIUWS" varchar(1) NOT NULL default ' ',
  "AAAAHPTQ" varchar(1) NOT NULL default ' ',
  "AAAAIUUR" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDRZ" varchar(4) NOT NULL default ' ',
  "AAAAIDSA" varchar(20) NOT NULL default ' ',
  "AAAAIDRX" varchar(1) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAAAHYBB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHPTN","AAABMQCJ"),
  KEY "AAAAHORJ" ("AAABLUXQ","AAABMQCE","AAABKJVG","AAABMQCD","AAABMCOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHORP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHOSG" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAAAHZIY" varchar(10) NOT NULL default ' ',
  "AAAAHZII" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAAAHOSD" varchar(1) NOT NULL default ' ',
  "AAAAHRRD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHOSG","AAABMREX","AAABMDBB","AAABMLEQ","AAABMQCA","AAABMQCB","AAAAHZIY","AAAAHZII","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOSP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHPTN" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAAAHWGM" varchar(1) NOT NULL default ' ',
  "AAAAHXUV" varchar(1) NOT NULL default '0',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHXWE" varchar(4) NOT NULL default '0000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIWOQ" varchar(16) NOT NULL default ' ',
  "AAAAIWOR" varchar(24) NOT NULL default ' ',
  "AAABANNN" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAAAHWHC" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHE" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHF" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHG" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHH" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHI" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHK" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHL" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHM" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHN" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHO" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHP" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHQ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHR" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHS" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHT" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHU" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHV" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHW" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHX" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHY" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHZ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIA" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIB" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIC" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWID" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIE" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIF" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIG" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIH" decimal(17,2) NOT NULL default '0.00',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHPTN","AAABMQCJ","AAABMQCO","AAABMQHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHOZN" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAAAHOVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOVG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAAAIVEY" varchar(10) NOT NULL default ' ',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAHOVL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPPR","AAAAIVEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOVP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAAAIVEY" varchar(10) NOT NULL default ' ',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAHPHI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPPR","AAAAIVEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default '0',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAHOWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABLRXL","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default '0',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAHOWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABLRXL","AAABDMCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABLRXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(4) NOT NULL default ' ',
  "AAAAHXFE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABDMCO","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAAAHXFE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABDMCO","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWH" varchar(10) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWH","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAAAHXFE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABDMCO","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHOZN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAAAHOZU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHOZY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMKWZ" varchar(2) NOT NULL default ' ',
  "AAABMREM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMKWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPAE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHPBH" varchar(4) NOT NULL default ' ',
  "AAAAHPAZ" varchar(4) NOT NULL default ' ',
  "AAAAHPBA" varchar(16) NOT NULL default ' ',
  "AAAAHPBB" varchar(24) NOT NULL default ' ',
  "AAAAHPBC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPNV","AAAAHZZQ","AAAAHPBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPAT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHPBH" varchar(4) NOT NULL default ' ',
  "AAAAHPAZ" varchar(4) NOT NULL default ' ',
  "AAAAHPBA" varchar(16) NOT NULL default ' ',
  "AAAAHPBB" varchar(24) NOT NULL default ' ',
  "AAAAHPBC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKJYV","AAAAHZZQ","AAAAHPBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPBI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIJTA","AAABIWLC","AAAAHZIB","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPBV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIJTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPCI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIJTA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPCS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHPCW" varchar(1) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAHPCY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPDC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAHPDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPDN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHSAJ" varchar(10) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHSAJ","AAAAHZID","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPDY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHSAJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAUFDY","AAAAHSAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPEH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHSAJ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAUFDY","AAAAHSAJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLLWR" varchar(9) NOT NULL default '000000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHPFB" int(11) NOT NULL default '0',
  "AAABIJYD" varchar(1) NOT NULL default ' ',
  "AAABJSEW" varchar(1) NOT NULL default ' ',
  "AAAAHPFE" varchar(1) NOT NULL default ' ',
  "AAAAHPFF" varchar(1) NOT NULL default ' ',
  "AAAAIEJV" varchar(1) NOT NULL default ' ',
  "AAAAHPFH" varchar(1) NOT NULL default ' ',
  "AAAAHPFI" varchar(1) NOT NULL default ' ',
  "AAAAHPFJ" varchar(1) NOT NULL default ' ',
  "AAAAHPFK" int(11) NOT NULL default '0',
  "AAAAHPFL" int(11) NOT NULL default '0',
  "AAAAHPFM" varchar(1) NOT NULL default ' ',
  "AAAAHPFN" varchar(1) NOT NULL default ' ',
  "AAAAHPFO" varchar(1) NOT NULL default ' ',
  "AAAAHPFP" varchar(1) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABLLWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHPGS" varchar(10) NOT NULL default ' ',
  "AAAAHPGT" varchar(16) NOT NULL default ' ',
  "AAAAHPGU" varchar(24) NOT NULL default ' ',
  "AAAAHPGV" varchar(16) NOT NULL default ' ',
  "AAAAHPGH" varchar(10) NOT NULL default ' ',
  "AAAAHPGI" varchar(16) NOT NULL default ' ',
  "AAAAHPGJ" varchar(24) NOT NULL default ' ',
  "AAAAHPGK" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABCPQF","AAABKGOS","AAABKGVD","AAABKJYV","AAAAHZZQ","AAAAHPGS","AAAAHPGT","AAAAHPGU","AAAAHPGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPHD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAAAIVEY" varchar(10) NOT NULL default ' ',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAHPHI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPPR","AAAAIVEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPNF" varchar(1) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHPNJ" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAAAQHMW" varchar(16) NOT NULL default ' ',
  "AAAAQHMX" varchar(24) NOT NULL default ' ',
  "AAAAIURH" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPNF","AAABCPQF","AAABLPMM","AAABKGOS","AAAAHPNJ","AAABKGVD","AAAAHZID","AAABIYQF","AAAAQHMW","AAAAQHMX","AAAAIURH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPIN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAHPIR" varchar(2) NOT NULL default '00',
  "AAAAHPIS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHZIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPIW" (
  "AAAAHPJF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAHPJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPIZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHPJF" varchar(3) NOT NULL default '000',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAHPJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPNF" varchar(1) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHPNJ" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABCPOB" varchar(4) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAAAQHMW" varchar(16) NOT NULL default ' ',
  "AAAAQHMX" varchar(24) NOT NULL default ' ',
  "AAAAIURH" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPNF","AAABCPQF","AAABLPMM","AAABKGOS","AAAAHPNJ","AAABKGVD","AAABCPOB","AAABIYQF","AAAAQHMW","AAAAQHMX","AAAAIURH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPNF" varchar(1) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHPNJ" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABCPOB" varchar(4) NOT NULL default ' ',
  "AAAAIURH" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPNF","AAABCPQF","AAABLPMM","AAABKGOS","AAAAHPNJ","AAABKGVD","AAABCPOB","AAAAIURH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPNF" varchar(1) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHPNJ" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAAAHRYU" varchar(4) NOT NULL default ' ',
  "AAAAIXFG" varchar(4) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHXFE" varchar(4) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAAAHXFF" varchar(1) NOT NULL default ' ',
  "AAAAHPNS" varchar(1) NOT NULL default ' ',
  "AAAAIURH" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPNF","AAABCPQF","AAABLPMM","AAABKGOS","AAAAHPNJ","AAABKGVD","AAABDMCO","AAAAHRYU","AAAAIXFG","AAAAHZZQ","AAAAHXFE","AAAAQDNR","AAAAHXFF","AAAAHPNS","AAAAIURH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPNF" varchar(1) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHPNJ" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAAAHRYU" varchar(4) NOT NULL default ' ',
  "AAAAIXFG" varchar(4) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHXFE" varchar(4) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAAAHXFF" varchar(1) NOT NULL default ' ',
  "AAAAHPNS" varchar(1) NOT NULL default ' ',
  "AAAAIURH" varchar(3) NOT NULL default '000',
  "AAABLPPS" varchar(10) NOT NULL default ' ',
  "AAABLPPT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPNF","AAABCPQF","AAABLPMM","AAABKGOS","AAAAHPNJ","AAABKGVD","AAABDMCO","AAAAHRYU","AAAAIXFG","AAAAHZZQ","AAAAHXFE","AAAAQDNR","AAAAHXFF","AAAAHPNS","AAAAIURH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPNU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABMQBQ" varchar(64) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  "AAABJLTX" varchar(32) NOT NULL default ' ',
  "AAAAIWXL" varchar(10) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABHJRW","AAABCPQF","AAAAHZIB","AAABLXTG","AAABKRUO","AAABLTXI","AAABIWLC","AAAAHZID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPOX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHPPB" varchar(1) NOT NULL default ' ',
  "AAAAHTBP" varchar(1) NOT NULL default ' ',
  "AAAAHTBQ" varchar(1) NOT NULL default ' ',
  "AAAAHTBR" varchar(1) NOT NULL default ' ',
  "AAAAHTBS" varchar(1) NOT NULL default ' ',
  "AAAAHTBT" varchar(1) NOT NULL default ' ',
  "AAAAHTBU" varchar(1) NOT NULL default ' ',
  "AAAAHTBV" varchar(1) NOT NULL default ' ',
  "AAAAHPPJ" varchar(1) NOT NULL default ' ',
  "AAAAHPPK" varchar(1) NOT NULL default ' ',
  "AAAAHPPL" varchar(1) NOT NULL default ' ',
  "AAABIKIG" varchar(30) NOT NULL default ' ',
  "AAAAHPPN" varchar(1) NOT NULL default ' ',
  "AAAAHPPO" varchar(30) NOT NULL default ' ',
  "AAABFLFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABHJRW","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPPT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHZID","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPQC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAABMJVH" varchar(5) NOT NULL default ' ',
  "AAABGYQH" varchar(20) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHZID","AAABMLQA","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHPQW" varchar(1) NOT NULL default ' ',
  "AAAAHPQX" varchar(1) NOT NULL default ' ',
  "AAAAHPQY" varchar(1) NOT NULL default ' ',
  "AAAAHPQZ" varchar(1) NOT NULL default ' ',
  "AAAAHPRA" varchar(1) NOT NULL default ' ',
  "AAAAHPRB" varchar(1) NOT NULL default ' ',
  "AAAAHPRC" varchar(1) NOT NULL default ' ',
  "AAAAHPRD" varchar(1) NOT NULL default ' ',
  "AAAAHPRE" varchar(1) NOT NULL default ' ',
  "AAAAHPRF" varchar(1) NOT NULL default ' ',
  "AAAAHPRG" varchar(1) NOT NULL default ' ',
  "AAAAHPRH" varchar(1) NOT NULL default ' ',
  "AAAAHPRI" varchar(10) NOT NULL default ' ',
  "AAAAHPRJ" varchar(10) NOT NULL default ' ',
  "AAAAHPRK" varchar(10) NOT NULL default ' ',
  "AAAAHPRL" varchar(10) NOT NULL default ' ',
  "AAAAHPRM" varchar(10) NOT NULL default ' ',
  "AAAAHPRN" varchar(10) NOT NULL default ' ',
  "AAAAHPRO" varchar(10) NOT NULL default ' ',
  "AAAAHPRP" varchar(10) NOT NULL default ' ',
  "AAAAHPRQ" varchar(10) NOT NULL default ' ',
  "AAAAHPRR" varchar(10) NOT NULL default ' ',
  "AAAAHPRS" varchar(10) NOT NULL default ' ',
  "AAAAHPRT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCZG","AAABMDBB","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPRZ" (
  "AAAAHPSC" varchar(2) NOT NULL default ' ',
  "AAAAHPSB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHPSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPSD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAABLTXI","AAAAHZIB","AAABKJYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPSR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  "AAAAHPSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPTC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAARXMN" varchar(30) NOT NULL default ' ',
  "AAABKQPS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAARXMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPTI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHPTW" varchar(3) NOT NULL default '000',
  "AAAAHPTN" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAAAIUWS" varchar(1) NOT NULL default ' ',
  "AAAAHPTQ" varchar(1) NOT NULL default ' ',
  "AAAAIUUR" varchar(10) NOT NULL default ' ',
  "AAAAHPTS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF","AAABCPQF","AAAAHPTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPTX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHWFK" varchar(4) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAAAHYBB" varchar(6) NOT NULL default ' ',
  "AAAAHPUF" varchar(15) NOT NULL default ' ',
  "AAAAHPUG" varchar(15) NOT NULL default ' ',
  "AAAAHPUH" varchar(15) NOT NULL default ' ',
  "AAAAHPUI" varchar(15) NOT NULL default ' ',
  "AAAAHPUJ" varchar(15) NOT NULL default ' ',
  "AAAAHPUK" varchar(1) NOT NULL default ' ',
  "AAAAHPUL" varchar(1) NOT NULL default ' ',
  "AAAAHPUM" varchar(1) NOT NULL default ' ',
  "AAAAHPUN" varchar(1) NOT NULL default ' ',
  "AAAAHPUO" varchar(1) NOT NULL default ' ',
  "AAAAHPUP" varchar(1) NOT NULL default ' ',
  "AAAAHPUQ" varchar(1) NOT NULL default ' ',
  "AAAAHPUR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPUU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHPVD" varchar(4) NOT NULL default '0000',
  "AAAAHPVE" varchar(2) NOT NULL default ' ',
  "AAAAHPVF" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHPVD","AAAAHPVE","AAAAHPVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPVG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABGZHE" varchar(50) NOT NULL default ' ',
  "AAAAHPVM" varchar(4) NOT NULL default ' ',
  "AAAAHPVN" varchar(4) NOT NULL default ' ',
  "AAAAHPVO" varchar(3) NOT NULL default '000',
  "AAAAHPVP" varchar(4) NOT NULL default ' ',
  "AAAAHPVQ" varchar(50) NOT NULL default ' ',
  "AAAAHPVR" varchar(4) NOT NULL default ' ',
  "AAAAHPVS" varchar(4) NOT NULL default ' ',
  "AAAAHPVT" varchar(3) NOT NULL default '000',
  "AAAAHPVU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKGOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHPXV" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAIBYM" varchar(4) NOT NULL default '0000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAAAQGXS" varchar(16) NOT NULL default ' ',
  "AAAAHPWR" varchar(2) NOT NULL default ' ',
  "AAAAHPZJ" varchar(2) NOT NULL default ' ',
  "AAABFDJI" varchar(4) NOT NULL default '0000',
  "AAAAHPYO" varchar(20) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAAAHZNN" varchar(16) NOT NULL default ' ',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAHRKJ" varchar(24) NOT NULL default ' ',
  "AAAAHPZN" varchar(20) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHPXH" varchar(1) NOT NULL default ' ',
  "AAAAPWTG" varchar(10) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHXSZ" varchar(2) NOT NULL default '00',
  "AAAAHPZT" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHPXN" varchar(1) NOT NULL default ' ',
  "AAAAHPXO" varchar(1) NOT NULL default ' ',
  "AAAAHPXP" varchar(1) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAAAHQAT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHPXV","AAABMCNU","AAABMQHY"),
  KEY "AAAAHPXY" ("AAABMRIL","AAAAHRIH","AAAAHRIF"),
  KEY "AAAAHPYC" ("AAABMRIL","AAABFDJI","AAABKRXF","AAABIIKX","AAABMREX"),
  KEY "AAAAHPYI" ("AAABMRIL","AAAAHZNN","AAAAIEPT","AAAAIEPV","AAABIBHQ","AAAAHPYO","AAAAHRKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABFDJI" varchar(4) NOT NULL default '0000',
  "AAAAHQAT" varchar(10) NOT NULL default ' ',
  "AAAAHPZY" varchar(8) NOT NULL default '00000000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAIBYM" varchar(4) NOT NULL default '0000',
  "AAAAHPZJ" varchar(2) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAHXSZ" varchar(2) NOT NULL default '00',
  "AAAAHPZN" varchar(20) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAAAHPZT" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABFDJI","AAAAHQAT","AAAAHPZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHPZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABFDJI" varchar(4) NOT NULL default '0000',
  "AAAAHQAT" varchar(10) NOT NULL default ' ',
  "AAABLTLR" varchar(1) NOT NULL default ' ',
  "AAABLKHG" varchar(1) NOT NULL default ' ',
  "AAAAHQAG" varchar(8) NOT NULL default '00000000',
  "AAAAHQAH" varchar(6) NOT NULL default '000000',
  "AAAAHQAI" varchar(12) NOT NULL default ' ',
  "AAAAHQAJ" varchar(8) NOT NULL default '00000000',
  "AAAAHQAK" varchar(6) NOT NULL default '000000',
  "AAAAHQAL" varchar(12) NOT NULL default ' ',
  "AAAAHQAM" varchar(8) NOT NULL default '00000000',
  "AAAAHQAN" varchar(6) NOT NULL default '000000',
  "AAAAHQAO" varchar(12) NOT NULL default ' ',
  "AAAAUQQT" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABFDJI","AAAAHQAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQAU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAAAHQBH" varchar(1) NOT NULL default ' ',
  "AAABLNQV" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABLTXI","AAABIWLC","AAABHJRW","AAAAHQBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHQBZ" varchar(1) NOT NULL default ' ',
  "AAAAHQCA" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCB" varchar(1) NOT NULL default ' ',
  "AAAAHQCC" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCD" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCE" varchar(1) NOT NULL default ' ',
  "AAAAHQCF" decimal(5,2) NOT NULL default '0.00',
  "AAAAHQCG" varchar(1) NOT NULL default ' ',
  "AAAAHQCH" varchar(1) NOT NULL default ' ',
  "AAAAHQCI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCJ" varchar(1) NOT NULL default ' ',
  "AAAAHQCK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQCM" varchar(1) NOT NULL default ' ',
  "AAAAHQCN" decimal(5,2) NOT NULL default '0.00',
  "AAAAHQCO" varchar(1) NOT NULL default ' ',
  "AAAAHQCP" varchar(10) NOT NULL default ' ',
  "AAAAHQCQ" varchar(16) NOT NULL default ' ',
  "AAAAHQCR" varchar(24) NOT NULL default ' ',
  "AAAAHQCS" varchar(10) NOT NULL default ' ',
  "AAAAHQCT" varchar(16) NOT NULL default ' ',
  "AAAAHQCU" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMCNU","AAABJKCJ","AAABMCZF","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHQEP" varchar(10) NOT NULL default ' ',
  "AAAAHQEQ" varchar(16) NOT NULL default ' ',
  "AAAAHQER" varchar(24) NOT NULL default ' ',
  "AAABCRYR" varchar(1) NOT NULL default ' ',
  "AAAAHQDM" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMCNU","AAABJKCJ","AAABMCZF","AAABKRUI","AAAAHQEP","AAAAHQEQ","AAAAHQER","AAABCRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHQEP" varchar(10) NOT NULL default ' ',
  "AAAAHQEQ" varchar(16) NOT NULL default ' ',
  "AAAAHQER" varchar(24) NOT NULL default ' ',
  "AAAAHQEH" decimal(5,2) NOT NULL default '0.00',
  "AAAAHQEI" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMCNU","AAABJKCJ","AAABMCZF","AAABKRUI","AAAAHQEP","AAAAHQEQ","AAAAHQER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQES" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHQEV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQEY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAHQGQ" varchar(16) NOT NULL default ' ',
  "AAAAHQFL" varchar(22) NOT NULL default ' ',
  "AAAAHQFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHZIB","AAAAHQGQ","AAAAHQFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQFM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAHQGQ" varchar(16) NOT NULL default ' ',
  "AAAAHQFS" varchar(1) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHZIB","AAAAHQGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQGE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAHQGQ" varchar(16) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHZIB","AAAAHQGQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABCCXW","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABCCXW","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHQSL" varchar(4) NOT NULL default '0000',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHQSL","AAAAHRIS","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHQSL" varchar(4) NOT NULL default '0000',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHQSL","AAAAHRIS","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  "AAAAHQRO" varchar(16) NOT NULL default ' ',
  "AAAAHQRP" varchar(22) NOT NULL default ' ',
  "AAAAHQRQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABCCXW","AAABJSBE"),
  KEY "AAAAHQND" ("AAABLXTG","AAABMCZG","AAABIHLK","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  "AAAAHQRO" varchar(16) NOT NULL default ' ',
  "AAAAHQRP" varchar(22) NOT NULL default ' ',
  "AAAAHQRQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABCCXW","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHQSL" varchar(4) NOT NULL default '0000',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  "AAAAHQRO" varchar(16) NOT NULL default ' ',
  "AAAAHQRP" varchar(22) NOT NULL default ' ',
  "AAAAHQRQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHQSL","AAAAHRIS","AAAAHQZY","AAABLXTG","AAABIHLK","AAABCPWH","AAABMCZG","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHQSL" varchar(4) NOT NULL default '0000',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQRJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQRL" varchar(16) NOT NULL default ' ',
  "AAAAHQRM" varchar(22) NOT NULL default ' ',
  "AAAAHQRN" varchar(8) NOT NULL default ' ',
  "AAAAHQRO" varchar(16) NOT NULL default ' ',
  "AAAAHQRP" varchar(22) NOT NULL default ' ',
  "AAAAHQRQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHQSL","AAAAHRIS","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAABKRXK","AAAAHRJN","AAAAHQXS","AAABJSBE"),
  KEY "AAAAHQSH" ("AAABLXTG","AAABMCZG","AAABIHLK","AAAAHQSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHQSX" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQSY" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABCCXW","AAAAHQZY","AAABLXTG","AAABIHLK","AAABMCZG","AAABKRXK","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQTM" varchar(1) NOT NULL default ' ',
  "AAAAHQTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAHQUX" varchar(8) NOT NULL default ' ',
  "AAAAHQUY" varchar(22) NOT NULL default ' ',
  "AAAAHQUZ" varchar(8) NOT NULL default ' ',
  "AAAAHQVA" varchar(4) NOT NULL default '0000',
  "AAAAHQVB" varchar(4) NOT NULL default '0000',
  "AAAAHQVC" varchar(10) NOT NULL default ' ',
  "AAAAHQVD" varchar(10) NOT NULL default ' ',
  "AAAAHQVE" varchar(16) NOT NULL default ' ',
  "AAAAHQVF" varchar(16) NOT NULL default ' ',
  "AAAAHQVG" varchar(6) NOT NULL default ' ',
  "AAAAHQVH" varchar(6) NOT NULL default ' ',
  "AAAAHQVI" varchar(2) NOT NULL default ' ',
  "AAAAHQVJ" varchar(3) NOT NULL default ' ',
  "AAAAHQVK" varchar(4) NOT NULL default '0000',
  "AAAAHQUI" varchar(2) NOT NULL default ' ',
  "AAAAHQUJ" varchar(3) NOT NULL default ' ',
  "AAAAHQUK" varchar(4) NOT NULL default '0000',
  "AAAAHQUL" varchar(4) NOT NULL default ' ',
  "AAAAHQUM" varchar(4) NOT NULL default ' ',
  "AAAAHQUN" varchar(20) NOT NULL default ' ',
  "AAABABCK" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMBIU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHQUR" varchar(50) NOT NULL default ' ',
  "AAAAHQUS" varchar(32) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIUPD","AAAAHQUX","AAAAHQUY","AAAAHQUZ","AAAAHQVA","AAAAHQVB","AAAAHQVC","AAAAHQVD","AAAAHQVE","AAAAHQVF","AAAAHQVG","AAAAHQVH","AAAAHQVI","AAAAHQVJ","AAAAHQVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAHQVX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHQWQ" varchar(1) NOT NULL default ' ',
  "AAAAHQWG" varchar(1) NOT NULL default ' ',
  "AAAAHQXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHQWQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMQHV","AAABMDBB","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAAAHQXS" varchar(2) NOT NULL default ' ',
  "AAAAHQXM" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAHQZY","AAAAHQXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHQXX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHRKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHQZX" varchar(1) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHQZP" decimal(5,2) NOT NULL default '0.00',
  "AAAAHQZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABIHLK","AAABMCZG","AAABCPWH","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAAAHQZX","AAAAHRKZ","AAAAHQZY","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAHQZT" varchar(22) NOT NULL default ' ',
  "AAAAHQZU" varchar(8) NOT NULL default ' ',
  "AAAAHQZV" varchar(10) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHQZX" varchar(1) NOT NULL default ' ',
  "AAAAHQZY" varchar(2) NOT NULL default '00',
  "AAAAHQZP" decimal(5,2) NOT NULL default '0.00',
  "AAAAHQZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAHQZT","AAAAHQZU","AAAAHQZV","AAAAHRIR","AAAAHQZX","AAAAHQZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHQZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHRAH" varchar(10) NOT NULL default ' ',
  "AAAAHRAI" varchar(3) NOT NULL default '000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAHRAK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHRAL" decimal(15,2) NOT NULL default '0.00',
  "AAABKAOO" varchar(10) NOT NULL default ' ',
  "AAABKAOQ" varchar(3) NOT NULL default '000',
  "AAAAHRAO" varchar(2) NOT NULL default '00',
  "AAAAHRAP" varchar(4) NOT NULL default '0000',
  "AAAAHRAQ" varchar(3) NOT NULL default '000',
  "AAAAHRAR" varchar(2) NOT NULL default '00',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMCNU","AAABMDBA","AAABMPWO","AAAAHRJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(17,2) NOT NULL default '0.00',
  "AAAALNWO" varchar(4) NOT NULL default ' ',
  "AAABKAOP" varchar(4) NOT NULL default '0000',
  "AAABKAOO" varchar(10) NOT NULL default ' ',
  "AAABKAOQ" varchar(3) NOT NULL default '000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHRIN" varchar(16) NOT NULL default ' ',
  "AAAAHRIO" varchar(10) NOT NULL default ' ',
  "AAAAHRIP" varchar(14) NOT NULL default ' ',
  "AAAAHRIQ" varchar(24) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHRIU" varchar(1) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAAAHRIW" varchar(2) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU","AAAAIBYE","AAABMQHV","AAABMDBB","AAABLTXE","AAAAHRJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(17,2) NOT NULL default '0.00',
  "AAAALNWO" varchar(4) NOT NULL default ' ',
  "AAABKAOP" varchar(4) NOT NULL default '0000',
  "AAABKAOO" varchar(10) NOT NULL default ' ',
  "AAABKAOQ" varchar(3) NOT NULL default '000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHRIN" varchar(16) NOT NULL default ' ',
  "AAAAHRIO" varchar(10) NOT NULL default ' ',
  "AAAAHRIQ" varchar(24) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHRIU" varchar(1) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU","AAAAIBYE","AAABMQHV","AAABMDBB","AAABLTXE","AAAAHRJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(17,2) NOT NULL default '0.00',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHRIN" varchar(16) NOT NULL default ' ',
  "AAAAHRIO" varchar(10) NOT NULL default ' ',
  "AAAAHRIP" varchar(14) NOT NULL default ' ',
  "AAAAHRIQ" varchar(24) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHRIU" varchar(1) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAAAHRIW" varchar(2) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIKX","AAABIBEW","AAAAKZUL","AAABLMJN","AAAAHXGC","AAAAHXGD","AAAAHXGE","AAABJSBR","AAAAKZUN","AAABMQHV","AAABMDBB","AAABLTXE","AAAAHRJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAHRJN" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(17,2) NOT NULL default '0.00',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  "AAAAHRIF" varchar(8) NOT NULL default '00000000',
  "AAAAHRIG" varchar(6) NOT NULL default '000000',
  "AAAAHRIH" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHRIN" varchar(16) NOT NULL default ' ',
  "AAAAHRIO" varchar(10) NOT NULL default ' ',
  "AAAAHRIP" varchar(14) NOT NULL default ' ',
  "AAAAHRIQ" varchar(24) NOT NULL default ' ',
  "AAAAHRIR" varchar(16) NOT NULL default ' ',
  "AAAAHRIS" varchar(4) NOT NULL default '0000',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAHRIU" varchar(1) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAAAHRIW" varchar(2) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIKX","AAABIBEW","AAAAKZUL","AAABLMJN","AAAAHXGC","AAAAHXGD","AAAAHXGE","AAABJSBR","AAAAKZUN","AAABMQHV","AAABMDBB","AAABLTXE","AAAAHRJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAAAZOEP" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHRKC" varchar(14) NOT NULL default ' ',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAAAHZNN" varchar(16) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAJTTK" varchar(10) NOT NULL default ' ',
  "AAAAHRKI" varchar(22) NOT NULL default ' ',
  "AAAAHRKJ" varchar(24) NOT NULL default ' ',
  "AAAAXZXG" varchar(20) NOT NULL default ' ',
  "AAAAHRKL" varchar(14) NOT NULL default ' ',
  "AAAAHZMK" varchar(24) NOT NULL default ' ',
  "AAAAHZNI" varchar(16) NOT NULL default ' ',
  "AAAAHZNU" varchar(10) NOT NULL default ' ',
  "AAAAHZMW" varchar(16) NOT NULL default ' ',
  "AAAAHRKQ" varchar(10) NOT NULL default ' ',
  "AAAAHRKR" varchar(22) NOT NULL default ' ',
  "AAAAHRKS" varchar(24) NOT NULL default ' ',
  "AAAAHRKT" varchar(20) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHRKZ" varchar(10) NOT NULL default ' ',
  "AAAAKZVM" varchar(8) NOT NULL default '00000000',
  "AAAAHRLB" varchar(8) NOT NULL default '00000000',
  "AAAAHRLC" varchar(6) NOT NULL default '000000',
  "AAAAHRLD" varchar(12) NOT NULL default ' ',
  "AAAAHRLE" varchar(8) NOT NULL default '00000000',
  "AAAAHRLF" varchar(6) NOT NULL default '000000',
  "AAAAHRLG" varchar(12) NOT NULL default ' ',
  "AAAAHZAU" varchar(1) NOT NULL default ' ',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABKWQO" varchar(3) NOT NULL default ' ',
  "AAAAHRLK" varchar(1) NOT NULL default ' ',
  "AAAAIBYL" varchar(10) NOT NULL default ' ',
  "AAAAHWYT" varchar(3) NOT NULL default '000',
  "AAAAIBYM" varchar(4) NOT NULL default '0000',
  "AAABKWSA" varchar(1) NOT NULL default ' ',
  "AAAAHRLP" varchar(1) NOT NULL default ' ',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAIEQE" varchar(4) NOT NULL default '0000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAHRLT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIKX","AAAAZOEP","AAABIBEW","AAAAKZUL","AAABJSBR","AAAAKZUN","AAABLMJN","AAAAHXGC","AAAAHXGD","AAAAHXGE","AAABMQHV"),
  KEY "AAAAHRMG" ("AAABMRIL","AAAAZOEP","AAABMQHV","AAABMCNU","AAAAKZVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABFTFQ" varchar(1) NOT NULL default '0',
  "AAAAHZBW" varchar(2) NOT NULL default '00',
  "AAABHXDH" varchar(1) NOT NULL default '0',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAABKPVV" varchar(3) NOT NULL default ' ',
  "AAAAHZCB" varchar(24) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJAUE" varchar(2) NOT NULL default ' ',
  "AAAAHZAL" varchar(1) NOT NULL default ' ',
  "AAAAHZAM" varchar(1) NOT NULL default ' ',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHZAP" varchar(1) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAAAIEJV" varchar(1) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAAAHZCD" varchar(1) NOT NULL default ' ',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  "AAAAHZAU" varchar(1) NOT NULL default ' ',
  "AAAAHZAV" varchar(1) NOT NULL default ' ',
  "AAAAHRNS" varchar(1) NOT NULL default ' ',
  "AAAAHZAW" varchar(4) NOT NULL default ' ',
  "AAAAOSPJ" varchar(16) NOT NULL default ' ',
  "AAAAHZAY" varchar(1) NOT NULL default ' ',
  "AAAAHZAZ" varchar(1) NOT NULL default ' ',
  "AAAAHZAR" varchar(1) NOT NULL default ' ',
  "AAAAHZAS" varchar(1) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAAAHROC" varchar(1) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYKD" varchar(10) NOT NULL default ' ',
  "AAAAHYKV" varchar(10) NOT NULL default ' ',
  "AAAAHYLN" varchar(10) NOT NULL default ' ',
  "AAAAHYMF" varchar(10) NOT NULL default ' ',
  "AAAAHYMX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  "AAABMJVF" varchar(50) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAAAIEKE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAHZIY" varchar(10) NOT NULL default ' ',
  "AAAAHZII" varchar(1) NOT NULL default ' ',
  "AAABMCPZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAQDPN" varchar(8) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABLCFS" varchar(50) NOT NULL default ' ',
  "AAABMCNC" varchar(10) NOT NULL default ' ',
  "AAAAHRRA" varchar(1) NOT NULL default ' ',
  "AAAAHRRB" varchar(1) NOT NULL default ' ',
  "AAAAHRRC" varchar(1) NOT NULL default ' ',
  "AAAAHRRD" varchar(20) NOT NULL default ' ',
  "AAABJWEQ" varchar(10) NOT NULL default ' ',
  "AAABJWER" varchar(3) NOT NULL default '000',
  "AAABJWEP" varchar(4) NOT NULL default ' ',
  "AAABJWES" varchar(4) NOT NULL default '0000',
  "AAAAHRRI" varchar(1) NOT NULL default ' ',
  "AAAAHRRJ" varchar(10) NOT NULL default ' ',
  "AAAAHRRK" varchar(4) NOT NULL default '0000',
  "AAAAHRRL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(15,2) NOT NULL default '0.00',
  "AAAAHRRU" varchar(8) NOT NULL default '00000000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHRSW" varchar(10) NOT NULL default ' ',
  "AAAAHRSX" varchar(4) NOT NULL default '0000',
  "AAAAHRSY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAAAHRSW","AAAAHRSX","AAAAHRSY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default '00',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABCPOE","AAABKGVD","AAABDMCO","AAAAHZZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZLG" varchar(10) NOT NULL default ' ',
  "AAAAKZPQ" varchar(5) NOT NULL default '00000',
  "AAAAKZPT" varchar(2) NOT NULL default '00',
  "AAAAKZPU" varchar(4) NOT NULL default '0000',
  "AAAAKZPV" varchar(3) NOT NULL default '000',
  "AAAAKZPW" varchar(2) NOT NULL default '00',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAHRTV" varchar(1) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZLG","AAAAKZPQ","AAAAKZPT","AAAAKZPU","AAAAKZPV","AAAAKZPW","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAABHDBW" varchar(30) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAAAHWAB" varchar(1) NOT NULL default ' ',
  "AAAAHRUP" varchar(60) NOT NULL default ' ',
  "AAAAHRUQ" varchar(60) NOT NULL default ' ',
  "AAAAHRUR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMDEN","AAAAHVEX","AAABHDBW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAAAHRVB" varchar(40) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAAAHVEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAAAHRVP" varchar(2) NOT NULL default ' ',
  "AAAAHRVL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAAAHVEX","AAAAHRVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHHTI" varchar(60) NOT NULL default ' ',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAAAHVEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHRWF" varchar(12) NOT NULL default ' ',
  "AAABIKIG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAAAHVEX","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAAAHRXS" varchar(30) NOT NULL default ' ',
  "AAAAHRXT" varchar(1) NOT NULL default ' ',
  "AAAAHRWS" varchar(1) NOT NULL default ' ',
  "AAAAHRWT" varchar(30) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAAAHRWV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMDEN","AAAAHVEX","AAAAHRXS","AAAAHRXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAAAHRXS" varchar(30) NOT NULL default ' ',
  "AAAAHRXT" varchar(1) NOT NULL default ' ',
  "AAAAHRXU" varchar(20) NOT NULL default ' ',
  "AAABCKXS" varchar(50) NOT NULL default ' ',
  "AAAAHRXL" varchar(60) NOT NULL default ' ',
  "AAAAHRXM" varchar(60) NOT NULL default ' ',
  "AAAAHRXN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMDEN","AAAAHVEX","AAAAHRXS","AAAAHRXT","AAAAHRXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABLPNX" varchar(4) NOT NULL default ' ',
  "AAAAHRZO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDMCO","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRYQ" (
  "AAAAHRZO" varchar(2) NOT NULL default ' ',
  "AAAAIXFG" varchar(4) NOT NULL default ' ',
  "AAABMMRH" varchar(2) NOT NULL default '00',
  "AAAAHRYU" varchar(4) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHRZO","AAAAIXFG","AAABMMRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRYZ" (
  "AAAAHRZO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHRZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRZC" (
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAAAHRZO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMCO","AAAAHRZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRZH" (
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAAAHRZO" varchar(2) NOT NULL default ' ',
  "AAAAHRZP" varchar(1) NOT NULL default ' ',
  "AAAAHRZQ" varchar(1) NOT NULL default ' ',
  "AAAAHRZR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABDMCO","AAAAHRZO","AAAAHRZP","AAAAHRZQ","AAAAHRZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHRZS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAHSAJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPNV","AAABCPOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSAD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAHSAJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKJYV","AAABCPOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAHSHM" varchar(3) NOT NULL default ' ',
  "AAAAHSHN" varchar(4) NOT NULL default '0000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAAAHSHR" varchar(3) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABLPQZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBH" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSBO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSQ" decimal(15,2) NOT NULL default '0.00',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAHSHM","AAAAHSHN","AAABMDBB","AAABMCNU","AAABMCZG","AAAAHSHR","AAABKOTY","AAABLXTG","AAABMCZF","AAABKRUI","AAABMCXH","AAABCPWH","AAABKRXF","AAABKRXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABCPUT" varchar(4) NOT NULL default ' ',
  "AAAAICQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABCPUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAABMDBB","AAABMCNU","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHSDU" varchar(1) NOT NULL default ' ',
  "AAAAHSDX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHSDU" varchar(1) NOT NULL default ' ',
  "AAAAHSDV" varchar(1) NOT NULL default ' ',
  "AAAAHSDW" varchar(1) NOT NULL default ' ',
  "AAAAHSDX" varchar(1) NOT NULL default ' ',
  "AAAAHSDY" varchar(1) NOT NULL default ' ',
  "AAAAHSDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHSFM" varchar(4) NOT NULL default '0000',
  "AAAAHSEI" varchar(24) NOT NULL default ' ',
  "AAAAHSEJ" varchar(10) NOT NULL default ' ',
  "AAAAHSEK" varchar(10) NOT NULL default ' ',
  "AAAAHSEL" varchar(10) NOT NULL default ' ',
  "AAAAHSEM" varchar(10) NOT NULL default ' ',
  "AAAAHSEN" varchar(10) NOT NULL default ' ',
  "AAAAHSEO" varchar(10) NOT NULL default ' ',
  "AAAAHSEP" varchar(16) NOT NULL default ' ',
  "AAAAHSEQ" varchar(10) NOT NULL default ' ',
  "AAAAHSER" varchar(10) NOT NULL default ' ',
  "AAAAHSES" varchar(10) NOT NULL default ' ',
  "AAAAHSET" varchar(10) NOT NULL default ' ',
  "AAAAHSEU" varchar(10) NOT NULL default ' ',
  "AAAAHSEV" varchar(10) NOT NULL default ' ',
  "AAAAHSEW" varchar(10) NOT NULL default ' ',
  "AAAAHSEX" varchar(10) NOT NULL default ' ',
  "AAAAHSEY" varchar(16) NOT NULL default ' ',
  "AAAAHSEZ" varchar(10) NOT NULL default ' ',
  "AAAAHSFA" varchar(10) NOT NULL default ' ',
  "AAAAHSFB" varchar(10) NOT NULL default ' ',
  "AAAAHSFC" varchar(10) NOT NULL default ' ',
  "AAAAHSFD" varchar(1) NOT NULL default ' ',
  "AAAAHSFE" varchar(6) NOT NULL default ' ',
  "AAAAHSFF" varchar(5) NOT NULL default ' ',
  "AAAAHSFG" varchar(5) NOT NULL default ' ',
  "AAAAHSFH" varchar(5) NOT NULL default ' ',
  "AAAAHSFI" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAHSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAHSHM" varchar(3) NOT NULL default ' ',
  "AAAAHSHN" varchar(4) NOT NULL default '0000',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHSHR" varchar(3) NOT NULL default ' ',
  "AAABJRIV" varchar(4) NOT NULL default '0000',
  "AAAAHSHT" varchar(18) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
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
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAHSHM","AAAAHSHN","AAABKOTY","AAABLXTG","AAABMCNU","AAAAHSHR","AAABJRIV","AAAAHSHT","AAAAHZIB","AAAAUFDY","AAAAHZID","AAABMQHS","AAABMQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAHSLL" varchar(8) NOT NULL default ' ',
  "AAAAHSJE" varchar(1) NOT NULL default ' ',
  "AAAAHSIZ" varchar(8) NOT NULL default ' ',
  "AAABLRYR" varchar(8) NOT NULL default '00000000',
  "AAABLZVN" varchar(6) NOT NULL default '000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLPZD" varchar(8) NOT NULL default '00000000',
  "AAABMAKM" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO"),
  KEY "AAAAHSIW" ("AAABMRIL","AAABMDBB","AAAAHSIZ","AAAAHSJE"),
  KEY "AAAAHSJB" ("AAABMRIL","AAABMDBB","AAAAHSJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHSLL" varchar(8) NOT NULL default ' ',
  "AAAAHSJJ" varchar(40) NOT NULL default ' ',
  "AAAAHSJK" varchar(28) NOT NULL default ' ',
  "AAAAHSJL" varchar(28) NOT NULL default ' ',
  "AAAAHSJM" varchar(30) NOT NULL default ' ',
  "AAAAHSJN" varchar(30) NOT NULL default ' ',
  "AAAAHSJO" varchar(30) NOT NULL default ' ',
  "AAAAHSJP" varchar(30) NOT NULL default ' ',
  "AAAAHSJQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHSLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAHSKD" varchar(21) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAAHSKD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAHSLL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCZF","AAABKRUI","AAAAJMCY","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHSNS" varchar(5) NOT NULL default ' ',
  "AAAAHSNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHSLL" varchar(8) NOT NULL default ' ',
  "AAAAHSLG" varchar(4) NOT NULL default '0000',
  "AAAAHSLH" varchar(7) NOT NULL default '0000000',
  "AAAAHSLI" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHSLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHSLP" varchar(20) NOT NULL default ' ',
  "AAAAHSLQ" varchar(20) NOT NULL default ' ',
  "AAAAHSLR" varchar(20) NOT NULL default ' ',
  "AAAAHSLS" varchar(20) NOT NULL default ' ',
  "AAAAHSLT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHSNS" varchar(5) NOT NULL default ' ',
  "AAAAHSMB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHSMI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHSNS" varchar(5) NOT NULL default ' ',
  "AAAAHSNT" varchar(1) NOT NULL default ' ',
  "AAAAHSMP" varchar(1) NOT NULL default ' ',
  "AAAAHSMQ" varchar(1) NOT NULL default ' ',
  "AAAAHSMR" varchar(1) NOT NULL default ' ',
  "AAAAHSMS" varchar(1) NOT NULL default ' ',
  "AAAAHSMT" varchar(1) NOT NULL default ' ',
  "AAAAHSMU" varchar(1) NOT NULL default ' ',
  "AAAAHSMV" varchar(1) NOT NULL default ' ',
  "AAAAHSMW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHSNS","AAAAHSNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHSNS" varchar(5) NOT NULL default ' ',
  "AAAAHSNT" varchar(1) NOT NULL default ' ',
  "AAAAHSNJ" varchar(1) NOT NULL default ' ',
  "AAAAHSNF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHSNS","AAAAHSNT","AAAAHSNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHSNS" varchar(5) NOT NULL default ' ',
  "AAAAHSNT" varchar(1) NOT NULL default ' ',
  "AAAAHSNP" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHSNS","AAAAHSNT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAHSOI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHSOJ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAANAVQ","AAABMDBB","AAABMCNU","AAABMCXH","AAABJKCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAAAHSRH" varchar(1) NOT NULL default ' ',
  "AAAAHSRI" varchar(1) NOT NULL default ' ',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAHSST" varchar(1) NOT NULL default ' ',
  "AAAAHSRL" varchar(1) NOT NULL default ' ',
  "AAAAHSPU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMCNU","AAABMDBB","AAAAICFT"),
  KEY "AAAAHSPP" ("AAABMCNU","AAABMDBB","AAAAICFT","AAAAHTGT","AAAAHSPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAHSRH" varchar(1) NOT NULL default ' ',
  "AAAAHSRI" varchar(1) NOT NULL default ' ',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAHSST" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICFT","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMDBB","AAABMCNU","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAAAHSRE" varchar(1) NOT NULL default ' ',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAAAHWQL" varchar(10) NOT NULL default ' ',
  "AAAAHSRH" varchar(1) NOT NULL default ' ',
  "AAAAHSRI" varchar(1) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAHSRL" varchar(1) NOT NULL default ' ',
  "AAAAHSRM" varchar(24) NOT NULL default ' ',
  "AAAAHSRN" varchar(16) NOT NULL default ' ',
  "AAAAHSRO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICFT","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  "AAABMJVF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAICFT","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAHSST" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICFT","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(4) NOT NULL default ' ',
  "AAAALNRB" varchar(4) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(4) NOT NULL default ' ',
  "AAAALNRB" varchar(4) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAAAHSUN" varchar(10) NOT NULL default ' ',
  "AAAAHSUO" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAAAHSUN","AAAAHSUO","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(4) NOT NULL default ' ',
  "AAAALNRB" varchar(4) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(12) NOT NULL default ' ',
  "AAAALNRB" varchar(12) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(12) NOT NULL default ' ',
  "AAAALNRB" varchar(12) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(24) NOT NULL default ' ',
  "AAAALNRB" varchar(24) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(24) NOT NULL default ' ',
  "AAAALNRB" varchar(24) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(10) NOT NULL default ' ',
  "AAAALNRB" varchar(10) NOT NULL default ' ',
  "AAAAKYFR" varchar(10) NOT NULL default ' ',
  "AAAAKYFS" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(10) NOT NULL default ' ',
  "AAAALNRB" varchar(10) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(24) NOT NULL default ' ',
  "AAAAHSZE" varchar(16) NOT NULL default ' ',
  "AAAAHSZF" varchar(10) NOT NULL default ' ',
  "AAAAHSZG" varchar(16) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHSZO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIHQV","AAABIHQW","AAABCBGD","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTAA" (
  "AAABEWQH" varchar(30) NOT NULL default ' ',
  "AAAAHTAU" varchar(32) NOT NULL default ' ',
  "AAABEWYR" varchar(30) NOT NULL default ' ',
  "AAABEWYT" varchar(30) NOT NULL default ' ',
  "AAABEWZC" varchar(30) NOT NULL default ' ',
  "AAABEWZD" varchar(30) NOT NULL default ' ',
  "AAABFOCV" varchar(1) NOT NULL default ' ',
  "AAABJETU" varchar(60) NOT NULL default ' ',
  "AAABFOCS" varchar(1) NOT NULL default ' ',
  "AAAAHTAK" varchar(4) NOT NULL default '0000',
  "AAAAHTAL" varchar(4) NOT NULL default '0000',
  "AAAAHTAM" varchar(1) NOT NULL default ' ',
  "AAAAHTAN" varchar(1) NOT NULL default ' ',
  "AAAAHTAO" varchar(1) NOT NULL default ' ',
  "AAABFOCY" varchar(1) NOT NULL default ' ',
  "AAABFOCZ" varchar(1) NOT NULL default ' ',
  "AAABFODA" varchar(1) NOT NULL default ' ',
  "AAABFODB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEWQH","AAAAHTAU","AAABEWYR","AAABEWYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTAX" (
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAAAHTBH" varchar(4) NOT NULL default ' ',
  "AAAAHTBI" varchar(1) NOT NULL default ' ',
  "AAAAHTBJ" varchar(2) NOT NULL default ' ',
  "AAAAHTBK" varchar(30) NOT NULL default ' ',
  "AAAAHTBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABIHQW","AAAAHTBH","AAAAHTBI","AAAAHTBJ","AAAAHTBK","AAAAHTBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTBM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHTBP" varchar(1) NOT NULL default ' ',
  "AAAAHTBQ" varchar(1) NOT NULL default ' ',
  "AAAAHTBR" varchar(1) NOT NULL default ' ',
  "AAAAHTBS" varchar(1) NOT NULL default ' ',
  "AAAAHTBT" varchar(1) NOT NULL default ' ',
  "AAAAHTBU" varchar(1) NOT NULL default ' ',
  "AAAAHTBV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTGT","AAAAHTCM","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTCM" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTCM","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMOHE" varchar(10) NOT NULL default ' ',
  "AAAAHTCX" varchar(20) NOT NULL default ' ',
  "AAAAHTCY" varchar(20) NOT NULL default ' ',
  "AAAAHTCZ" varchar(2) NOT NULL default '00',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAABMOHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHMLY" varchar(24) NOT NULL default ' ',
  "AAAAHTDH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBOBP" varchar(1) NOT NULL default ' ',
  "AAABBOBS" varchar(1) NOT NULL default ' ',
  "AAABBOBV" varchar(1) NOT NULL default ' ',
  "AAABBJXB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAAAHWFK" varchar(4) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIYQF","AAAAHWFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTDY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAAAHTEB" varchar(2) NOT NULL default ' ',
  "AAAAJIVB" varchar(1) NOT NULL default ' ',
  "AAAAHTED" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTEG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYQF","AAABKWJY","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTEP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABIYQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTGY" varchar(24) NOT NULL default ' ',
  "AAAAHTGZ" varchar(16) NOT NULL default ' ',
  "AAAAHTHA" varchar(10) NOT NULL default ' ',
  "AAAAHTHB" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHTHC" varchar(24) NOT NULL default ' ',
  "AAAAHTHD" varchar(16) NOT NULL default ' ',
  "AAAAHWRB" varchar(10) NOT NULL default ' ',
  "AAAAHTHF" varchar(16) NOT NULL default ' ',
  "AAAAHTGR" varchar(1) NOT NULL default ' ',
  "AAAAPMXC" varchar(1) NOT NULL default ' ',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAAAHTGU" varchar(10) NOT NULL default ' ',
  "AAAAHTGV" varchar(10) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTGY","AAAAHTGZ","AAAAHTHA","AAAAHTHB","AAABMCNU","AAABMDBB","AAAAHTHC","AAAAHTHD","AAAAHWRB","AAAAHTHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTGY" varchar(24) NOT NULL default ' ',
  "AAAAHTGZ" varchar(16) NOT NULL default ' ',
  "AAAAHTHA" varchar(10) NOT NULL default ' ',
  "AAAAHTHB" varchar(16) NOT NULL default ' ',
  "AAAAHTHC" varchar(24) NOT NULL default ' ',
  "AAAAHTHD" varchar(16) NOT NULL default ' ',
  "AAAAHWRB" varchar(10) NOT NULL default ' ',
  "AAAAHTHF" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAHTGR" varchar(1) NOT NULL default ' ',
  "AAAAPMXC" varchar(1) NOT NULL default ' ',
  "AAAAHTGT" varchar(24) NOT NULL default ' ',
  "AAAAHTGU" varchar(10) NOT NULL default ' ',
  "AAAAHTGV" varchar(10) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTGY","AAAAHTGZ","AAAAHTHA","AAAAHTHB","AAAAHTHC","AAAAHTHD","AAAAHWRB","AAAAHTHF","AAABMDBB","AAABMCNU","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAHTKR" varchar(21) NOT NULL default ' ',
  "AAAAHTKB" varchar(21) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAAAHTIE" decimal(13,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(13,2) NOT NULL default '0.00',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCSI" varchar(1) NOT NULL default ' ',
  "AAAAHTIK" varchar(8) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAAAHTKF" varchar(21) NOT NULL default ' ',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAAAMVOG" varchar(1) NOT NULL default ' ',
  "AAAAHTIU" varchar(2) NOT NULL default '00',
  "AAAAHTIV" varchar(2) NOT NULL default '00',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAANDRS" varchar(8) NOT NULL default '00000000',
  "AAAAHTIY" varchar(10) NOT NULL default ' ',
  "AAAAHTIZ" varchar(10) NOT NULL default ' ',
  "AAAAHTJA" varchar(10) NOT NULL default ' ',
  "AAAAHTJB" varchar(10) NOT NULL default ' ',
  "AAAAHTJC" varchar(10) NOT NULL default ' ',
  "AAAAHTJD" varchar(10) NOT NULL default ' ',
  "AAAAHTJE" decimal(13,2) NOT NULL default '0.00',
  "AAAAHTJF" decimal(13,2) NOT NULL default '0.00',
  "AAAAHTJG" varchar(1) NOT NULL default ' ',
  "AAAAHTJH" varchar(1) NOT NULL default ' ',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAAAHTJK" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKPXV","AAABMCZG","AAABKRXF","AAABLOLZ","AAABIHLK","AAABKRXN","AAABMREX","AAABMCPV","AAAAIBGY","AAAAHTKR","AAAAHTKB"),
  KEY "AAAAHTKC" ("AAABMRIL","AAABKRXE","AAAAHTKF"),
  KEY "AAAAHTKG" ("AAABMCVP","AAABKRXE","AAABMRIL"),
  KEY "AAAAHTKK" ("AAABIHLK","AAABLXTG","AAABKRXE"),
  KEY "AAAAHTKO" ("AAABMRIL","AAABKRXE","AAAAHTKR"),
  KEY "AAAAHTKS" ("AAABMCZN","AAABMCZO","AAABKRXE","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAIBQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(24) NOT NULL default ' ',
  "AAAAQKPP" varchar(24) NOT NULL default ' ',
  "AAAAQKPT" varchar(24) NOT NULL default ' ',
  "AAAAQKPX" varchar(24) NOT NULL default ' ',
  "AAAAQKQB" varchar(24) NOT NULL default ' ',
  "AAAAQKQF" varchar(24) NOT NULL default ' ',
  "AAAAHWDP" varchar(24) NOT NULL default ' ',
  "AAAAHWDQ" varchar(24) NOT NULL default ' ',
  "AAAAHWDR" varchar(24) NOT NULL default ' ',
  "AAAAHWDS" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAHTMF" varchar(50) NOT NULL default ' ',
  "AAAAHTMG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAIBQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTNA" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABHGYF" varchar(8) NOT NULL default '00000000',
  "AAABHGYG" varchar(6) NOT NULL default '000000',
  "AAAAHTMS" varchar(1) NOT NULL default '0',
  "AAAAHTMT" varchar(8) NOT NULL default '00000000',
  "AAAAHTMU" varchar(6) NOT NULL default '000000',
  "AAAAHTMV" varchar(1) NOT NULL default '0',
  "AAAAHTMW" varchar(1) NOT NULL default ' ',
  "AAAAHTMX" varchar(4) NOT NULL default '0000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWNG" varchar(10) NOT NULL default ' ',
  "AAAAHTPA" varchar(14) NOT NULL default ' ',
  "AAAAHWNF" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAHVLS" varchar(1) NOT NULL default '0',
  "AAAAHZBW" varchar(2) NOT NULL default '00',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAAAHZAR" varchar(1) NOT NULL default ' ',
  "AAAAHZAS" varchar(1) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHWNG","AAAAHTPA"),
  KEY "AAAAHTOJ" ("AAABIHLK","AAABMCNU","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWNG" varchar(10) NOT NULL default ' ',
  "AAAAHTPA" varchar(14) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAABLBGN" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAAHWNG","AAAAHTPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAHWLG" varchar(22) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAHTPM" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHTPO" varchar(2) NOT NULL default ' ',
  "AAAAHTPP" varchar(12) NOT NULL default ' ',
  "AAAAHTPQ" varchar(20) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAHVEJ" varchar(1) NOT NULL default ' ',
  "AAAAHTPX" varchar(15) NOT NULL default ' ',
  "AAABLZCP" varchar(35) NOT NULL default ' ',
  "AAABLZCR" varchar(35) NOT NULL default ' ',
  "AAABLVHG" varchar(35) NOT NULL default ' ',
  "AAABLVHH" varchar(35) NOT NULL default ' ',
  "AAABBZZZ" varchar(35) NOT NULL default ' ',
  "AAAAHTQD" varchar(10) NOT NULL default ' ',
  "AAABCAAD" varchar(35) NOT NULL default ' ',
  "AAABCAAE" varchar(35) NOT NULL default ' ',
  "AAAAHTQG" varchar(10) NOT NULL default ' ',
  "AAAAHTQH" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHTQL" varchar(15) NOT NULL default ' ',
  "AAAAHTQM" varchar(16) NOT NULL default ' ',
  "AAAAHTQN" varchar(16) NOT NULL default ' ',
  "AAAAHTQO" varchar(31) NOT NULL default ' ',
  "AAAAHTQP" varchar(30) NOT NULL default ' ',
  "AAABEGXK" varchar(30) NOT NULL default ' ',
  "AAAAHTQR" varchar(14) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYNP" varchar(10) NOT NULL default ' ',
  "AAAAHYOH" varchar(10) NOT NULL default ' ',
  "AAAAHYOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAANAVQ","AAABKGFD"),
  KEY "AAAAHTRA" ("AAAAHWLG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHVIE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMCZG","AAABMDBB","AAAAHVIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABIIWF" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSC" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSD" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSF" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSH" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSM" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSN" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHTSQ" decimal(15,2) NOT NULL default '0.00',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAAAYWIT" varchar(73) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCZF","AAABKRXK","AAABKRXF","AAABJKCJ","AAABMDBB","AAABLXTG","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAANAVQ","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAHTTV" varchar(10) NOT NULL default ' ',
  "AAAAHTTW" varchar(1) NOT NULL default ' ',
  "AAAAHZBW" varchar(2) NOT NULL default '00',
  "AAAAHVLS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  "AAAAHTUG" varchar(1) NOT NULL default ' ',
  "AAAAHTUH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTD","AAABLUFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABIAWZ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCVP","AAABMCVQ","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHTVK" varchar(10) NOT NULL default ' ',
  "AAAATVIB" varchar(8) NOT NULL default '00000000',
  "AAAAHTVF" varchar(10) NOT NULL default ' ',
  "AAABAXTI" decimal(13,2) NOT NULL default '0.00',
  "AAABAXTJ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHTVK","AAAATVIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAAAHTVP" varchar(1) NOT NULL default ' ',
  "AAAAHTVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDER" varchar(3) NOT NULL default ' ',
  "AAAAHTVX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTXE" varchar(4) NOT NULL default ' ',
  "AAAAHTWD" varchar(1) NOT NULL default ' ',
  "AAABJHFV" varchar(1) NOT NULL default ' ',
  "AAAAHTWF" varchar(4) NOT NULL default ' ',
  "AAAAHTWG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTXE" varchar(4) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTXE" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTXE","AAABMREL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHTXE" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHTXE","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(2) NOT NULL default ' ',
  "AAAALNRB" varchar(2) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(1) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAALNRA" varchar(10) NOT NULL default ' ',
  "AAAALNRB" varchar(10) NOT NULL default ' ',
  "AAAAKYFR" varchar(2) NOT NULL default ' ',
  "AAAAKYFS" varchar(2) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAAALNQU" varchar(1) NOT NULL default ' ',
  "AAAALNQW" varchar(1) NOT NULL default ' ',
  "AAAALNQX" varchar(12) NOT NULL default ' ',
  "AAAALNQY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAALNRA","AAAALNRB","AAAAKYFR","AAAAKYFS","AAABLARI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXN" varchar(2) NOT NULL default '00',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAABBWEJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXN","AAAAIEIA","AAABBWEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUEA" varchar(3) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAHTZN" varchar(1) NOT NULL default ' ',
  "AAAAIDRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUEA","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDPV" varchar(3) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAHTZN" varchar(1) NOT NULL default ' ',
  "AAAAHTZO" varchar(1) NOT NULL default ' ',
  "AAAAHTZP" varchar(1) NOT NULL default ' ',
  "AAAAHTZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLDPV","AAAAIEIA","AAAAHTZN","AAAAHTZO","AAAAHTZP","AAAAHTZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTXN" varchar(2) NOT NULL default '00',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTXN","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHTZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUBT" varchar(131) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAAAHUAC" varchar(1) NOT NULL default ' ',
  "AAABFWKK" varchar(3) NOT NULL default ' ',
  "AAAAQWFJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAHUAM" varchar(1) NOT NULL default ' ',
  "AAAAHUAN" varchar(1) NOT NULL default ' ',
  "AAAAHUAO" varchar(1) NOT NULL default ' ',
  "AAAAHUAP" varchar(1) NOT NULL default ' ',
  "AAAAHUAQ" varchar(1) NOT NULL default ' ',
  "AAAAHUAR" varchar(1) NOT NULL default ' ',
  "AAAAHUAS" varchar(1) NOT NULL default ' ',
  "AAAAHUAT" varchar(1) NOT NULL default ' ',
  "AAAAHUAU" varchar(1) NOT NULL default ' ',
  "AAAAHUAV" varchar(1) NOT NULL default ' ',
  "AAAAHUAW" varchar(1) NOT NULL default ' ',
  "AAAAHUAX" varchar(1) NOT NULL default ' ',
  "AAAAHUAY" varchar(1) NOT NULL default ' ',
  "AAAAHUAZ" varchar(1) NOT NULL default ' ',
  "AAAAHUBA" varchar(1) NOT NULL default ' ',
  "AAAAHUBB" varchar(1) NOT NULL default ' ',
  "AAAAHUBC" varchar(1) NOT NULL default ' ',
  "AAAAHUBD" varchar(1) NOT NULL default ' ',
  "AAAAHUBE" varchar(1) NOT NULL default ' ',
  "AAAAHUBF" varchar(1) NOT NULL default ' ',
  "AAAAHUBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCSO"),
  KEY "AAAAHUBK" ("AAABMRIL","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUBT" varchar(131) NOT NULL default ' ',
  "AAAAHUBU" varchar(3) NOT NULL default '000',
  "AAAAHUBR" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUBT","AAAAHUBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVUH" varchar(2) NOT NULL default ' ',
  "AAAAHVUI" varchar(2) NOT NULL default ' ',
  "AAAAHVUJ" varchar(4) NOT NULL default ' ',
  "AAAAHVUK" varchar(4) NOT NULL default ' ',
  "AAAAHVUL" varchar(3) NOT NULL default ' ',
  "AAAAHVUM" varchar(3) NOT NULL default ' ',
  "AAAAHUDC" varchar(3) NOT NULL default '000',
  "AAAAHUDD" varchar(3) NOT NULL default ' ',
  "AAAAHUCF" varchar(1) NOT NULL default ' ',
  "AAAAHUCG" varchar(1) NOT NULL default ' ',
  "AAAAHUCH" varchar(8) NOT NULL default ' ',
  "AAAAHUCI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHUCJ" decimal(18,2) NOT NULL default '0.00',
  "AAAAHUCK" varchar(2) NOT NULL default ' ',
  "AAAAHUCL" varchar(1) NOT NULL default ' ',
  "AAAAHUCM" varchar(1) NOT NULL default ' ',
  "AAAAHUCN" varchar(1) NOT NULL default ' ',
  "AAAAIDRU" varchar(1) NOT NULL default ' ',
  "AAAAHUCP" varchar(1) NOT NULL default ' ',
  "AAAAHUCQ" varchar(1) NOT NULL default ' ',
  "AAAAHUCR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVUH","AAAAHVUI","AAAAHVUJ","AAAAHVUK","AAAAHVUL","AAAAHVUM","AAAAHUDC","AAAAHUDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUDE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHUDT" varchar(16) NOT NULL default ' ',
  "AAAAHUDU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHUDT","AAAAHUDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUDL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAHUDT" varchar(16) NOT NULL default ' ',
  "AAAAHUDU" varchar(3) NOT NULL default ' ',
  "AAAAHUDQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAHUDT","AAAAHUDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUEA" varchar(3) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUEA","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUEI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHUEN" varchar(40) NOT NULL default ' ',
  "AAAAHUEO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAAAHUEN","AAAAHUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUEP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUEU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAHUFI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAIEIA","AAAAHUFI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUFB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAHUFI" varchar(10) NOT NULL default ' ',
  "AAAAVCFB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAIEIA","AAAAHUFI","AAAAVCFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUFK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAIEIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUHL" varchar(10) NOT NULL default ' ',
  "AAAAHUGA" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUHL"),
  UNIQUE KEY "AAAAHUFY" ("AAABMRIL","AAAAHUGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUHL" varchar(10) NOT NULL default ' ',
  "AAAAIDKS" varchar(1) NOT NULL default ' ',
  "AAAAIDKT" varchar(2) NOT NULL default ' ',
  "AAAAIDKU" varchar(2) NOT NULL default ' ',
  "AAAAIDKV" varchar(2) NOT NULL default ' ',
  "AAAAIDKW" varchar(2) NOT NULL default ' ',
  "AAAAIDKX" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUHL"),
  UNIQUE KEY "AAAAHUGP" ("AAABMRIL","AAAAIDKS","AAAAIDKT","AAAAIDKU","AAAAIDKV","AAAAIDKW","AAAAIDKX","AAAAIEIA","AAAAIEJA","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHUHL" varchar(10) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIDZC" varchar(2) NOT NULL default ' ',
  "AAAAIDZD" varchar(4) NOT NULL default ' ',
  "AAAAIDZE" varchar(4) NOT NULL default ' ',
  "AAAAIDZF" varchar(1) NOT NULL default ' ',
  "AAAAIDZG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHUHL"),
  UNIQUE KEY "AAAAHUHM" ("AAABMRIL","AAAAIEJA","AAAAJVGP","AAAAIDZC","AAAAIDZD","AAAAIDZE","AAAAIDZF","AAAAIDZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUHV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPNX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJARD" varchar(10) NOT NULL default ' ',
  "AAAAHULS" varchar(8) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJARD","AAAAHULS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJARD" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAHUIP" varchar(1) NOT NULL default ' ',
  "AAAAHUIQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJARD","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUIU" (
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABCQLS" varchar(30) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUIZ" (
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMHJ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUJF" (
  "AAAAHUKM" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEYPA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHUKM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUJK" (
  "AAAAHUKM" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHUKM","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJARD" varchar(10) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAAAWBOJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABJARD" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNJW","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHULS" varchar(8) NOT NULL default ' ',
  "AAAAHUKM" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAAAHUKO" varchar(32) NOT NULL default ' ',
  "AAAAHUKP" varchar(1) NOT NULL default ' ',
  "AAAAHUKQ" varchar(1) NOT NULL default ' ',
  "AAAAHUKR" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAHULS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHULS" varchar(8) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAHULK" varchar(4) NOT NULL default '0000',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAAAUNSO" varchar(45) NOT NULL default ' ',
  "AAAAUNSP" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHULS","AAABMREF","AAAAHULK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHULL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHULS" varchar(8) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  "AAAAHULQ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHULS","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHULU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAAAICYA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAABLPNX","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLTB" varchar(10) NOT NULL default ' ',
  "AAAAHUXC" varchar(40) NOT NULL default ' ',
  "AAABLPSU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLTB" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(6) NOT NULL default '000000',
  "AAABLJKC" varchar(6) NOT NULL default ' ',
  "AAABFPLD" varchar(2) NOT NULL default ' ',
  "AAAAHUMR" varchar(20) NOT NULL default ' ',
  "AAAAHUMS" varchar(20) NOT NULL default ' ',
  "AAAAHUMT" varchar(20) NOT NULL default ' ',
  "AAAAHUMU" varchar(20) NOT NULL default ' ',
  "AAAAHUMV" varchar(20) NOT NULL default ' ',
  "AAAAHUMW" varchar(20) NOT NULL default ' ',
  "AAAAHUMX" varchar(20) NOT NULL default ' ',
  "AAAAHUMY" varchar(20) NOT NULL default ' ',
  "AAAAHUMZ" varchar(20) NOT NULL default ' ',
  "AAAAHUNA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLTB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLTB" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLTB","AAABMRHS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUNO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYXW" varchar(4) NOT NULL default ' ',
  "AAAAHUNX" varchar(2) NOT NULL default ' ',
  "AAABIYVH" varchar(2) NOT NULL default ' ',
  "AAAAHUNZ" varchar(4) NOT NULL default ' ',
  "AAAAHUOA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYXW","AAAAHUNX","AAABIYVH","AAAAHUNZ","AAAAHUOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAAAHUOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWFPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABENNY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMOAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHUPI" varchar(8) NOT NULL default '00000000',
  "AAAAHUPJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  "AAAAHUQC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFPK","AAAAHUQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHUQC" varchar(8) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHUQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUQD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAVAPA" varchar(3) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABENNY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMREX","AAAAVAPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUQO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAKTTC" varchar(8) NOT NULL default ' ',
  "AAAAZMDG" varchar(20) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAVAPA" varchar(3) NOT NULL default ' ',
  "AAAAHUQV" varchar(8) NOT NULL default ' ',
  "AAAAHUQW" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAHUQY" varchar(8) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABEIJJ" varchar(8) NOT NULL default ' ',
  "AAAAHURC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMREX","AAAAKTTC","AAAAZMDG","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHURI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHURN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWFPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHURV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLFE" varchar(10) NOT NULL default ' ',
  "AAAAHURZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAAAHUSI" varchar(3) NOT NULL default '000',
  "AAAAHUSJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBB","AAABMCMO","AAABKGMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIMEG" varchar(12) NOT NULL default ' ',
  "AAAAHUST" varchar(12) NOT NULL default ' ',
  "AAABHFFQ" varchar(12) NOT NULL default ' ',
  "AAAAHUSV" varchar(12) NOT NULL default ' ',
  "AAAAHUSW" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIMEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUTB" (
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAAAQHMW" varchar(16) NOT NULL default ' ',
  "AAAAQHMX" varchar(24) NOT NULL default ' ',
  "AAABCPOB" varchar(4) NOT NULL default ' ',
  "AAAAIDRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREX","AAABGPZS","AAABKJYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHUTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWFK" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUUB" (
  "AAAAHUUE" varchar(4) NOT NULL default ' ',
  "AAAAHUUD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHUUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHUUS" varchar(1) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHUUU" varchar(10) NOT NULL default ' ',
  "AAAAHUUM" varchar(10) NOT NULL default ' ',
  "AAAAHUUN" varchar(10) NOT NULL default ' ',
  "AAAAHUUO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPMM","AAABKGOS","AAAAHUUS","AAAAHZZQ","AAAAHUUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUUV" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAHUVA" varchar(10) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHUVC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPMM","AAABCPNV","AAAAHZID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUVH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHUVK" varchar(4) NOT NULL default ' ',
  "AAAAHUVL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUVO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHUYO" varchar(10) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAABIYRN" varchar(8) NOT NULL default '00000000',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHUYO","AAABKGOS","AAABLPMM","AAABLPSK","AAABIYRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLTOL" varchar(40) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABILGE" varchar(1) NOT NULL default ' ',
  "AAABFOUX" varchar(100) NOT NULL default ' ',
  "AAABFOUW" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXKV","AAABLUHY","AAABMCVP","AAABMCVQ","AAABMREL","AAABLTOL","AAABMREF"),
  KEY "AAAAHUWW" ("AAABMCVP","AAABMCVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(16) NOT NULL default ' ',
  "AAAAHUXC" varchar(40) NOT NULL default ' ',
  "AAAAHUXD" varchar(2) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAHUXI" varchar(20) NOT NULL default ' ',
  "AAAAHUXJ" varchar(20) NOT NULL default ' ',
  "AAAAHUXK" varchar(20) NOT NULL default ' ',
  "AAAAHUXL" varchar(20) NOT NULL default ' ',
  "AAAAHUXM" varchar(20) NOT NULL default ' ',
  "AAAAHUXN" varchar(20) NOT NULL default ' ',
  "AAAAHUXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(16) NOT NULL default ' ',
  "AAABLJKC" varchar(6) NOT NULL default ' ',
  "AAAAHUYD" varchar(4) NOT NULL default ' ',
  "AAAAHUYE" varchar(50) NOT NULL default ' ',
  "AAABDEYE" varchar(2) NOT NULL default ' ',
  "AAABKGMO" decimal(17,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABLJKC","AAAAHUYD","AAAAHUYE","AAABDEYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUYG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHUYO" varchar(10) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAAAHUYR" varchar(6) NOT NULL default '000000',
  "AAAAHUYM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLUXQ","AAAAHUYO","AAABKGOS","AAABLPMM","AAAAHUYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUYS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHUYW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHUYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLTXI" varchar(6) NOT NULL default '000000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABMCVO" varchar(3) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAAAXMWI" varchar(2) NOT NULL default ' ',
  "AAAAXEKV" varchar(8) NOT NULL default '00000000',
  "AAABCRMW" varchar(3) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABMCVS" varchar(1) NOT NULL default ' ',
  "AAABLLZK" varchar(1) NOT NULL default ' ',
  "AAABKSPM" varchar(1) NOT NULL default ' ',
  "AAABKPOS" varchar(1) NOT NULL default ' ',
  "AAABKSPN" varchar(1) NOT NULL default ' ',
  "AAAAVZTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABLTXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHUZW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAAAHUZZ" varchar(1) NOT NULL default ' ',
  "AAAAHVAA" varchar(1) NOT NULL default ' ',
  "AAAAHVAB" varchar(1) NOT NULL default ' ',
  "AAAAHVAC" varchar(1) NOT NULL default ' ',
  "AAAAHVAD" varchar(1) NOT NULL default ' ',
  "AAAAHVAE" varchar(1) NOT NULL default ' ',
  "AAAAHVAF" varchar(1) NOT NULL default ' ',
  "AAAAHVAG" varchar(1) NOT NULL default ' ',
  "AAAAHVAH" varchar(1) NOT NULL default ' ',
  "AAAAHVAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(12) NOT NULL default ' ',
  "AAAAQKPP" varchar(12) NOT NULL default ' ',
  "AAAAQKPT" varchar(12) NOT NULL default ' ',
  "AAAAQKPX" varchar(12) NOT NULL default ' ',
  "AAAAQKQB" varchar(12) NOT NULL default ' ',
  "AAAAQKQF" varchar(12) NOT NULL default ' ',
  "AAAAHWDP" varchar(12) NOT NULL default ' ',
  "AAAAHWDQ" varchar(12) NOT NULL default ' ',
  "AAAAHWDR" varchar(12) NOT NULL default ' ',
  "AAAAHWDS" varchar(12) NOT NULL default ' ',
  "AAAAHVBB" varchar(12) NOT NULL default ' ',
  "AAAAHVBC" varchar(12) NOT NULL default ' ',
  "AAAAHVBD" varchar(12) NOT NULL default ' ',
  "AAAAHVBE" varchar(12) NOT NULL default ' ',
  "AAAAHVBF" varchar(12) NOT NULL default ' ',
  "AAAAHVBG" varchar(12) NOT NULL default ' ',
  "AAAAHVBH" varchar(12) NOT NULL default ' ',
  "AAAAHVBI" varchar(12) NOT NULL default ' ',
  "AAAAHVBJ" varchar(12) NOT NULL default ' ',
  "AAAAHVBK" varchar(12) NOT NULL default ' ',
  "AAAAHVBL" varchar(12) NOT NULL default ' ',
  "AAAAHVYZ" varchar(12) NOT NULL default ' ',
  "AAAAHVZA" varchar(12) NOT NULL default ' ',
  "AAAAHVBO" varchar(12) NOT NULL default ' ',
  "AAAAHVBP" varchar(12) NOT NULL default ' ',
  "AAAAHVBQ" varchar(12) NOT NULL default ' ',
  "AAAAHVBR" varchar(12) NOT NULL default ' ',
  "AAAAHVBS" varchar(12) NOT NULL default ' ',
  "AAAAHVBT" varchar(12) NOT NULL default ' ',
  "AAAAHVBU" varchar(12) NOT NULL default ' ',
  "AAAAHVBV" varchar(12) NOT NULL default ' ',
  "AAAAHVBW" varchar(12) NOT NULL default ' ',
  "AAAAHVBX" varchar(12) NOT NULL default ' ',
  "AAAAHVBY" varchar(12) NOT NULL default ' ',
  "AAAAHVBZ" varchar(12) NOT NULL default ' ',
  "AAAAHVCA" varchar(12) NOT NULL default ' ',
  "AAAAHVCB" varchar(12) NOT NULL default ' ',
  "AAAAHVCC" varchar(12) NOT NULL default ' ',
  "AAAAHVCD" varchar(12) NOT NULL default ' ',
  "AAAAHVCE" varchar(12) NOT NULL default ' ',
  "AAAAHVCF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIETU","AAAAHWDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAICOS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIETU","AAAAICOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIGO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIGO" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIGO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(10) NOT NULL default ' ',
  "AAAAHVDO" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABFIGO","AAAAHVDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  "AAAAHVDT" varchar(10) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAAAHVEA" varchar(1) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(6) NOT NULL default ' ',
  "AAAAHVEG" varchar(8) NOT NULL default '00000000',
  "AAAAHVEH" varchar(8) NOT NULL default '00000000',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAAAHVEJ" varchar(1) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYPR" varchar(10) NOT NULL default ' ',
  "AAAAHYQJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAJMCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAAAHVEX" varchar(14) NOT NULL default ' ',
  "AAABLEJW" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAAAHVEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAAJMCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVIE" varchar(20) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAAHVFQ" varchar(1) NOT NULL default ' ',
  "AAAAHVFR" varchar(1) NOT NULL default ' ',
  "AAAAHVFS" varchar(1) NOT NULL default ' ',
  "AAAAHVFT" varchar(1) NOT NULL default ' ',
  "AAAAHVFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHVIE","AAABMDBB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVIE" varchar(20) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHVIE","AAABMDBB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVIE" varchar(20) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHWNG" varchar(10) NOT NULL default ' ',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHVIE","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(12) NOT NULL default ' ',
  "AAAAQKPP" varchar(12) NOT NULL default ' ',
  "AAAAQKPT" varchar(12) NOT NULL default ' ',
  "AAAAQKPX" varchar(12) NOT NULL default ' ',
  "AAAAQKQB" varchar(12) NOT NULL default ' ',
  "AAAAQKQF" varchar(12) NOT NULL default ' ',
  "AAAAHWDP" varchar(12) NOT NULL default ' ',
  "AAAAHWDQ" varchar(12) NOT NULL default ' ',
  "AAAAHWDR" varchar(12) NOT NULL default ' ',
  "AAAAHWDS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVIE" varchar(20) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAAHVIE","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHVJA" varchar(12) NOT NULL default ' ',
  "AAAAHVIL" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHVIO" varchar(12) NOT NULL default ' ',
  "AAAAHVIP" varchar(12) NOT NULL default ' ',
  "AAAAHVIQ" varchar(12) NOT NULL default ' ',
  "AAAAHVIR" varchar(12) NOT NULL default ' ',
  "AAAAHVIS" varchar(12) NOT NULL default ' ',
  "AAAAHVIT" varchar(12) NOT NULL default ' ',
  "AAAAHVIU" varchar(12) NOT NULL default ' ',
  "AAAAHVIV" varchar(12) NOT NULL default ' ',
  "AAAAHVIW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU","AAAAHVJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(12) NOT NULL default ' ',
  "AAAAQKPP" varchar(12) NOT NULL default ' ',
  "AAAAQKPT" varchar(12) NOT NULL default ' ',
  "AAAAQKPX" varchar(12) NOT NULL default ' ',
  "AAAAQKQB" varchar(12) NOT NULL default ' ',
  "AAAAQKQF" varchar(12) NOT NULL default ' ',
  "AAAAHWDP" varchar(12) NOT NULL default ' ',
  "AAAAHWDQ" varchar(12) NOT NULL default ' ',
  "AAAAHWDR" varchar(12) NOT NULL default ' ',
  "AAAAHWDS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAAAQZIB" varchar(30) NOT NULL default ' ',
  "AAABELZJ" varchar(2) NOT NULL default '00',
  "AAAAHVKA" varchar(10) NOT NULL default ' ',
  "AAAAWHHO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVKT" varchar(10) NOT NULL default ' ',
  "AAAAHVKR" varchar(10) NOT NULL default ' ',
  "AAAAHVKP" varchar(10) NOT NULL default ' ',
  "AAAAHVKQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVKT" varchar(10) NOT NULL default ' ',
  "AAAAHVKP" varchar(10) NOT NULL default ' ',
  "AAAAHVKQ" varchar(10) NOT NULL default ' ',
  "AAAAHVKR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVLA" varchar(2) NOT NULL default '00',
  "AAAAIEUJ" varchar(2) NOT NULL default ' ',
  "AAABIKIG" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVLA","AAAAIEUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCFHW" varchar(40) NOT NULL default ' ',
  "AAABIKIG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCFHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAHZBW" varchar(2) NOT NULL default '00',
  "AAAAHVLS" varchar(1) NOT NULL default '0',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAABLUQG" varchar(1) NOT NULL default ' ',
  "AAAAHWNF" varchar(22) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  "AAAAHZAR" varchar(1) NOT NULL default ' ',
  "AAAAHZAS" varchar(1) NOT NULL default ' ',
  "AAABJAUE" varchar(2) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHVMF" varchar(1) NOT NULL default ' ',
  "AAAAMBXX" varchar(1) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAABLYAX" varchar(4) NOT NULL default ' ',
  "AAABKZYT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMCXH","AAABKGFD"),
  KEY "AAAAHVMO" ("AAABIHLK","AAABMCNU","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAABLBGN" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMCXH","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVNM" varchar(22) NOT NULL default ' ',
  "AAAAHVNN" varchar(1) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVNM","AAAAHVNN","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAAAHVNT" varchar(2) NOT NULL default '00',
  "AAAAHVNU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVPU","AAABJLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVNY" (
  "AAABFITE" varchar(4) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFITE","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVOD" (
  "AAABFITE" varchar(4) NOT NULL default ' ',
  "AAAAPYHA" varchar(1) NOT NULL default ' ',
  "AAAAHVOG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFITE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVPU","AAABJLLP","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  "AAAAHVPA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHVPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJLLP" varchar(4) NOT NULL default ' ',
  "AAAAHVPJ" varchar(5) NOT NULL default ' ',
  "AAAAHVPK" varchar(20) NOT NULL default ' ',
  "AAAAHVPL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHVPU","AAABMRHS","AAABJLLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHVPU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVPY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVRF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVQD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHVRF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABIYQF","AAABKWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVQL" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVRF" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVRF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVQT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVRX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVQY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVSQ" varchar(4) NOT NULL default ' ',
  "AAAAHVRF" varchar(4) NOT NULL default ' ',
  "AAAAHVRX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVSQ","AAAAHVRF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVRG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVRX" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAHVRK" varchar(1) NOT NULL default ' ',
  "AAAAHVRL" varchar(1) NOT NULL default ' ',
  "AAAAHVRM" varchar(1) NOT NULL default ' ',
  "AAABCILI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVRX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVRR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVRX" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVRX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVRZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVSQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVSQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVSE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHVSQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVSK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHVSQ" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHVSQ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVTC" varchar(10) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  "AAAAHVSZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHVTC","AAABJKCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAAAHVTI" varchar(1) NOT NULL default ' ',
  "AAAAHVTJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABKGVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHVTS" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABKGVD","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAHVUH" varchar(2) NOT NULL default ' ',
  "AAAAHVUI" varchar(2) NOT NULL default ' ',
  "AAAAHVUJ" varchar(4) NOT NULL default ' ',
  "AAAAHVUK" varchar(4) NOT NULL default ' ',
  "AAAAHVUL" varchar(3) NOT NULL default ' ',
  "AAAAHVUM" varchar(3) NOT NULL default ' ',
  "AAAAHVUN" varchar(1) NOT NULL default ' ',
  "AAAAHVUO" varchar(8) NOT NULL default ' ',
  "AAAAHVUP" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAIEBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAAIEBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCXH"),
  UNIQUE KEY "AAAAHVVK" ("AAABKRUI","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHVVQ" varchar(2) NOT NULL default '00',
  "AAAAHVVR" varchar(20) NOT NULL default ' ',
  "AAAAHVVS" varchar(1) NOT NULL default ' ',
  "AAAAHVVT" varchar(1) NOT NULL default ' ',
  "AAAAHVVU" varchar(1) NOT NULL default ' ',
  "AAAAHVVV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAHVWU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHVWV" decimal(15,2) NOT NULL default '0.00',
  "AAAAHVWW" decimal(15,2) NOT NULL default '0.00',
  "AAAAHVWX" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABIIKX","AAABIBEW","AAAAKZUL","AAABLMJN","AAAAHXGC","AAAAHXGD","AAAAHXGE","AAABJSBR","AAAAKZUN","AAABMQCA","AAABMQCB","AAABMDEN"),
  KEY "AAAAHVXP" ("AAABJKCJ","AAABMCZF","AAABKRUI","AAABCPWH","AAABKRUO"),
  KEY "AAAAHVXV" ("AAABMQCA","AAABMDEN","AAABMQCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVXZ" (
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAIBQU" varchar(12) NOT NULL default ' ',
  "AAAAIBQV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAIBQT","AAAAIBQU","AAAAIBQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAURZR" varchar(1) NOT NULL default ' ',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAHVYZ" varchar(50) NOT NULL default ' ',
  "AAAAHVZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU","AAAAURZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVZF" (
  "AAAAUHRO" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHVZQ" varchar(4) NOT NULL default '0000',
  "AAAAHVZJ" varchar(60) NOT NULL default ' ',
  "AAAAHVZK" varchar(60) NOT NULL default ' ',
  "AAAAHWAB" varchar(1) NOT NULL default ' ',
  "AAAAHWCC" varchar(50) NOT NULL default ' ',
  "AAAAHWCD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUHRO","AAAAIETU","AAAAHVZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHVZR" (
  "AAAAUHRO" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWAG" varchar(2) NOT NULL default '00',
  "AAAAZSTU" varchar(2) NOT NULL default '00',
  "AAAAHVZW" varchar(20) NOT NULL default ' ',
  "AAAAHVZX" varchar(50) NOT NULL default ' ',
  "AAAAHVZY" varchar(50) NOT NULL default ' ',
  "AAAAHVZZ" varchar(50) NOT NULL default ' ',
  "AAAAHWAA" varchar(50) NOT NULL default ' ',
  "AAAAHWAB" varchar(1) NOT NULL default ' ',
  "AAAAHWCC" varchar(50) NOT NULL default ' ',
  "AAAAHWCD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUHRO","AAAAIETU","AAAAHWAG","AAAAZSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICRP" varchar(10) NOT NULL default ' ',
  "AAABHACY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  "AAABMMJW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAICOQ","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(24) NOT NULL default ' ',
  "AAAAQKPP" varchar(24) NOT NULL default ' ',
  "AAAAQKPT" varchar(24) NOT NULL default ' ',
  "AAAAQKPX" varchar(24) NOT NULL default ' ',
  "AAAAQKQB" varchar(24) NOT NULL default ' ',
  "AAAAQKQF" varchar(24) NOT NULL default ' ',
  "AAAAHWDP" varchar(24) NOT NULL default ' ',
  "AAAAHWDQ" varchar(24) NOT NULL default ' ',
  "AAAAHWDR" varchar(24) NOT NULL default ' ',
  "AAAAHWDS" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAURZR" varchar(1) NOT NULL default ' ',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAHWCC" varchar(50) NOT NULL default ' ',
  "AAAAHWCD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU","AAAAURZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAABMMJW" varchar(12) NOT NULL default ' ',
  "AAAAHWCM" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWFD" varchar(12) NOT NULL default ' ',
  "AAAAHWES" varchar(12) NOT NULL default ' ',
  "AAAAHWET" varchar(12) NOT NULL default ' ',
  "AAAAHWEU" varchar(12) NOT NULL default ' ',
  "AAAAHWEV" varchar(12) NOT NULL default ' ',
  "AAAAHWEW" varchar(12) NOT NULL default ' ',
  "AAAAHWEX" varchar(12) NOT NULL default ' ',
  "AAAAHWEY" varchar(12) NOT NULL default ' ',
  "AAAAHWEZ" varchar(12) NOT NULL default ' ',
  "AAAAHWFA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIETU","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWDG" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWDJ" varchar(12) NOT NULL default ' ',
  "AAAAQKPP" varchar(12) NOT NULL default ' ',
  "AAAAQKPT" varchar(12) NOT NULL default ' ',
  "AAAAQKPX" varchar(12) NOT NULL default ' ',
  "AAAAQKQB" varchar(12) NOT NULL default ' ',
  "AAAAQKQF" varchar(12) NOT NULL default ' ',
  "AAAAHWDP" varchar(12) NOT NULL default ' ',
  "AAAAHWDQ" varchar(12) NOT NULL default ' ',
  "AAAAHWDR" varchar(12) NOT NULL default ' ',
  "AAAAHWDS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU","AAAAIETU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  "AAABMMJW" varchar(12) NOT NULL default ' ',
  "AAAAHWED" varchar(50) NOT NULL default ' ',
  "AAAAHWEE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAICOQ","AAABMMJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAHWFD" varchar(12) NOT NULL default ' ',
  "AAAAHWFE" varchar(20) NOT NULL default ' ',
  "AAABMAGQ" varchar(50) NOT NULL default ' ',
  "AAAAHWER" varchar(50) NOT NULL default ' ',
  "AAAAHWES" varchar(12) NOT NULL default ' ',
  "AAAAHWET" varchar(12) NOT NULL default ' ',
  "AAAAHWEU" varchar(12) NOT NULL default ' ',
  "AAAAHWEV" varchar(12) NOT NULL default ' ',
  "AAAAHWEW" varchar(12) NOT NULL default ' ',
  "AAAAHWEX" varchar(12) NOT NULL default ' ',
  "AAAAHWEY" varchar(12) NOT NULL default ' ',
  "AAAAHWEZ" varchar(12) NOT NULL default ' ',
  "AAAAHWFA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIETU","AAAAHWFD","AAAAHWFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWFF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAAAHWFK" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAABKIFQ" varchar(8) NOT NULL default '00000000',
  "AAABAEUS" varchar(8) NOT NULL default '00000000',
  "AAAAHWFQ" varchar(20) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  "AAABFBEW" varchar(32) NOT NULL default ' ',
  "AAAAHWFT" varchar(1) NOT NULL default ' ',
  "AAAAHYBB" varchar(6) NOT NULL default ' ',
  "AAABIYQF" varchar(4) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDRZ" varchar(4) NOT NULL default ' ',
  "AAAAIDSA" varchar(20) NOT NULL default ' ',
  "AAAAIDRX" varchar(1) NOT NULL default ' ',
  "AAAAHWGA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIWKF","AAABMQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWGF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAAAHWGM" varchar(1) NOT NULL default ' ',
  "AAAAHXUV" varchar(1) NOT NULL default '0',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHXWE" varchar(4) NOT NULL default '0000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIWOQ" varchar(16) NOT NULL default ' ',
  "AAAAIWOR" varchar(24) NOT NULL default ' ',
  "AAABANNN" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAAAHWHC" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHE" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHF" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHG" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHH" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHI" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHJ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHK" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHL" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHM" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHN" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHO" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHP" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHQ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHR" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHS" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHT" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHU" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHV" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHW" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHX" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHY" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWHZ" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIA" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIB" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIC" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWID" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIE" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIF" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIG" decimal(17,2) NOT NULL default '0.00',
  "AAAAHWIH" decimal(17,2) NOT NULL default '0.00',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAIWKF","AAABMQCJ","AAABMQCO","AAABMQHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAICOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  "AAAAHWJF" varchar(50) NOT NULL default ' ',
  "AAAAHWJG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAICOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAHWNN" varchar(24) NOT NULL default ' ',
  "AAAAHWNO" varchar(24) NOT NULL default ' ',
  "AAAAHWNP" varchar(24) NOT NULL default ' ',
  "AAAAHWNQ" varchar(24) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAIEJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAJVAY","AAABKRUI"),
  KEY "AAAAHWKH" ("AAABMCNU","AAAAJVAY","AAABMDBB","AAAAHWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHWLC" varchar(10) NOT NULL default ' ',
  "AAAAHWLG" varchar(22) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAVIRJ" varchar(22) NOT NULL default ' ',
  "AAAAHWKS" varchar(22) NOT NULL default ' ',
  "AAABGAPX" varchar(22) NOT NULL default ' ',
  "AAAAHWKU" varchar(22) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAHWLA" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHWLC","AAAAHWLG"),
  KEY "AAAAHWLE" ("AAABMRIL","AAAAHWLG","AAAAHWOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAHWNN" varchar(24) NOT NULL default ' ',
  "AAAAHWNO" varchar(24) NOT NULL default ' ',
  "AAAAHWNP" varchar(24) NOT NULL default ' ',
  "AAAAHWNQ" varchar(24) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAJVAY","AAABMDBB","AAABKRUI"),
  KEY "AAAAHWMD" ("AAABMCNU","AAAAJVAY","AAABMDBB","AAAAHWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHWNF" varchar(22) NOT NULL default ' ',
  "AAAAHWNG" varchar(10) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAAHWNE" varchar(8) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAHWMP" varchar(8) NOT NULL default ' ',
  "AAAAHWMQ" varchar(8) NOT NULL default ' ',
  "AAAAHWMR" varchar(8) NOT NULL default ' ',
  "AAAAHWMS" varchar(8) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHWNF","AAAAHWNG","AAABIHLK"),
  KEY "AAAAHWNC" ("AAABMRIL","AAAAHWNE","AAAAHWNF","AAAAHWNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAIEJV" varchar(1) NOT NULL default ' ',
  "AAAAHWNN" varchar(24) NOT NULL default ' ',
  "AAAAHWNO" varchar(24) NOT NULL default ' ',
  "AAAAHWNP" varchar(24) NOT NULL default ' ',
  "AAAAHWNQ" varchar(24) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  "AAAALYMH" varchar(12) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAABIRRR" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAJVAY","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWPE" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAAAHWOF" varchar(16) NOT NULL default ' ',
  "AAAAHWOG" varchar(16) NOT NULL default ' ',
  "AAAAHWOH" varchar(16) NOT NULL default ' ',
  "AAAAHWOI" varchar(16) NOT NULL default ' ',
  "AAAAHWOJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHWPE","AAABMCZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWPE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAHWPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHWPE" varchar(4) NOT NULL default ' ',
  "AAAAHWPA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAAAHWPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAAAHWPI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAAAHWPP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQDTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHWQL" varchar(10) NOT NULL default ' ',
  "AAAAHWPW" varchar(2) NOT NULL default ' ',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHWQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHWQL" varchar(10) NOT NULL default ' ',
  "AAABMEDK" varchar(40) NOT NULL default ' ',
  "AAAAHWQF" varchar(4) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  "AAABMJVF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHWQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWQM" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAHWRB" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAHXTS" varchar(2) NOT NULL default ' ',
  "AAAAHXTT" varchar(4) NOT NULL default ' ',
  "AAAAHXUC" varchar(4) NOT NULL default '0000',
  "AAAAHXNO" varchar(10) NOT NULL default ' ',
  "AAAAHXTU" varchar(1) NOT NULL default ' ',
  "AAAAHXTX" varchar(1) NOT NULL default ' ',
  "AAAAHXNR" varchar(22) NOT NULL default ' ',
  "AAAAHXTV" varchar(4) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(3) NOT NULL default '000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHWSN" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAAAHWSV" ("AAAAICIH","AAAAIEJA"),
  KEY "AAAAHWSY" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHWTS" varchar(1) NOT NULL default ' ',
  "AAAAHWTT" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAHWTQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHWTS","AAAAHWTT","AAABMCNU","AAABMCZF","AAABKRUI","AAABJKCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHWUF" ("AAABMRIL","AAABMQDU","AAABKRUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAKIFV" varchar(10) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU"),
  KEY "AAAAHWUV" ("AAAAKIFV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAHXED" decimal(13,2) NOT NULL default '0.00',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAAAHWVL" varchar(3) NOT NULL default '000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAAAHWVQ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAAAIBYE","AAABMCNU"),
  KEY "AAAAHWVV" ("AAABMCXH","AAABMCZF","AAABJKCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCRD" varchar(4) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAAAHWWP" varchar(1) NOT NULL default ' ',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEU" varchar(4) NOT NULL default ' ',
  "AAABIBEV" varchar(4) NOT NULL default '0000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU"),
  KEY "AAAAHWWX" ("AAABKJVG","AAABMQCE","AAABMQCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHWXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAANDRS" varchar(8) NOT NULL default '00000000',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAHXEA" varchar(2) NOT NULL default '00',
  "AAAAHXSZ" varchar(2) NOT NULL default '00',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAHXED" decimal(13,2) NOT NULL default '0.00',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAQGXS" varchar(16) NOT NULL default ' ',
  "AAAAHXEV" varchar(20) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAAAYXBH" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAAAHXAM" varchar(4) NOT NULL default ' ',
  "AAAAHXAN" varchar(4) NOT NULL default '0000',
  "AAAAHXAL" varchar(10) NOT NULL default ' ',
  "AAAAHWYQ" varchar(3) NOT NULL default '000',
  "AAAAIBYM" varchar(4) NOT NULL default '0000',
  "AAAAIBYL" varchar(10) NOT NULL default ' ',
  "AAAAHWYT" varchar(3) NOT NULL default '000',
  "AAABMCTE" decimal(13,2) NOT NULL default '0.00',
  "AAABMCWR" decimal(13,2) NOT NULL default '0.00',
  "AAAAHWYW" varchar(1) NOT NULL default ' ',
  "AAAAQEAA" varchar(8) NOT NULL default '00000000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAHXGR" varchar(3) NOT NULL default '000',
  "AAAAHXGQ" varchar(10) NOT NULL default ' ',
  "AAAAHXFI" varchar(10) NOT NULL default ' ',
  "AAAAHXFJ" varchar(5) NOT NULL default '00000',
  "AAAAHXFK" varchar(5) NOT NULL default '00000',
  "AAAAHXFL" varchar(5) NOT NULL default ' ',
  "AAAAHXFM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU","AAAAIBYE","AAAAXZDS","AAABMQHV","AAABMDBB","AAABLTXE"),
  KEY "AAAAHWZQ" ("AAABMRIL","AAAAQEAA","AAABMREX","AAABKRXF"),
  KEY "AAAAHWZV" ("AAABJKCJ","AAABMCZF","AAABKRUI"),
  KEY "AAAAHWZZ" ("AAABMRIL","AAAAIBYL","AAAAIBYM","AAABMREX"),
  KEY "AAAAHXAE" ("AAAAHXGQ","AAAAHXGR"),
  KEY "AAAAHXAH" ("AAAANDRS"),
  KEY "AAAAHXAJ" ("AAABMRIL","AAAAHXAL","AAAAHXAM","AAAAHXAN"),
  KEY "AAAAHXAO" ("AAABMRIL","AAABMCNU","AAABMDBB","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHXAX" varchar(10) NOT NULL default '0000000000',
  "AAAAHXAY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXBC" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLUSV","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXBK" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLUSV" varchar(3) NOT NULL default ' ',
  "AAABLLNS" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHXCQ" ("AAABMCZF","AAABJKCJ","AAABMREX","AAABMRIL","AAABKRXF","AAAAXZDS","AAABCCXW","AAABMOAH","AAABJSBE","AAAAIBGY","AAAAQDNR","AAABJGDL","AAABKRUI","AAAAIUQC","AAABCPWH","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAAAHXGC" varchar(5) NOT NULL default '00000',
  "AAAAHXGD" varchar(4) NOT NULL default '0000',
  "AAAAHXGE" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAANDRS" varchar(8) NOT NULL default '00000000',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAHXEA" varchar(2) NOT NULL default '00',
  "AAAAHXSZ" varchar(2) NOT NULL default '00',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAHXED" decimal(13,2) NOT NULL default '0.00',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAQGXS" varchar(16) NOT NULL default ' ',
  "AAAAHXEV" varchar(20) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  "AAAAYXBH" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABJRZB" varchar(1) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAHXFE" varchar(4) NOT NULL default ' ',
  "AAAAHXFF" varchar(1) NOT NULL default ' ',
  "AAAAHXGR" varchar(3) NOT NULL default '000',
  "AAAAHXGQ" varchar(10) NOT NULL default ' ',
  "AAAAHXFI" varchar(10) NOT NULL default ' ',
  "AAAAHXFJ" varchar(5) NOT NULL default '00000',
  "AAAAHXFK" varchar(5) NOT NULL default '00000',
  "AAAAHXFL" varchar(5) NOT NULL default ' ',
  "AAAAHXFM" varchar(10) NOT NULL default ' ',
  "AAAAHXFN" varchar(1) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAHXFS" varchar(8) NOT NULL default '00000000',
  "AAAAHXFT" decimal(13,3) NOT NULL default '0.000',
  "AAAAHXFU" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIKX","AAABIBEW","AAAAKZUL","AAABLMJN","AAAAHXGC","AAAAHXGD","AAAAHXGE","AAABJSBR","AAAAKZUN","AAAAXZDS","AAABMQHV","AAABMDBB","AAABLTXE"),
  KEY "AAAAHXGL" ("AAABJKCJ","AAABKRUI","AAABMCZF"),
  KEY "AAAAHXGP" ("AAAAHXGQ","AAAAHXGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIKX" varchar(10) NOT NULL default ' ',
  "AAABIBEW" varchar(3) NOT NULL default '000',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAABLMJN" varchar(5) NOT NULL default '00000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIKX","AAABIBEW","AAAAKZUL","AAABLMJN","AAABJSBR","AAAAKZUN","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXHM" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(3) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAHXTS" varchar(2) NOT NULL default ' ',
  "AAAAHXTT" varchar(4) NOT NULL default ' ',
  "AAAAHXUC" varchar(4) NOT NULL default '0000',
  "AAAAHXNO" varchar(10) NOT NULL default ' ',
  "AAAAHXTU" varchar(1) NOT NULL default ' ',
  "AAAAHXTX" varchar(1) NOT NULL default ' ',
  "AAAAHXNR" varchar(22) NOT NULL default ' ',
  "AAAAHXTV" varchar(4) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
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
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQCW" varchar(4) NOT NULL default '0000',
  "AAABMQDA" varchar(3) NOT NULL default '000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQBZ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHXKP" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAHXKY" ("AAAAIEJA"),
  KEY "AAAAHXLA" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHXLK" varchar(1) NOT NULL default ' ',
  "AAAAHXMK" varchar(1) NOT NULL default ' ',
  "AAAAHXML" varchar(8) NOT NULL default '00000000',
  "AAAAHXMM" varchar(1) NOT NULL default ' ',
  "AAAAHXMN" varchar(8) NOT NULL default '00000000',
  "AAAAHXMO" varchar(1) NOT NULL default ' ',
  "AAAAHXMQ" varchar(1) NOT NULL default ' ',
  "AAAAHXMR" varchar(8) NOT NULL default '00000000',
  "AAAAHXMH" varchar(1) NOT NULL default ' ',
  "AAAAHXMI" varchar(1) NOT NULL default ' ',
  "AAAAHXMJ" varchar(1) NOT NULL default ' ',
  "AAAAHXMS" varchar(1) NOT NULL default ' ',
  "AAAAHXLW" varchar(1) NOT NULL default ' ',
  "AAAAHXMT" varchar(1) NOT NULL default ' ',
  "AAAAHXLY" varchar(1) NOT NULL default ' ',
  "AAAAHXLZ" varchar(1) NOT NULL default ' ',
  "AAAAHXMA" varchar(1) NOT NULL default ' ',
  "AAAAHXMB" varchar(1) NOT NULL default ' ',
  "AAAAHXMC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHXMH" varchar(1) NOT NULL default ' ',
  "AAAAHXMI" varchar(1) NOT NULL default ' ',
  "AAAAHXMJ" varchar(1) NOT NULL default ' ',
  "AAAAHXMK" varchar(1) NOT NULL default ' ',
  "AAAAHXML" varchar(8) NOT NULL default '00000000',
  "AAAAHXMM" varchar(1) NOT NULL default ' ',
  "AAAAHXMN" varchar(8) NOT NULL default '00000000',
  "AAAAHXMO" varchar(1) NOT NULL default ' ',
  "AAAAHXMP" varchar(8) NOT NULL default '00000000',
  "AAAAHXMQ" varchar(1) NOT NULL default ' ',
  "AAAAHXMR" varchar(8) NOT NULL default '00000000',
  "AAAAHXMS" varchar(1) NOT NULL default ' ',
  "AAAAHXMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXMW" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAHXTS" varchar(2) NOT NULL default ' ',
  "AAAAHXTT" varchar(4) NOT NULL default ' ',
  "AAAAHXUC" varchar(4) NOT NULL default '0000',
  "AAAAHXNO" varchar(10) NOT NULL default ' ',
  "AAAAHXTU" varchar(1) NOT NULL default ' ',
  "AAAAHXTX" varchar(1) NOT NULL default ' ',
  "AAAAHXNR" varchar(22) NOT NULL default ' ',
  "AAAAHXTV" varchar(4) NOT NULL default ' ',
  "AAAAHXNT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAHXNV" varchar(24) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHS"),
  KEY "AAAAHXPT" ("AAABMQHU","AAABMQHV","AAABMQHY","AAABMQDO","AAABMQHW","AAABMQHX"),
  KEY "AAAAHXQA" ("AAABMQHU","AAABMCNU","AAAAIEGF"),
  KEY "AAAAHXQE" ("AAABMQHU","AAABMCNU","AAAAICIH","AAAAIEJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAZRYR" varchar(1) NOT NULL default ' ',
  "AAAAHXQN" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAAAHXQR" varchar(1) NOT NULL default ' ',
  "AAAAHXQS" varchar(10) NOT NULL default ' ',
  "AAAAHXQT" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAZRYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAWDEM" varchar(24) NOT NULL default ' ',
  "AAABDMCO" varchar(2) NOT NULL default ' ',
  "AAABKGOS" varchar(4) NOT NULL default '0000',
  "AAAAHXRO" varchar(2) NOT NULL default ' ',
  "AAABMREM" varchar(2) NOT NULL default ' ',
  "AAAAKLVZ" varchar(4) NOT NULL default ' ',
  "AAAAHXRH" varchar(3) NOT NULL default '000',
  "AAAAHXRI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAAAWDEM","AAABDMCO","AAABKGOS","AAAAHXRO","AAABMREM","AAAAKLVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAAARHOV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSAK","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAQDUH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCC","AAAAQDUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXSG" (
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAAARXMN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXSM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABBOTQ" varchar(15) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAAAHXWI" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABCPTL" varchar(4) NOT NULL default ' ',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(3) NOT NULL default ' ',
  "AAAAHXSZ" varchar(2) NOT NULL default ' ',
  "AAAAQDNR" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAAAHXTY" varchar(1) NOT NULL default ' ',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAAAIUQC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABBOTQ","AAAARXMN","AAAAHXWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXTM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABBOTQ" varchar(15) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAAAHXWI" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAHXTS" varchar(2) NOT NULL default ' ',
  "AAAAHXTT" varchar(4) NOT NULL default ' ',
  "AAAAHXTU" varchar(1) NOT NULL default ' ',
  "AAAAHXTV" varchar(4) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAAAHXTX" varchar(1) NOT NULL default ' ',
  "AAAAHXTY" varchar(1) NOT NULL default ' ',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(3) NOT NULL default ' ',
  "AAAAKZPO" varchar(4) NOT NULL default ' ',
  "AAAAHXUC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABBOTQ","AAAARXMN","AAAAHXWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXUH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABBOTQ" varchar(15) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAAAHXWI" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAHXVJ" varchar(1) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default ' ',
  "AAAAHXWE" varchar(4) NOT NULL default ' ',
  "AAAAHXUV" varchar(1) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABBOTQ","AAAARXMN","AAAAHXWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXVB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABBOTQ" varchar(15) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAAAHXWI" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAHXVH" varchar(2) NOT NULL default ' ',
  "AAAAHXVI" varchar(1) NOT NULL default ' ',
  "AAAAHXVJ" varchar(1) NOT NULL default ' ',
  "AAAAHXVK" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default ' ',
  "AAAAHXWE" varchar(4) NOT NULL default ' ',
  "AAABLUTZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABBOTQ","AAAARXMN","AAAAHXWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXVS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABBOTQ" varchar(15) NOT NULL default ' ',
  "AAAARXMN" varchar(4) NOT NULL default ' ',
  "AAAAHXWI" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default ' ',
  "AAAAHXWE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABBOTQ","AAAARXMN","AAAAHXWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAHZPX" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABIHLK","AAABKRXF","AAABMDBB","AAABMCZG","AAABKRXK","AAABLOLZ","AAABKRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAABAMAD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWKKC","AAABAMAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHZAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAAAHXYB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZAN","AAAAWKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAABAMAD" varchar(10) NOT NULL default ' ',
  "AAAAHXYK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKKC","AAABAMAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAABMLZG" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAWKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAAAHXYZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAWKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZHB" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHXZH" varchar(3) NOT NULL default '000',
  "AAAAHXZI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDZHB","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHXZM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHYBB" varchar(6) NOT NULL default ' ',
  "AAAAHXZP" varchar(1) NOT NULL default ' ',
  "AAAAHXZQ" varchar(1) NOT NULL default ' ',
  "AAAAHXZR" varchar(1) NOT NULL default ' ',
  "AAAAHXZS" varchar(1) NOT NULL default ' ',
  "AAAAHXZT" varchar(1) NOT NULL default ' ',
  "AAAAHXZU" varchar(1) NOT NULL default ' ',
  "AAAAHXZV" varchar(1) NOT NULL default ' ',
  "AAAAHXZW" varchar(1) NOT NULL default ' ',
  "AAAAHXZX" varchar(1) NOT NULL default ' ',
  "AAAAHXZY" varchar(1) NOT NULL default ' ',
  "AAAAIVRT" varchar(4) NOT NULL default ' ',
  "AAAAHYAA" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAB" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAC" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAD" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAE" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAF" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAG" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAI" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAJ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAK" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAL" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAM" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAN" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHYAR" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLUXQ","AAAAHYBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYAU" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHYBB" varchar(6) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAAAHYBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default '0000',
  "AAABDZWZ" varchar(30) NOT NULL default ' ',
  "AAABDHON" varchar(30) NOT NULL default ' ',
  "AAAAXSVS" varchar(2) NOT NULL default '00',
  "AAAAXSVT" varchar(2) NOT NULL default '00',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABCWSJ" varchar(2) NOT NULL default ' ',
  "AAAAHYCY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLXTW","AAABJRHX","AAAAWFCG","AAAAWHKE","AAABFMBR","AAABDZWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKJAF" varchar(2) NOT NULL default ' ',
  "AAAAHYCE" varchar(2) NOT NULL default ' ',
  "AAAAHYCF" varchar(2) NOT NULL default ' ',
  "AAAAHYCG" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKJAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAWHKE" varchar(1) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMMJT" decimal(16,2) NOT NULL default '0.00',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAAAXSVS" varchar(2) NOT NULL default '00',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABCWSJ" varchar(2) NOT NULL default ' ',
  "AAAAHYCY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLXTW","AAABJRHX","AAAAWFCG","AAAAWHKE","AAABLXDR","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDPN" varchar(8) NOT NULL default ' ',
  "AAABLLZQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAQDPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMBR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMBR" varchar(4) NOT NULL default '0000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMBR","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABJWTL" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAAQDPN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABJWTL" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABJWTL" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABJWTL" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKPXV","AAABJWRR","AAABMGVL","AAABMGVM","AAABMCZN","AAABMCZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHYHV" varchar(10) NOT NULL default '0000000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAHYHV","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYHX" (
  "AAAAXSVS" varchar(2) NOT NULL default '00',
  "AAAAXSVT" varchar(2) NOT NULL default '00',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXSVS","AAAAXSVT","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYIE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXSVS" varchar(2) NOT NULL default '00',
  "AAAAXSVT" varchar(2) NOT NULL default '00',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAXSVS","AAAAXSVT","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAHYIT" varchar(3) NOT NULL default ' ',
  "AAAAHYIU" varchar(40) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHYIY" int(11) NOT NULL default '0',
  "AAAAHYIZ" int(11) NOT NULL default '0',
  "AAAAHYJA" int(11) NOT NULL default '0',
  "AAAAHYJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBB","AAABMREX","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHYUD" varchar(1) NOT NULL default ' ',
  "AAAAHYJJ" varchar(1) NOT NULL default ' ',
  "AAAAHYJK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHYUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYKD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYKD" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYMT" varchar(50) NOT NULL default ' ',
  "AAAAHYMU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYKD","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYKV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYKV" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYMT" varchar(50) NOT NULL default ' ',
  "AAAAHYMU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYKV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYLN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYLN" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYMT" varchar(50) NOT NULL default ' ',
  "AAAAHYMU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYLN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYMF" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYMT" varchar(50) NOT NULL default ' ',
  "AAAAHYMU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYMF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYMX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYMX" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYMT" varchar(50) NOT NULL default ' ',
  "AAAAHYMU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYMX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYNP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYNP" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYOV" varchar(50) NOT NULL default ' ',
  "AAAAHYOW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYNP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYOH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYOH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYOH" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYOV" varchar(50) NOT NULL default ' ',
  "AAAAHYOW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYOH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYOZ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYOV" varchar(50) NOT NULL default ' ',
  "AAAAHYOW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYOZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYPR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYPR" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYSH" varchar(50) NOT NULL default ' ',
  "AAAAHYSI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYPR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYQJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAHYQJ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYSH" varchar(50) NOT NULL default ' ',
  "AAAAHYSI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAAAHYQJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPOZ" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYSH" varchar(50) NOT NULL default ' ',
  "AAAAHYSI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPOZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPPA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPPA" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYSH" varchar(50) NOT NULL default ' ',
  "AAAAHYSI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPPA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPPB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPPB" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHYSH" varchar(50) NOT NULL default ' ',
  "AAAAHYSI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPOY","AAABEPPB","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAHYTU" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAAAHYTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAHYTU" varchar(11) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAHYSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAAAHYTU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHYUD" varchar(1) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHYUD","AAABEPOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAHYTU" varchar(11) NOT NULL default ' ',
  "AAAAHYTQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMOHM","AAAAHYTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHYUD" varchar(1) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAAAVCJB" varchar(4) NOT NULL default ' ',
  "AAAAHYUA" varchar(2) NOT NULL default '00',
  "AAAAKTOZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAHYUD","AAABEPOY","AAAAVCJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYUG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHYVH" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABCSRR" varchar(1) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAABKNAR" varchar(8) NOT NULL default '00000000',
  "AAABLEBK" varchar(6) NOT NULL default '000000',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAAAIVII" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAAAHYVH","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYUX" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAHYVH" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABIYBO" varchar(20) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  "AAAAXFUI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLFZK","AAAAHYVH","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYVJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default '0000000000',
  "AAABMDBA" varchar(10) NOT NULL default '0000000000',
  "AAAAVFTT" varchar(3) NOT NULL default ' ',
  "AAABFSDE" varchar(3) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHYWE" varchar(10) NOT NULL default ' ',
  "AAAAHYVS" varchar(1) NOT NULL default ' ',
  "AAAAHYVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQCJ","AAABMDBA","AAAAVFTT","AAABFSDE","AAABCPQF"),
  KEY "AAAAHYWA" ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHYWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYWF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAVFTT" varchar(3) NOT NULL default ' ',
  "AAABFSDE" varchar(3) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHYWM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKJYV","AAAAVFTT","AAABFSDE","AAABIWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYWT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(10) NOT NULL default '0000000000',
  "AAABMDBA" varchar(10) NOT NULL default '0000000000',
  "AAAAVFTT" varchar(3) NOT NULL default ' ',
  "AAABFSDE" varchar(3) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMQCJ","AAABMDBA","AAAAVFTT","AAABFSDE","AAABCPQF","AAABIWLC","AAABKJYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABLHRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAAAHYXV" varchar(1) NOT NULL default ' ',
  "AAABLYUA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYXZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAUIMI" varchar(3) NOT NULL default '000',
  "AAAAUIMJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHZIB","AAABLTXI","AAABIWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYYM" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAUIMI" varchar(3) NOT NULL default '000',
  "AAAAUIMJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAAAHZIB","AAABLTXI","AAABIWLC","AAABKJYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYZB" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKYTL" varchar(5) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLTVR" varchar(22) NOT NULL default ' ',
  "AAABMDXP" varchar(64) NOT NULL default ' ',
  "AAABHHAD" varchar(64) NOT NULL default ' ',
  "AAABCTLF" int(11) NOT NULL default '0',
  "AAAAUWQD" varchar(22) NOT NULL default ' ',
  "AAAAUUXW" varchar(1) NOT NULL default ' ',
  "AAABEZGK" varchar(64) NOT NULL default ' ',
  "AAAAUUXY" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMDEN","AAABKYTL","AAABCPQF","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHYZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWEXJ" varchar(4) NOT NULL default ' ',
  "AAAAHYZX" varchar(12) NOT NULL default ' ',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJAUE" varchar(2) NOT NULL default ' ',
  "AAAAHZAL" varchar(1) NOT NULL default ' ',
  "AAAAHZAM" varchar(1) NOT NULL default ' ',
  "AAAAHZAN" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAAAHZAP" varchar(1) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  "AAAAHZAR" varchar(1) NOT NULL default ' ',
  "AAAAHZAS" varchar(1) NOT NULL default ' ',
  "AAABLXAB" varchar(1) NOT NULL default ' ',
  "AAAAHZAU" varchar(1) NOT NULL default ' ',
  "AAAAHZAV" varchar(1) NOT NULL default ' ',
  "AAAAHZAW" varchar(4) NOT NULL default ' ',
  "AAAAOSPJ" varchar(16) NOT NULL default ' ',
  "AAAAHZAY" varchar(1) NOT NULL default ' ',
  "AAAAHZAZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAJVAY","AAABMDBB","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKRUI"),
  KEY "AAAAHZBN" ("AAABMCNU","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABFTFQ" varchar(1) NOT NULL default '0',
  "AAAAHZBW" varchar(2) NOT NULL default '00',
  "AAABHXDH" varchar(1) NOT NULL default '0',
  "AAAAWKKC" varchar(5) NOT NULL default ' ',
  "AAABKPVV" varchar(3) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAAAHZCB" varchar(24) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAAAHZCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAJVAY","AAABKRUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAHZCR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABABEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMAHQ" varchar(20) NOT NULL default ' ',
  "AAABMJVD" varchar(50) NOT NULL default ' ',
  "AAABMJVE" varchar(50) NOT NULL default ' ',
  "AAABMJVF" varchar(50) NOT NULL default ' ',
  "AAAAHZDF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABABEW","AAAAJVAY","AAABKRUI"),
  KEY "AAAAHZDL" ("AAABMRIL","AAABABEW","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZDP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZDS" varchar(5) NOT NULL default ' ',
  "AAAAHZDT" varchar(1) NOT NULL default ' ',
  "AAAAHZDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIZJ" varchar(4) NOT NULL default ' ',
  "AAAAHZEA" varchar(1) NOT NULL default ' ',
  "AAABLCFN" varchar(2) NOT NULL default ' ',
  "AAAAHZEC" decimal(7,3) NOT NULL default '0.000',
  "AAAAHZED" varchar(1) NOT NULL default ' ',
  "AAAAQCXL" varchar(1) NOT NULL default ' ',
  "AAABCLSH" varchar(2) NOT NULL default ' ',
  "AAAAHZEG" varchar(1) NOT NULL default ' ',
  "AAAAHZEH" varchar(1) NOT NULL default ' ',
  "AAAAHZEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJIZJ" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHZEP" decimal(13,2) NOT NULL default '0.00',
  "AAAAHZEQ" varchar(6) NOT NULL default '000000',
  "AAAAQCYE" varchar(6) NOT NULL default '000000',
  "AAAAQCYD" varchar(1) NOT NULL default ' ',
  "AAAAHZET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJIZJ","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZEX" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJIZJ" varchar(4) NOT NULL default ' ',
  "AAAAHZFB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABMRHS","AAABJIZJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAAALRKB" varchar(20) NOT NULL default ' ',
  "AAAAQDPN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAAALRKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAALRKB" varchar(20) NOT NULL default ' ',
  "AAAAQDPN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCSR","AAAALRKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI","AAABMQCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAABMOHD" varchar(20) NOT NULL default ' ',
  "AAAAIBKS" varchar(50) NOT NULL default ' ',
  "AAAAIBKT" varchar(50) NOT NULL default ' ',
  "AAAAQEAA" varchar(8) NOT NULL default '00000000',
  "AAAAQDRK" varchar(8) NOT NULL default '00000000',
  "AAABCLSH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI","AAABMCNU","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCC","AAAAQDTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZHD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHZHJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKWJY","AAAAHZID","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZHP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  "AAAAHZIB" varchar(2) NOT NULL default ' ',
  "AAABKWJY" varchar(4) NOT NULL default ' ',
  "AAAAHZID" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAABLNZH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABMRFO","AAAAHZIB","AAABKWJY","AAAAHZID","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZIY" varchar(10) NOT NULL default ' ',
  "AAAAHZII" varchar(1) NOT NULL default ' ',
  "AAAAHZIJ" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAHZIL" varchar(2) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAAAHZIN" varchar(1) NOT NULL default ' ',
  "AAABMCTH" varchar(1) NOT NULL default ' ',
  "AAABILMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZIY" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHZIW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZIY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZJC" varchar(1) NOT NULL default ' ',
  "AAAAHZJD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZJF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZKR" varchar(2) NOT NULL default ' ',
  "AAAAIWKZ" varchar(22) NOT NULL default ' ',
  "AAAAHZKT" varchar(20) NOT NULL default ' ',
  "AAAAIWKF" varchar(12) NOT NULL default ' ',
  "AAAAIWKG" varchar(8) NOT NULL default '00000000',
  "AAAAIWKH" varchar(6) NOT NULL default '000000',
  "AAAAVPNH" varchar(12) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  "AAAAIWKL" varchar(1) NOT NULL default ' ',
  "AAAAIWKM" varchar(4) NOT NULL default ' ',
  "AAABJZMF" varchar(4) NOT NULL default ' ',
  "AAAAIWKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHZKR","AAAAIWKZ","AAAAHZKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZKC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABIWLC" varchar(4) NOT NULL default '0000',
  "AAAAHZKR" varchar(2) NOT NULL default ' ',
  "AAAAIWKZ" varchar(22) NOT NULL default ' ',
  "AAAAHZKT" varchar(20) NOT NULL default ' ',
  "AAABFLEO" varchar(1) NOT NULL default ' ',
  "AAABKZAZ" varchar(3) NOT NULL default '000',
  "AAAAIWKW" decimal(5,2) NOT NULL default '0.00',
  "AAAAIWKX" varchar(22) NOT NULL default ' ',
  "AAAAHZKN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABIWLC","AAAAHZKR","AAAAIWKZ","AAAAHZKT","AAABFLEO","AAABKZAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHZLN" varchar(8) NOT NULL default '00000000',
  "AAABKWQO" varchar(3) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHZLF" varchar(8) NOT NULL default '00000000',
  "AAAAHZLG" decimal(13,2) NOT NULL default '0.00',
  "AAAAHZLH" decimal(13,2) NOT NULL default '0.00',
  "AAABMFSI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAAAHZLN"),
  KEY "AAAAHZLO" ("AAABMCNU","AAABMQCA","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMQCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJUN" varchar(4) NOT NULL default '0000',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAAAKHHY" varchar(8) NOT NULL default ' ',
  "AAAAHZNT" varchar(4) NOT NULL default '0000',
  "AAAAHZMK" varchar(24) NOT NULL default ' ',
  "AAAAHZML" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKJUN","AAAAIEPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJUN" varchar(4) NOT NULL default '0000',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAHZNT" varchar(4) NOT NULL default '0000',
  "AAAAHZMW" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKJUN","AAABIBHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJUN" varchar(4) NOT NULL default '0000',
  "AAAAHZNN" varchar(16) NOT NULL default ' ',
  "AAABMQHO" varchar(22) NOT NULL default ' ',
  "AAAAHZNT" varchar(4) NOT NULL default '0000',
  "AAAAHZNI" varchar(16) NOT NULL default ' ',
  "AAAAHZNJ" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKJUN","AAAAHZNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKJUN" varchar(4) NOT NULL default '0000',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAAAHZNT" varchar(4) NOT NULL default '0000',
  "AAAAHZNU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABKJUN","AAAAIEPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHZOI" varchar(2) NOT NULL default ' ',
  "AAABFODY" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAHZOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABIKIG" varchar(16) NOT NULL default ' ',
  "AAAAHZPA" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCMO","AAABIKIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAAAHZQH" varchar(1) NOT NULL default ' ',
  "AAABCCXW" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAHZQY" varchar(22) NOT NULL default ' ',
  "AAAAHZQS" varchar(8) NOT NULL default ' ',
  "AAAAHZQW" varchar(10) NOT NULL default ' ',
  "AAAAHZQP" varchar(34) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAHZPW" varchar(16) NOT NULL default ' ',
  "AAAAHZPX" decimal(15,2) NOT NULL default '0.00',
  "AAAAHZPY" decimal(15,2) NOT NULL default '0.00',
  "AAAAHZPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABIHLK","AAABMCZG","AAABMDBB","AAABKRXF","AAABLOLZ","AAAAHZQH","AAABCCXW","AAABKRXK","AAABKRXE","AAABKRXN","AAAAHZQY","AAAAHZQS","AAAAHZQW","AAAAHZQP"),
  KEY "AAAAHZQQ" ("AAAAHZQY","AAAAHZQS","AAABMRIL","AAABMDBB"),
  KEY "AAAAHZQV" ("AAAAHZQW","AAABMDBB","AAAAHZQY","AAABMRIL"),
  KEY "AAAAHZRA" ("AAABLXTG","AAABIHLK","AAABMRIL","AAABMDBB"),
  KEY "AAAAHZRF" ("AAABMCZG","AAABMDBB","AAABLXTG","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZRK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAHZRN" varchar(1) NOT NULL default ' ',
  "AAAAHZRO" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHZRP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZRS" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAAAHZSH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPNV","AAABCPOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZSC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABCPOE" varchar(4) NOT NULL default '0000',
  "AAAAHZSH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKJYV","AAABCPOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRD" varchar(4) NOT NULL default ' ',
  "AAAAHZTM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZTM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZTM" varchar(10) NOT NULL default ' ',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAAAHZTB" varchar(1) NOT NULL default ' ',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZTM","AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZTM" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZTM","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZTO" (
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  "AAAAQZIB" varchar(30) NOT NULL default ' ',
  "AAABEGDW" varchar(1) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  "AAABMDCZ" varchar(3) NOT NULL default ' ',
  "AAABMDDF" varchar(3) NOT NULL default ' ',
  "AAABMDDL" varchar(3) NOT NULL default ' ',
  "AAABMDDR" varchar(3) NOT NULL default ' ',
  "AAAAHZTX" varchar(1) NOT NULL default ' ',
  "AAABLKIV" varchar(1) NOT NULL default ' ',
  "AAAAHZTZ" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGRZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZUM" varchar(1) NOT NULL default ' ',
  "AAAAHZUN" varchar(1) NOT NULL default ' ',
  "AAAAHZUO" varchar(1) NOT NULL default ' ',
  "AAAAHZUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHZUM" varchar(1) NOT NULL default ' ',
  "AAAAHZUN" varchar(1) NOT NULL default ' ',
  "AAAAHZUO" varchar(1) NOT NULL default ' ',
  "AAAAHZUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZUS" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAHZUU" varchar(4) NOT NULL default ' ',
  "AAAAHZUV" varchar(4) NOT NULL default ' ',
  "AAAAHZUW" varchar(4) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABHGTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZVB" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZVH" (
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABHGTL" varchar(1) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAHZVL" varchar(20) NOT NULL default ' ',
  "AAAAHZVM" varchar(20) NOT NULL default ' ',
  "AAAAHZVN" varchar(20) NOT NULL default ' ',
  "AAAAHZVO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLODQ","AAABHGTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZWC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHZWC" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHZWC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAAAHZWV" varchar(4) NOT NULL default '0000',
  "AAAAHZWI" varchar(1) NOT NULL default ' ',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" varchar(6) NOT NULL default '000000',
  "AAABILZK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAAAHZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZWP" (
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAAAHZWV" varchar(4) NOT NULL default '0000',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAAAHZWT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMNC","AAAAHZWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZWW" (
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABHMNR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAIAAB" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAHZXG" varchar(2) NOT NULL default ' ',
  "AAAAHZXH" varchar(3) NOT NULL default '000',
  "AAAAHZXI" varchar(1) NOT NULL default ' ',
  "AAABBKTQ" varchar(2) NOT NULL default '00',
  "AAAAHZXK" varchar(8) NOT NULL default '00000000',
  "AAAAHZXL" varchar(8) NOT NULL default '00000000',
  "AAAAHZXM" varchar(1) NOT NULL default ' ',
  "AAAAWBOJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAAAIAAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAIAAB" varchar(3) NOT NULL default '000',
  "AAABGVPU" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAAALBWH" varchar(8) NOT NULL default '00000000',
  "AAABKZLG" varchar(10) NOT NULL default ' ',
  "AAABLBET" varchar(1) NOT NULL default ' ',
  "AAABJLFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAAAIAAB","AAABGVPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZYH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABCPNV" varchar(6) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHZZR" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHZZH" varchar(4) NOT NULL default ' ',
  "AAAAHZZI" varchar(16) NOT NULL default ' ',
  "AAAAHZZJ" varchar(24) NOT NULL default ' ',
  "AAAAHZZK" varchar(16) NOT NULL default ' ',
  "AAAAHZZL" varchar(24) NOT NULL default ' ',
  "AAAAHZZM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABCPNV","AAAAHZZQ","AAAAHZZR","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZZA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAHZZQ" varchar(1) NOT NULL default ' ',
  "AAAAHZZR" varchar(4) NOT NULL default ' ',
  "AAAAUFDY" varchar(2) NOT NULL default ' ',
  "AAAAHZZH" varchar(4) NOT NULL default ' ',
  "AAAAHZZI" varchar(16) NOT NULL default ' ',
  "AAAAHZZJ" varchar(24) NOT NULL default ' ',
  "AAAAHZZK" varchar(16) NOT NULL default ' ',
  "AAAAHZZL" varchar(24) NOT NULL default ' ',
  "AAAAHZZM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABKJYV","AAAAHZZQ","AAAAHZZR","AAAAUFDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHZZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAIAAB" varchar(3) NOT NULL default '000',
  "AAAALBZM" varchar(3) NOT NULL default '000',
  "AAAAHZZY" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAAAIAAB","AAAALBZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWHA" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWHA","AAABMCNU","AAABMDBB","AAAAICFT"),
  KEY "AAAAIAAQ" ("AAABFWHA","AAABMDBB","AAABMCNU"),
  KEY "AAAAIAAU" ("AAABMCNU","AAABMDBB","AAAAICFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWHA" varchar(16) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWHA","AAAAIEGF","AAAAICIG","AAAAICIH","AAABIBHM","AAABMDBB","AAABMCNU","AAABGPJB"),
  KEY "AAAAIABV" ("AAAAIEGF","AAAAICIG","AAAAICIH","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIACB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIAEB" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICLL" varchar(1) NOT NULL default ' ',
  "AAAAICLM" varchar(10) NOT NULL default ' ',
  "AAAAICLN" varchar(10) NOT NULL default ' ',
  "AAAAICLO" varchar(1) NOT NULL default ' ',
  "AAAAICLP" varchar(1) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAICLR" varchar(1) NOT NULL default ' ',
  "AAAAICLS" varchar(1) NOT NULL default ' ',
  "AAAAICLT" varchar(1) NOT NULL default ' ',
  "AAAAICLU" varchar(1) NOT NULL default ' ',
  "AAAAICLV" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLY" varchar(1) NOT NULL default ' ',
  "AAAAICLZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMA" decimal(15,2) NOT NULL default '0.00',
  "AAAAICME" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMB" varchar(1) NOT NULL default ' ',
  "AAAAICMC" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMD" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMF" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMG" varchar(24) NOT NULL default ' ',
  "AAAAICMH" varchar(16) NOT NULL default ' ',
  "AAAAICMI" varchar(10) NOT NULL default ' ',
  "AAAAICMK" decimal(5,2) NOT NULL default '0.00',
  "AAAAICML" varchar(1) NOT NULL default ' ',
  "AAAAICMM" varchar(1) NOT NULL default ' ',
  "AAAAICMN" varchar(1) NOT NULL default ' ',
  "AAAAICMO" varchar(1) NOT NULL default ' ',
  "AAAAICMP" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMR" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMS" varchar(1) NOT NULL default ' ',
  "AAAAICMT" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMU" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMV" varchar(1) NOT NULL default ' ',
  "AAAAICMW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMX" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAICNA" varchar(24) NOT NULL default ' ',
  "AAAAICNB" varchar(16) NOT NULL default ' ',
  "AAAAICNC" varchar(10) NOT NULL default ' ',
  "AAAAICND" decimal(5,2) NOT NULL default '0.00',
  "AAAAICNF" varchar(1) NOT NULL default ' ',
  "AAAAICNG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIAEB","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFZVY" decimal(13,2) NOT NULL default '0.00',
  "AAABFZVZ" varchar(1) NOT NULL default ' ',
  "AAABFZWA" varchar(1) NOT NULL default ' ',
  "AAABFZWB" decimal(4,2) NOT NULL default '0.00',
  "AAABFZWE" varchar(1) NOT NULL default ' ',
  "AAABFZWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFZWY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFZXA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYBL" int(11) NOT NULL default '0',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYBL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMQCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAHA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPPR" varchar(4) NOT NULL default ' ',
  "AAAAIVEY" varchar(10) NOT NULL default ' ',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAIAHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLPPR","AAAAIVEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAIAHR" varchar(3) NOT NULL default ' ',
  "AAAAIAHS" varchar(80) NOT NULL default ' ',
  "AAAAVMGR" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(3) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABKGST" varchar(5) NOT NULL default '00000',
  "AAABKGSU" varchar(8) NOT NULL default '00000000',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABJRKN" varchar(55) NOT NULL default ' ',
  "AAABJRIB" varchar(18) NOT NULL default ' ',
  "AAABJRIA" varchar(15) NOT NULL default ' ',
  "AAAAIAIJ" varchar(81) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAAAIAIM" varchar(12) NOT NULL default ' ',
  "AAAAIAIN" varchar(8) NOT NULL default '00000000',
  "AAAAIAIO" varchar(1) NOT NULL default ' ',
  "AAAAIAIP" varchar(32) NOT NULL default ' ',
  "AAAAIAIQ" varchar(4) NOT NULL default ' ',
  "AAAAIAIR" varchar(10) NOT NULL default ' ',
  "AAAAIAIS" varchar(4) NOT NULL default '0000',
  "AAABKZXE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABKGSX","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIAKN" varchar(24) NOT NULL default ' ',
  "AAAAIAKA" varchar(16) NOT NULL default ' ',
  "AAAAIAKB" varchar(10) NOT NULL default ' ',
  "AAAAIANM" varchar(24) NOT NULL default ' ',
  "AAAAIANN" varchar(16) NOT NULL default ' ',
  "AAAAIANO" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIAKN","AAAAIAKA","AAAAIAKB","AAAAIANM","AAAAIANN","AAAAIANO","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIANS" varchar(24) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIAKU" varchar(3) NOT NULL default ' ',
  "AAAAIAKN" varchar(24) NOT NULL default ' ',
  "AAAAIAKO" varchar(3) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIANS","AAABMDBB","AAABMCNU","AAAAIAKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIANS" varchar(24) NOT NULL default ' ',
  "AAAAIANT" varchar(16) NOT NULL default ' ',
  "AAAAIANU" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAUZTA" varchar(1) NOT NULL default ' ',
  "AAAAIAMU" varchar(1) NOT NULL default ' ',
  "AAAAIAMV" varchar(24) NOT NULL default ' ',
  "AAAAIAMW" varchar(16) NOT NULL default ' ',
  "AAAAIAMX" varchar(10) NOT NULL default ' ',
  "AAAAIAMY" decimal(17,2) NOT NULL default '0.00',
  "AAAAIAMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANB" varchar(8) NOT NULL default '00000000',
  "AAAAIANC" varchar(8) NOT NULL default '00000000',
  "AAAAIAND" varchar(1) NOT NULL default ' ',
  "AAAAIANE" varchar(24) NOT NULL default ' ',
  "AAAAIANF" varchar(16) NOT NULL default ' ',
  "AAAAIANG" varchar(10) NOT NULL default ' ',
  "AAAAIANH" decimal(17,2) NOT NULL default '0.00',
  "AAAAIANI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANK" varchar(8) NOT NULL default '00000000',
  "AAAAIANL" varchar(8) NOT NULL default '00000000',
  "AAAAIANM" varchar(24) NOT NULL default ' ',
  "AAAAIANN" varchar(16) NOT NULL default ' ',
  "AAAAIANO" varchar(10) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAIANQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIANS","AAAAIANT","AAAAIANU","AAABMDBB","AAABMCNU","AAAAUZTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIANS" varchar(24) NOT NULL default ' ',
  "AAAAIANT" varchar(16) NOT NULL default ' ',
  "AAAAIANU" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAUZTA" varchar(1) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAIAMU" varchar(1) NOT NULL default ' ',
  "AAAAIAMV" varchar(24) NOT NULL default ' ',
  "AAAAIAMW" varchar(16) NOT NULL default ' ',
  "AAAAIAMX" varchar(10) NOT NULL default ' ',
  "AAAAIAMY" decimal(17,2) NOT NULL default '0.00',
  "AAAAIAMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANB" varchar(8) NOT NULL default '00000000',
  "AAAAIANC" varchar(8) NOT NULL default '00000000',
  "AAAAIAND" varchar(1) NOT NULL default ' ',
  "AAAAIANE" varchar(24) NOT NULL default ' ',
  "AAAAIANF" varchar(16) NOT NULL default ' ',
  "AAAAIANG" varchar(10) NOT NULL default ' ',
  "AAAAIANH" decimal(17,2) NOT NULL default '0.00',
  "AAAAIANI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIANK" varchar(8) NOT NULL default '00000000',
  "AAAAIANL" varchar(8) NOT NULL default '00000000',
  "AAAAIANM" varchar(24) NOT NULL default ' ',
  "AAAAIANN" varchar(16) NOT NULL default ' ',
  "AAAAIANO" varchar(10) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAIANQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIANS","AAAAIANT","AAAAIANU","AAABMDBB","AAABMCNU","AAAAUZTA","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIANZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABKRXG" varchar(1) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAIBFE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFK" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFL" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFM" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFN" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFO" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFP" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFR" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFS" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFT" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFU" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFV" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFW" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFX" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFY" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGB" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGC" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGD" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGJ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXO","AAABMCZG","AAABKRXF","AAABKRXG","AAABLOLZ","AAABIHLK","AAABKRXN","AAABMREX","AAABMCPV","AAAAIBGY"),
  KEY "AAAAIAQI" ("AAABLXTG","AAABMRIL","AAABKRXE","AAABKRXF","AAABMDBB","AAABLOLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAQP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIBAJ" varchar(6) NOT NULL default ' ',
  "AAAALBWR" varchar(12) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAIBAU" varchar(10) NOT NULL default ' ',
  "AAAAXZXG" varchar(20) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
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
  "AAAAIVXA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIVXB" varchar(5) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAAAIVWZ" varchar(1) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAIASX" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAAAIATF" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAIATL" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIATT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIAUB" ("AAABMRIL","AAABMQDU","AAABMQBS","AAAAJVGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABDTDP" varchar(10) NOT NULL default ' ',
  "AAABMCXM" varchar(8) NOT NULL default '00000000',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIAVA" ("AAABGPZS","AAAAKOVZ","AAABDTDP","AAABMREX","AAABMRIL","AAABCPWH","AAABKJYV","AAABMCXM","AAABMCZF","AAABKGVD","AAABMCRM","AAABKRUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAVN" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIBAJ" varchar(6) NOT NULL default ' ',
  "AAAALBWR" varchar(12) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAIBAU" varchar(10) NOT NULL default ' ',
  "AAAAXZXG" varchar(20) NOT NULL default ' ',
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
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAIAYT" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAIAZC" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAAAIAZH" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIAZP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIBAJ" varchar(6) NOT NULL default ' ',
  "AAAALBWR" varchar(12) NOT NULL default ' ',
  "AAAAIWGC" varchar(20) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAIBAU" varchar(10) NOT NULL default ' ',
  "AAAAXZXG" varchar(20) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAIBCS" ("AAABMQHV","AAABMQDO","AAABMQHZ"),
  KEY "AAAAIBCW" ("AAABMQHV","AAABMQDO","AAABBTTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBDA" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBDH" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(15) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJVH" varchar(5) NOT NULL default ' ',
  "AAABESOB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABCPQF","AAABMRFO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAAAIBEA" varchar(5) NOT NULL default ' ',
  "AAAAIBEB" varchar(5) NOT NULL default ' ',
  "AAAAIBEC" varchar(5) NOT NULL default ' ',
  "AAAAIBED" varchar(5) NOT NULL default ' ',
  "AAAAIBEE" varchar(5) NOT NULL default ' ',
  "AAAAIBEF" varchar(5) NOT NULL default ' ',
  "AAAAIBEG" varchar(5) NOT NULL default ' ',
  "AAAAIBEH" varchar(5) NOT NULL default ' ',
  "AAAAIBEI" varchar(5) NOT NULL default ' ',
  "AAAAIBEJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABMCZG","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXO" varchar(3) NOT NULL default '000',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABLOLZ" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAIBFE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFK" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFL" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFM" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFN" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFO" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFP" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFR" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFS" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFT" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFU" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFV" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFW" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFX" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFY" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBFZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGB" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGC" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGD" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGK" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBGL" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXO","AAABMCZG","AAABKRXF","AAABLOLZ","AAABIHLK","AAABKRXN","AAABMREX","AAABMCPV","AAAAIBGY"),
  KEY "AAAAIBGZ" ("AAABMRIL","AAABKRXE","AAABMCZG","AAABLXTG"),
  KEY "AAAAIBHE" ("AAABLXTG","AAABMRIL","AAABKRXE","AAABKRXF","AAABMDBB","AAABLOLZ"),
  KEY "AAAAIBHL" ("AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAIBHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABLTXI" varchar(24) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS","AAABMCNU","AAABKGVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABCBGD" varchar(4) NOT NULL default ' ',
  "AAABCBFS" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABCBGD","AAABCBFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAAAIBKC" varchar(15) NOT NULL default ' ',
  "AAAAQEAA" varchar(8) NOT NULL default '00000000',
  "AAAAQDRK" varchar(8) NOT NULL default '00000000',
  "AAAAIBJO" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJP" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJR" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJS" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJT" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJU" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJV" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJW" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBJX" varchar(12) NOT NULL default ' ',
  "AAAAIBJY" varchar(8) NOT NULL default ' ',
  "AAAAIBJZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI","AAAAIBKC","AAAAQEAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAAAIBLD" varchar(1) NOT NULL default ' ',
  "AAAAIBKL" varchar(1) NOT NULL default ' ',
  "AAAAQDTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAAAIBKS" varchar(50) NOT NULL default ' ',
  "AAAAIBKT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDTI" varchar(20) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAQDTK" varchar(1) NOT NULL default ' ',
  "AAABMAGQ" varchar(20) NOT NULL default ' ',
  "AAAAIBLD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDTI","AAABMLZS","AAABMCNU","AAAAQDTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBLJ" (
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBLM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBLS" (
  "AAABCRMI" varchar(10) NOT NULL default ' ',
  "AAABKVFD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABCRMI","AAABKVFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYHHY" varchar(11) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAAAPOSG" varchar(13) NOT NULL default ' ',
  "AAAAPOSH" varchar(13) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYHHY","AAABMNML","AAAAPOSG","AAAAPOSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(1) NOT NULL default ' ',
  "AAAAIBMP" varchar(3) NOT NULL default ' ',
  "AAAAIBMQ" varchar(6) NOT NULL default ' ',
  "AAAAIBMR" varchar(6) NOT NULL default ' ',
  "AAAAIBMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  "AAABHAZH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJRIC","AAAAWFPK","AAAAWFPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBNZ" varchar(11) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABJRIC" varchar(1) NOT NULL default ' ',
  "AAAAWFPK" varchar(8) NOT NULL default ' ',
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  "AAAAQUEE" varchar(6) NOT NULL default '000000',
  "AAAAIBNQ" varchar(8) NOT NULL default '00000000',
  "AAAAIBNR" varchar(8) NOT NULL default '00000000',
  "AAABKGST" varchar(5) NOT NULL default '00000',
  "AAAAIBNT" varchar(1) NOT NULL default ' ',
  "AAAAPOSG" varchar(13) NOT NULL default ' ',
  "AAAAPOSH" varchar(13) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBOG" varchar(2) NOT NULL default ' ',
  "AAABCRWQ" varchar(8) NOT NULL default '00000000',
  "AAAAIBOE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIBOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBOH" (
  "AAABHHRZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBOK" (
  "AAABHHRZ" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABHHRZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBOQ" (
  "AAAAIBOV" varchar(30) NOT NULL default ' ',
  "AAAAIBOW" varchar(30) NOT NULL default ' ',
  "AAAAIBOT" varchar(30) NOT NULL default ' ',
  "AAAAIBOU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIBOV","AAAAIBOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCBKW" varchar(2) NOT NULL default ' ',
  "AAABCBKX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBPC" (
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABHIFX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQDUI" varchar(2) NOT NULL default ' ',
  "AAABMOHD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQDUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAIBQU" varchar(12) NOT NULL default ' ',
  "AAAAIBQV" varchar(12) NOT NULL default ' ',
  "AAAAIBPX" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAIBQT","AAAAIBQU","AAAAIBQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIBQT" varchar(12) NOT NULL default ' ',
  "AAAAIBQU" varchar(12) NOT NULL default ' ',
  "AAAAIBQV" varchar(12) NOT NULL default ' ',
  "AAAAIBQM" varchar(50) NOT NULL default ' ',
  "AAAAIBQN" varchar(50) NOT NULL default ' ',
  "AAAAIBQO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAIBQT","AAAAIBQU","AAAAIBQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAVOMC" varchar(8) NOT NULL default '00000000',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABCPQF","AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBWX" varchar(10) NOT NULL default ' ',
  "AAAAIBWY" varchar(4) NOT NULL default ' ',
  "AAAAIBWZ" varchar(4) NOT NULL default '0000',
  "AAAAIBXA" varchar(3) NOT NULL default '000',
  "AAAAIBZV" varchar(14) NOT NULL default ' ',
  "AAAAIBZW" varchar(16) NOT NULL default ' ',
  "AAAAIBZX" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAAAIBYZ" varchar(14) NOT NULL default ' ',
  "AAAAIBZA" varchar(16) NOT NULL default ' ',
  "AAAAIBZB" varchar(10) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZG" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZH" varchar(3) NOT NULL default '000',
  "AAAAIBZI" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZJ" varchar(3) NOT NULL default '000',
  "AAAAICAS" varchar(14) NOT NULL default ' ',
  "AAAAICAT" varchar(16) NOT NULL default ' ',
  "AAAAICAU" varchar(10) NOT NULL default ' ',
  "AAAAIBZP" varchar(1) NOT NULL default ' ',
  "AAAAIBXV" varchar(1) NOT NULL default ' ',
  "AAAAIBZQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBZR" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAIBWX","AAAAIBWY","AAAAIBWZ","AAAAIBXA","AAAAIBZV","AAAAIBZW","AAAAIBZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBWX" varchar(10) NOT NULL default ' ',
  "AAAAIBWY" varchar(4) NOT NULL default ' ',
  "AAAAIBWZ" varchar(4) NOT NULL default '0000',
  "AAAAIBXA" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAAAICAO" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICAQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAICCY" decimal(15,2) NOT NULL default '0.00',
  "AAAAICAS" varchar(14) NOT NULL default ' ',
  "AAAAICAT" varchar(16) NOT NULL default ' ',
  "AAAAICAU" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBWX","AAAAIBWY","AAAAIBWZ","AAAAIBXA","AAAAICDD","AAAAICIG","AAAAICIH","AAABIBHM","AAAAICDG","AAABGPJB","AAAAICCV","AAAAICCW","AAAAICCX","AAAAICAO"),
  KEY "AAAAIBUK" ("AAAAICCV","AAAAICCW","AAAAICCX","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBWX" varchar(10) NOT NULL default ' ',
  "AAAAIBWY" varchar(4) NOT NULL default ' ',
  "AAAAIBWZ" varchar(4) NOT NULL default '0000',
  "AAAAIBXA" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAICAO" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAAAICBV" decimal(15,2) NOT NULL default '0.00',
  "AAAAICBW" varchar(10) NOT NULL default '0000000000',
  "AAAAICBX" varchar(3) NOT NULL default '000',
  "AAAAICBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBWX","AAAAIBWY","AAAAIBWZ","AAAAIBXA","AAAAICDD","AAAAICIG","AAAAICIH","AAAAICDG","AAABGPJB","AAABLTXE","AAAAICCV","AAAAICCW","AAAAICCX","AAABIBHM","AAAAICAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBWX" varchar(10) NOT NULL default ' ',
  "AAAAIBWY" varchar(4) NOT NULL default ' ',
  "AAAAIBWZ" varchar(4) NOT NULL default '0000',
  "AAAAIBXA" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAAAICCY" decimal(15,2) NOT NULL default '0.00',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBWX","AAAAIBWY","AAAAIBWZ","AAAAIBXA","AAAAICDD","AAAAICIG","AAAAICIH","AAAAICDG","AAABGPJB","AAAAICCV","AAAAICCW","AAAAICCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIBYC" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIBYE" varchar(3) NOT NULL default '000',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAAAIBZP" varchar(1) NOT NULL default ' ',
  "AAAAIBZI" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZJ" varchar(3) NOT NULL default '000',
  "AAAAIBXV" varchar(1) NOT NULL default ' ',
  "AAAAIBZG" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZH" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAIBYM" varchar(4) NOT NULL default '0000',
  "AAAAIBYL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIBYC","AAABMCNU","AAAAIBYE","AAAAXZDS","AAABMQHV","AAABMDBB","AAABLTXE"),
  KEY "AAAAIBYJ" ("AAABMRIL","AAAAIBYL","AAAAIBYM","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICDB" varchar(10) NOT NULL default ' ',
  "AAAAICDC" varchar(3) NOT NULL default '000',
  "AAAAIBZV" varchar(14) NOT NULL default ' ',
  "AAAAIBZW" varchar(16) NOT NULL default ' ',
  "AAAAIBZX" varchar(10) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAAAIBYZ" varchar(14) NOT NULL default ' ',
  "AAAAIBZA" varchar(16) NOT NULL default ' ',
  "AAAAIBZB" varchar(10) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAIBZD" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAAIBZG" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZH" varchar(3) NOT NULL default '000',
  "AAAAIBZI" varchar(10) NOT NULL default '0000000000',
  "AAAAIBZJ" varchar(3) NOT NULL default '000',
  "AAAAICAS" varchar(14) NOT NULL default ' ',
  "AAAAICAT" varchar(16) NOT NULL default ' ',
  "AAAAICAU" varchar(10) NOT NULL default ' ',
  "AAAAIBZN" varchar(10) NOT NULL default ' ',
  "AAAAIBZO" varchar(3) NOT NULL default '000',
  "AAAAIBZP" varchar(1) NOT NULL default ' ',
  "AAAAIBZQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIBZR" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAICDB","AAAAICDC","AAAAIBZV","AAAAIBZW","AAAAIBZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIBZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICDB" varchar(10) NOT NULL default ' ',
  "AAAAICDC" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAAAICAO" varchar(16) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICAQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAICCY" decimal(13,2) NOT NULL default '0.00',
  "AAAAICAS" varchar(14) NOT NULL default ' ',
  "AAAAICAT" varchar(16) NOT NULL default ' ',
  "AAAAICAU" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICDB","AAAAICDC","AAAAICDD","AAAAICIG","AAAAICIH","AAABIBHM","AAAAICDG","AAABGPJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICDB" varchar(10) NOT NULL default ' ',
  "AAAAICDC" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAABLTXE" varchar(16) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAAAICBV" decimal(15,2) NOT NULL default '0.00',
  "AAAAICBW" varchar(10) NOT NULL default '0000000000',
  "AAAAICBX" varchar(3) NOT NULL default '000',
  "AAAAICBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICDB","AAAAICDC","AAAAICDD","AAAAICIG","AAAAICIH","AAAAICDG","AAABGPJB","AAABLTXE","AAABIBHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAICDB" varchar(10) NOT NULL default ' ',
  "AAAAICDC" varchar(3) NOT NULL default '000',
  "AAAAICDD" varchar(14) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAAAICDG" varchar(1) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICCT" varchar(4) NOT NULL default '0000',
  "AAAAICCU" varchar(3) NOT NULL default '000',
  "AAAAICCV" varchar(14) NOT NULL default ' ',
  "AAAAICCW" varchar(16) NOT NULL default ' ',
  "AAAAICCX" varchar(10) NOT NULL default ' ',
  "AAAAICCY" decimal(13,2) NOT NULL default '0.00',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAICDB","AAAAICDC","AAAAICDD","AAAAICIG","AAAAICIH","AAAAICDG","AAABGPJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMCNU","AAABMDBB","AAAAICFT"),
  KEY "AAAAICEB" ("AAABKRUI","AAAANAVQ","AAABJKCJ","AAABMDBB","AAABMCNU"),
  KEY "AAAAICEH" ("AAABMCNU","AAABMDBB","AAAAICFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICFT" varchar(16) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMCNU","AAABMDBB","AAAAICFT","AAABKGFD"),
  KEY "AAAAICFK" ("AAABKRUI","AAAANAVQ","AAABJKCJ","AAABMDBB","AAABMCNU"),
  KEY "AAAAICFQ" ("AAABMCNU","AAABMDBB","AAAAICFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAAAIEPU" varchar(16) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEPT","AAAAIEPU","AAAAIEPV","AAABIBHQ","AAAAIEGF","AAAAICIG","AAAAICIH","AAABIBHM","AAABMDBB","AAABMCNU","AAABGPJB"),
  KEY "AAAAICGX" ("AAAAIEGF","AAAAICIG","AAAAICIH","AAABMDBB","AAABMCNU","AAABIBHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAAAIEPU" varchar(16) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAICIG" varchar(16) NOT NULL default ' ',
  "AAAAICIH" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABGPJB" varchar(2) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAICHW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICHY" varchar(1) NOT NULL default ' ',
  "AAAAICHZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEPT","AAAAIEPU","AAAAIEPV","AAABIBHQ","AAAAIEGF","AAAAICIG","AAAAICIH","AAABIBHM","AAABMDBB","AAABMCNU","AAABGPJB","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAICLL" varchar(1) NOT NULL default ' ',
  "AAAAICLM" varchar(10) NOT NULL default ' ',
  "AAAAICLN" varchar(10) NOT NULL default ' ',
  "AAAAICLO" varchar(1) NOT NULL default ' ',
  "AAAAICLP" varchar(1) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAICLR" varchar(1) NOT NULL default ' ',
  "AAAAICLS" varchar(1) NOT NULL default ' ',
  "AAAAICLT" varchar(1) NOT NULL default ' ',
  "AAAAICLU" varchar(1) NOT NULL default ' ',
  "AAAAICLV" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLY" varchar(1) NOT NULL default ' ',
  "AAAAICLZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMA" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMB" varchar(1) NOT NULL default ' ',
  "AAAAICMC" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMD" decimal(15,2) NOT NULL default '0.00',
  "AAAAICME" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMF" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMG" varchar(24) NOT NULL default ' ',
  "AAAAICMH" varchar(16) NOT NULL default ' ',
  "AAAAICMI" varchar(10) NOT NULL default ' ',
  "AAAAICMJ" varchar(16) NOT NULL default ' ',
  "AAAAICMK" decimal(5,2) NOT NULL default '0.00',
  "AAAAICML" varchar(1) NOT NULL default ' ',
  "AAAAICMM" varchar(1) NOT NULL default ' ',
  "AAAAICMN" varchar(1) NOT NULL default ' ',
  "AAAAICMO" varchar(1) NOT NULL default ' ',
  "AAAAICMP" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMR" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMS" varchar(1) NOT NULL default ' ',
  "AAAAICMT" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMU" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMV" varchar(1) NOT NULL default ' ',
  "AAAAICMW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMX" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAICNA" varchar(24) NOT NULL default ' ',
  "AAAAICNB" varchar(16) NOT NULL default ' ',
  "AAAAICNC" varchar(10) NOT NULL default ' ',
  "AAAAICNE" varchar(16) NOT NULL default ' ',
  "AAAAICND" decimal(5,2) NOT NULL default '0.00',
  "AAAAICNF" varchar(1) NOT NULL default ' ',
  "AAAAICNG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAAANAVQ" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABKGFD" varchar(16) NOT NULL default ' ',
  "AAABMNZR" varchar(16) NOT NULL default ' ',
  "AAAAICLI" varchar(1) NOT NULL default ' ',
  "AAAAVMHD" varchar(1) NOT NULL default ' ',
  "AAAAICLK" varchar(12) NOT NULL default ' ',
  "AAAAICLL" varchar(1) NOT NULL default ' ',
  "AAAAICLM" varchar(10) NOT NULL default ' ',
  "AAAAICLN" varchar(10) NOT NULL default ' ',
  "AAAAICLO" varchar(1) NOT NULL default ' ',
  "AAAAICLP" varchar(1) NOT NULL default ' ',
  "AAABCPUN" varchar(1) NOT NULL default ' ',
  "AAAAICLR" varchar(1) NOT NULL default ' ',
  "AAAAICLS" varchar(1) NOT NULL default ' ',
  "AAAAICLT" varchar(1) NOT NULL default ' ',
  "AAAAICLU" varchar(1) NOT NULL default ' ',
  "AAAAICLV" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLX" decimal(5,2) NOT NULL default '0.00',
  "AAAAICLY" varchar(1) NOT NULL default ' ',
  "AAAAICLZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMA" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMB" varchar(1) NOT NULL default ' ',
  "AAAAICMC" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMD" decimal(15,2) NOT NULL default '0.00',
  "AAAAICME" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMF" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMG" varchar(24) NOT NULL default ' ',
  "AAAAICMH" varchar(16) NOT NULL default ' ',
  "AAAAICMI" varchar(10) NOT NULL default ' ',
  "AAAAICMJ" varchar(16) NOT NULL default ' ',
  "AAAAICMK" decimal(5,2) NOT NULL default '0.00',
  "AAAAICML" varchar(1) NOT NULL default ' ',
  "AAAAICMM" varchar(1) NOT NULL default ' ',
  "AAAAICMN" varchar(1) NOT NULL default ' ',
  "AAAAICMO" varchar(1) NOT NULL default ' ',
  "AAAAICMP" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMR" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMS" varchar(1) NOT NULL default ' ',
  "AAAAICMT" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMU" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMV" varchar(1) NOT NULL default ' ',
  "AAAAICMW" decimal(5,2) NOT NULL default '0.00',
  "AAAAICMX" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMY" decimal(15,2) NOT NULL default '0.00',
  "AAAAICMZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAICNA" varchar(24) NOT NULL default ' ',
  "AAAAICNB" varchar(16) NOT NULL default ' ',
  "AAAAICNC" varchar(10) NOT NULL default ' ',
  "AAAAICND" decimal(5,2) NOT NULL default '0.00',
  "AAAAICNE" varchar(16) NOT NULL default ' ',
  "AAAAICNF" varchar(1) NOT NULL default ' ',
  "AAAAICNG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRUI","AAAANAVQ","AAABJKCJ","AAABCPWH","AAABMDBB","AAABMCNU","AAABKGFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  "AAABMMJW" varchar(12) NOT NULL default ' ',
  "AAAAICOS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAICOQ","AAABMMJW","AAAAICOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAICOQ" varchar(12) NOT NULL default ' ',
  "AAABMMJW" varchar(12) NOT NULL default ' ',
  "AAAAICOS" varchar(12) NOT NULL default ' ',
  "AAAAICOK" varchar(50) NOT NULL default ' ',
  "AAAAICOL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCNU","AAABMDBB","AAAAICOQ","AAABMMJW","AAAAICOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICOT" (
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAICOW" varchar(1) NOT NULL default ' ',
  "AAAAICOX" varchar(1) NOT NULL default ' ',
  "AAAAICOY" varchar(1) NOT NULL default ' ',
  "AAAAICRG" varchar(1) NOT NULL default ' ',
  "AAAAICRH" varchar(1) NOT NULL default ' ',
  "AAAAICPB" varchar(1) NOT NULL default ' ',
  "AAAAICRI" varchar(1) NOT NULL default ' ',
  "AAAAICRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKFFG","AAABLHGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICPG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICPM" (
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAKZSY" varchar(1) NOT NULL default ' ',
  "AAAAKZSZ" varchar(1) NOT NULL default ' ',
  "AAAAICPS" varchar(1) NOT NULL default ' ',
  "AAAAICPT" varchar(1) NOT NULL default ' ',
  "AAAAICPU" varchar(1) NOT NULL default ' ',
  "AAAAICQS" varchar(1) NOT NULL default ' ',
  "AAAAICQT" varchar(1) NOT NULL default ' ',
  "AAAAICPX" varchar(1) NOT NULL default ' ',
  "AAAAICPY" varchar(1) NOT NULL default ' ',
  "AAAAICPZ" varchar(1) NOT NULL default ' ',
  "AAAAICQA" varchar(1) NOT NULL default ' ',
  "AAAAICQB" varchar(1) NOT NULL default ' ',
  "AAAAICQC" varchar(1) NOT NULL default ' ',
  "AAAAICQV" varchar(1) NOT NULL default ' ',
  "AAAAICQX" varchar(1) NOT NULL default ' ',
  "AAAAICQU" varchar(1) NOT NULL default ' ',
  "AAAAICQG" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAICQW" varchar(1) NOT NULL default ' ',
  "AAAAICQY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKFFG","AAABKRXF","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAICQS" varchar(1) NOT NULL default ' ',
  "AAAAICQT" varchar(1) NOT NULL default ' ',
  "AAAAICQU" varchar(1) NOT NULL default ' ',
  "AAAAICQV" varchar(1) NOT NULL default ' ',
  "AAAAICQW" varchar(1) NOT NULL default ' ',
  "AAAAICQX" varchar(1) NOT NULL default ' ',
  "AAAAICQY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG","AAABKRXF","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFFG" varchar(6) NOT NULL default ' ',
  "AAAAICRG" varchar(1) NOT NULL default ' ',
  "AAAAICRH" varchar(1) NOT NULL default ' ',
  "AAAAICRI" varchar(1) NOT NULL default ' ',
  "AAAAICRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAAAICRP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICRS" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAABBTTG" varchar(12) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIEGJ" varchar(8) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIEGM" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
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
  "AAAAWFPL" varchar(4) NOT NULL default ' ',
  "AAAAIDRZ" varchar(4) NOT NULL default ' ',
  "AAAAIDSA" varchar(20) NOT NULL default ' ',
  "AAAAIDRX" varchar(1) NOT NULL default ' ',
  "AAAAIDDU" varchar(1) NOT NULL default ' ',
  "AAAAIDDV" varchar(1) NOT NULL default ' ',
  "AAAAIDDW" varchar(1) NOT NULL default ' ',
  "AAAAIDDX" varchar(1) NOT NULL default ' ',
  "AAAAIDKS" varchar(1) NOT NULL default ' ',
  "AAAAIDRS" varchar(1) NOT NULL default ' ',
  "AAAAIDRT" varchar(1) NOT NULL default ' ',
  "AAAAIDRU" varchar(1) NOT NULL default ' ',
  "AAAAIDRV" varchar(1) NOT NULL default ' ',
  "AAAAIDRW" varchar(1) NOT NULL default ' ',
  "AAAAIDRY" varchar(1) NOT NULL default ' ',
  "AAAAIDSB" varchar(1) NOT NULL default ' ',
  "AAAAIDSC" varchar(1) NOT NULL default ' ',
  "AAAAIDKT" varchar(2) NOT NULL default ' ',
  "AAAAIDKU" varchar(2) NOT NULL default ' ',
  "AAAAIDKV" varchar(2) NOT NULL default ' ',
  "AAAAIDKW" varchar(2) NOT NULL default ' ',
  "AAAAIDKX" varchar(4) NOT NULL default ' ',
  "AAAAIVXA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIVXB" varchar(5) NOT NULL default ' ',
  "AAAAICUM" varchar(10) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAAAIVWZ" varchar(1) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCSI" varchar(1) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIDEA" varchar(3) NOT NULL default ' ',
  "AAAAIDPH" varchar(62) NOT NULL default ' ',
  "AAAAIDZC" varchar(2) NOT NULL default ' ',
  "AAAAIDZD" varchar(4) NOT NULL default ' ',
  "AAAAIDZE" varchar(4) NOT NULL default ' ',
  "AAAAIDZF" varchar(1) NOT NULL default ' ',
  "AAAAIDZG" varchar(3) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAICVK" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAAAICVS" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAICVY" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAICWG" ("AAABMQHZ","AAABBTTF"),
  KEY "AAAAICWJ" ("AAAAIDPH","AAABMQHZ","AAABMQHV","AAABMQDO","AAABMQHY","AAABCPQF"),
  KEY "AAAAICWQ" ("AAABMQHV","AAABMQHW","AAABMQHX","AAABMQDO","AAABMDBA","AAABMPWO","AAABMQCW"),
  KEY "AAAAICWY" ("AAAAIWGG","AAAAIEJA","AAAAIEGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIEGM" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAICXK" ("AAABMRIL","AAABMQDU","AAAAIEGM","AAAAIEGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICXP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAICYC" varchar(2) NOT NULL default ' ',
  "AAAAICXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAICYC","AAAAICXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICXW" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAICYC" varchar(2) NOT NULL default ' ',
  "AAAAICYD" varchar(1) NOT NULL default ' ',
  "AAAAICYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAICYC","AAAAICYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAICYE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDIV" varchar(1) NOT NULL default ' ',
  "AAAAIDIW" varchar(1) NOT NULL default ' ',
  "AAAAIDIX" varchar(1) NOT NULL default ' ',
  "AAAAIDIY" varchar(1) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDJB" varchar(3) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDJG" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
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
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAAAIVWZ" varchar(1) NOT NULL default ' ',
  "AAAAIVXA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIVXB" varchar(5) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIDKT" varchar(2) NOT NULL default ' ',
  "AAAAIDKU" varchar(2) NOT NULL default ' ',
  "AAAAIDKV" varchar(2) NOT NULL default ' ',
  "AAAAIDKW" varchar(2) NOT NULL default ' ',
  "AAAAIDKX" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIDKS" varchar(1) NOT NULL default ' ',
  "AAAAIDPH" varchar(62) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAIDAT" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAAAIDBB" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAIDBH" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAIDBP" ("AAABMQHZ"),
  KEY "AAAAIDBR" ("AAAAIDPH","AAABMQHZ","AAABMQHV","AAABMQDO","AAABMQHY","AAABCPQF"),
  KEY "AAAAIDBY" ("AAABMQHV","AAABMQHW","AAABMQHX","AAABMQDO","AAABMDBA","AAABMPWO","AAABMQCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDJG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIDCN" ("AAABMRIL","AAABMQDU","AAAAIDJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAAAIDDU" varchar(1) NOT NULL default ' ',
  "AAAAIDDV" varchar(1) NOT NULL default ' ',
  "AAAAIDDW" varchar(1) NOT NULL default ' ',
  "AAAAIDDX" varchar(1) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAIDRW" varchar(1) NOT NULL default ' ',
  "AAAAIDEA" varchar(3) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIDDO" ("AAABGPZS","AAABMREX","AAABMRIL","AAABKJYV","AAAAIEBH","AAAAIDDU","AAAAIDDV","AAAAIDDW","AAAAIDDX","AAABMCRA","AAAAIDRW","AAAAIDEA","AAAAKOVZ","AAABCPWH","AAABMCWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDEE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDIV" varchar(1) NOT NULL default ' ',
  "AAAAIDIW" varchar(1) NOT NULL default ' ',
  "AAAAIDIX" varchar(1) NOT NULL default ' ',
  "AAAAIDIY" varchar(1) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDJB" varchar(3) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDJG" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAIDHH" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAIDHQ" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAAAIDHV" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAIDID" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDIF" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDIV" varchar(1) NOT NULL default ' ',
  "AAAAIDIW" varchar(1) NOT NULL default ' ',
  "AAAAIDIX" varchar(1) NOT NULL default ' ',
  "AAAAIDIY" varchar(1) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDJB" varchar(3) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIDJG" varchar(10) NOT NULL default ' ',
  "AAABMQDV" varchar(4) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIDKS" varchar(1) NOT NULL default ' ',
  "AAAAIDKT" varchar(2) NOT NULL default ' ',
  "AAAAIDKU" varchar(2) NOT NULL default ' ',
  "AAAAIDKV" varchar(2) NOT NULL default ' ',
  "AAAAIDKW" varchar(2) NOT NULL default ' ',
  "AAAAIDKX" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAIDLM" ("AAABMQHV","AAABMQDO","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDLQ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDWY" varchar(1) NOT NULL default ' ',
  "AAAAIDWZ" varchar(1) NOT NULL default ' ',
  "AAAAIDXA" varchar(1) NOT NULL default ' ',
  "AAAAIDXB" varchar(1) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDXD" varchar(1) NOT NULL default ' ',
  "AAAAIDXE" varchar(1) NOT NULL default ' ',
  "AAAAIDXF" varchar(1) NOT NULL default ' ',
  "AAAAIDXG" varchar(4) NOT NULL default ' ',
  "AAAAIDXH" varchar(20) NOT NULL default ' ',
  "AAAAIDXI" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  "AAAAIDXO" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
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
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAAAIVWZ" varchar(1) NOT NULL default ' ',
  "AAAAIVXA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIVXB" varchar(5) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAIDPH" varchar(62) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIDZC" varchar(2) NOT NULL default ' ',
  "AAAAIDZD" varchar(4) NOT NULL default ' ',
  "AAAAIDZE" varchar(4) NOT NULL default ' ',
  "AAAAIDZF" varchar(1) NOT NULL default ' ',
  "AAAAIDZG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAIDOI" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO"),
  KEY "AAAAIDOQ" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAIDOW" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAIDPE" ("AAABMQHZ"),
  KEY "AAAAIDPG" ("AAAAIDPH","AAABMQHZ","AAABMQHV","AAABMQDO","AAABMQHY","AAABCPQF"),
  KEY "AAAAIDPN" ("AAABMQHV","AAABMQHW","AAABMQHX","AAABMQDO","AAABMDBA","AAABMPWO","AAABMQCW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  "AAAAIDXO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIDQG" ("AAABMRIL","AAABMQDU","AAABCPWH","AAAAKOVZ","AAABMCWT","AAAAIEGN","AAAAIDXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  "AAAAIDRS" varchar(1) NOT NULL default ' ',
  "AAAAIDRT" varchar(1) NOT NULL default ' ',
  "AAAAIDRU" varchar(1) NOT NULL default ' ',
  "AAAAIDRV" varchar(1) NOT NULL default ' ',
  "AAAAIDRW" varchar(1) NOT NULL default ' ',
  "AAAAIDSC" varchar(1) NOT NULL default ' ',
  "AAAAIDRX" varchar(1) NOT NULL default ' ',
  "AAAAIDRY" varchar(1) NOT NULL default ' ',
  "AAAAIDRZ" varchar(4) NOT NULL default ' ',
  "AAAAIDSA" varchar(20) NOT NULL default ' ',
  "AAAAIDSB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIDRM" ("AAABGPZS","AAABMREX","AAABMRIL","AAABKJYV","AAAAIEBH","AAAAIDRS","AAAAIDRT","AAAAIDRU","AAAAIDRV","AAAAIDRW","AAAAIDRX","AAAAIDRY","AAAAIDRZ","AAAAIDSA","AAAAIDSB","AAAAIDSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDSD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDWY" varchar(1) NOT NULL default ' ',
  "AAAAIDWZ" varchar(1) NOT NULL default ' ',
  "AAAAIDXA" varchar(1) NOT NULL default ' ',
  "AAAAIDXB" varchar(1) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDXD" varchar(1) NOT NULL default ' ',
  "AAAAIDXE" varchar(1) NOT NULL default ' ',
  "AAAAIDXF" varchar(1) NOT NULL default ' ',
  "AAAAIDXG" varchar(4) NOT NULL default ' ',
  "AAAAIDXH" varchar(20) NOT NULL default ' ',
  "AAAAIDXI" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  "AAAAIDXO" varchar(10) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAIDVK" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAIDVT" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAAAIDVY" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAIDWG" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDWI" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAAAIDWY" varchar(1) NOT NULL default ' ',
  "AAAAIDWZ" varchar(1) NOT NULL default ' ',
  "AAAAIDXA" varchar(1) NOT NULL default ' ',
  "AAAAIDXB" varchar(1) NOT NULL default ' ',
  "AAAAIDXC" varchar(1) NOT NULL default ' ',
  "AAAAIDXD" varchar(1) NOT NULL default ' ',
  "AAAAIDXE" varchar(1) NOT NULL default ' ',
  "AAAAIDXF" varchar(1) NOT NULL default ' ',
  "AAAAIDXG" varchar(4) NOT NULL default ' ',
  "AAAAIDXH" varchar(20) NOT NULL default ' ',
  "AAAAIDXI" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
  "AAAAIDXO" varchar(10) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  "AAAAIDZC" varchar(2) NOT NULL default ' ',
  "AAAAIDZD" varchar(4) NOT NULL default ' ',
  "AAAAIDZE" varchar(4) NOT NULL default ' ',
  "AAAAIDZF" varchar(1) NOT NULL default ' ',
  "AAAAIDZG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAIDZS" ("AAABMQHV","AAABMQDO","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIDZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABDTDP" varchar(10) NOT NULL default ' ',
  "AAABMCXM" varchar(8) NOT NULL default '00000000',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAABKJYV" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABGNXB" varchar(12) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAAAIEBF" varchar(8) NOT NULL default ' ',
  "AAAAIEBH" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAIEAT" ("AAABGPZS","AAAAKOVZ","AAABDTDP","AAABMREX","AAABMRIL","AAABCPWH","AAABKJYV","AAABKRUI","AAABMCZF","AAABGNXB","AAABMCRA","AAAAIEBF","AAABMCXM","AAAAIEBH","AAABMCWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEBJ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQCG" varchar(18) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAABBTTG" varchar(12) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIEGJ" varchar(8) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIEGM" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAIEEM" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABMQDO","AAABMQHV","AAABMQCO","AAABMQHS"),
  KEY "AAAAIEEV" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ"),
  KEY "AAAAIEFA" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAIEFI" ("AAABMQHZ","AAABBTTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEFL" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHM" varchar(18) NOT NULL default ' ',
  "AAABMQHN" varchar(18) NOT NULL default ' ',
  "AAABMQHO" varchar(18) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBTSC" varchar(10) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABBTTH" varchar(8) NOT NULL default '00000000',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEGF" varchar(24) NOT NULL default ' ',
  "AAAAIEJA" varchar(16) NOT NULL default ' ',
  "AAABBTTG" varchar(12) NOT NULL default ' ',
  "AAAAIEGI" varchar(6) NOT NULL default ' ',
  "AAAAIEGJ" varchar(8) NOT NULL default ' ',
  "AAAAIEGK" varchar(16) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIEGM" varchar(10) NOT NULL default ' ',
  "AAAAIEGN" varchar(5) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABKGVD" varchar(6) NOT NULL default ' ',
  "AAABCPQF" varchar(4) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAAAIEIA" varchar(4) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAIEIN" ("AAABMQHV","AAABMQDO","AAABMQHZ"),
  KEY "AAAAIEIR" ("AAABMQHV","AAABMQDO","AAABBTTF"),
  KEY "AAAAIEIV" ("AAABMQHZ","AAABBTTF"),
  KEY "AAAAIEIY" ("AAAAIWGG","AAAAIEJA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEJM" varchar(2) NOT NULL default ' ',
  "AAAAIEJE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAIWGG" varchar(10) NOT NULL default ' ',
  "AAAAIEJM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCMO","AAABMQHZ","AAAAIWGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAAAIEJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABABEW" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAAAIEKE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABABEW","AAAAJVAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQDUI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQDUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAQDUI" varchar(2) NOT NULL default ' ',
  "AAABMOHD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAQDUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJAUE" varchar(2) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABJAUE","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIELH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABJAUE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABJAUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIELO" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAAAIELR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIELU" (
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIELX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAIEOT" varchar(4) NOT NULL default ' ',
  "AAAAIEMB" decimal(15,2) NOT NULL default '0.00',
  "AAAAIEMC" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAIEOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIEOS" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABMDBB","AAAAIEOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEMZ" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIENA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIENF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAAAIENJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAUGNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIENN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMCNU","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIEOS" varchar(10) NOT NULL default ' ',
  "AAAAIEOT" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  "AAAAIEOL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMCNU","AAABMDBB","AAAAIEOS","AAAAIEOT","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJVAY" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIEPT" varchar(24) NOT NULL default ' ',
  "AAAAIEPU" varchar(16) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAAAIEPE" varchar(16) NOT NULL default ' ',
  "AAAAIEPF" varchar(24) NOT NULL default ' ',
  "AAABLONY" decimal(5,2) NOT NULL default '0.00',
  "AAAAIEPH" varchar(24) NOT NULL default ' ',
  "AAABLONZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAIEPJ" varchar(24) NOT NULL default ' ',
  "AAAAIEPK" decimal(5,2) NOT NULL default '0.00',
  "AAAAIEPL" varchar(24) NOT NULL default ' ',
  "AAAAIEPM" decimal(5,2) NOT NULL default '0.00',
  "AAAAIEPN" varchar(24) NOT NULL default ' ',
  "AAAAIEPO" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAJVAY","AAABMDBB","AAAAIEPT","AAAAIEPU","AAAAIEPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAAAIEQE" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAAAIEQJ" varchar(8) NOT NULL default '00000000',
  "AAAAIEQK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABKOTY","AAAAIETU","AAABMCRJ","AAABMQCA","AAABMQCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAAAIEQW" varchar(1) NOT NULL default ' ',
  "AAAAIEQX" varchar(1) NOT NULL default ' ',
  "AAAAIEQY" varchar(1) NOT NULL default ' ',
  "AAAAIEQZ" varchar(1) NOT NULL default ' ',
  "AAAAIERA" varchar(1) NOT NULL default ' ',
  "AAAAIERB" varchar(1) NOT NULL default ' ',
  "AAAAIERC" varchar(1) NOT NULL default ' ',
  "AAAAIERD" varchar(1) NOT NULL default ' ',
  "AAAAIERE" varchar(1) NOT NULL default ' ',
  "AAAAIERF" varchar(1) NOT NULL default ' ',
  "AAAAIERG" varchar(1) NOT NULL default ' ',
  "AAAAIERH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIERL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMJW" varchar(24) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAAAIESI" varchar(24) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCUF" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAAAIESF" varchar(30) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMJW","AAABKOTY","AAAAIESI","AAABMCRJ","AAABMCUF","AAAAIETU","AAAAIESF"),
  KEY "AAAAIESG" ("AAABMRIL","AAAAIESI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIESJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCUF" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAAAIEUA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTY","AAABMCRJ","AAABMCUF","AAAAIETU"),
  KEY "AAAAIESX" ("AAAAIEUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIESZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCUF" varchar(3) NOT NULL default '000',
  "AAAAIETU" varchar(4) NOT NULL default '0000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABJKCJ" varchar(10) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABCPWH" varchar(16) NOT NULL default ' ',
  "AAAAIETX" varchar(24) NOT NULL default ' ',
  "AAAAIETN" varchar(30) NOT NULL default ' ',
  "AAAAIEUA" varchar(24) NOT NULL default ' ',
  "AAAAJVGP" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABKOTY","AAABMCUF","AAAAIETU"),
  KEY "AAAAIETV" ("AAABMRIL","AAAAIETX"),
  KEY "AAAAIETY" ("AAABMRIL","AAAAIEUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEUB" (
  "AAAAIEUJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIEUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEUE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAIEUJ" varchar(2) NOT NULL default ' ',
  "AAAAIEUH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAIEUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEUK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUGRQ" VARBINARY(255) NOT NULL,
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABKGPK" varchar(84) NOT NULL default ' ',
  "AAAATPFH" varchar(1) NOT NULL default ' ',
  "AAAAIEUR" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUGRQ"(255),"AAABMBXY"(255)),
  UNIQUE KEY "AAAAIEUV" ("AAABLUXQ","AAABLFYC","AAABKGPK","AAAAUGRQ"(255)),
  KEY "AAAAIEVA" ("AAABLUXQ","AAAAUGRQ"(255),"AAABLFYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEVE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUGRQ" VARBINARY(255) NOT NULL,
  "AAABLFYJ" VARBINARY(255) NOT NULL,
  "AAAAIEVO" varchar(10) NOT NULL default ' ',
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAAAIEVV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUGRQ"(255),"AAABLFYJ"(255),"AAAAIEVO","AAABMBXY"(255),"AAAAIEVV"),
  KEY "AAAAIEVR" ("AAABLUXQ","AAAAUGRQ"(255),"AAABMBXY"(255),"AAAAIEVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABJARY" varchar(12) NOT NULL default ' ',
  "AAABILTS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUSV","AAABLUFA","AAABMLBW","AAABJARY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEWI" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEWL" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAIEWO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFFT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMKFH" varchar(34) NOT NULL default ' ',
  "AAAARZCR" varchar(30) NOT NULL default ' ',
  "AAABLJNI" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAIEXA" varchar(8) NOT NULL default '00000000',
  "AAAALNWJ" varchar(50) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABLELS" varchar(1) NOT NULL default ' ',
  "AAAAUYTK" varchar(1) NOT NULL default ' ',
  "AAAAIEXF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAABJARD","AAABMKFH","AAAARZCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMKFH" varchar(34) NOT NULL default ' ',
  "AAAARZCR" varchar(30) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAABJARD","AAABMKFH","AAAARZCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAAAQXDR" varchar(34) NOT NULL default ' ',
  "AAAAIEYY" varchar(1) NOT NULL default ' ',
  "AAAAIFJT" varchar(34) NOT NULL default ' ',
  "AAABMKFU" decimal(16,0) NOT NULL default '0',
  "AAAAIEZB" varchar(10) NOT NULL default ' ',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAIFKA" varchar(1) NOT NULL default ' ',
  "AAAAIEZJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAIFKD","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAAAQXDR" varchar(34) NOT NULL default ' ',
  "AAAAIEYY" varchar(1) NOT NULL default ' ',
  "AAAAIFJT" varchar(34) NOT NULL default ' ',
  "AAABMKFU" decimal(16,0) NOT NULL default '0',
  "AAAAIEZB" varchar(10) NOT NULL default ' ',
  "AAABKWRL" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAIFKA" varchar(1) NOT NULL default ' ',
  "AAABKYQH" varchar(8) NOT NULL default '00000000',
  "AAAAIFJF" varchar(6) NOT NULL default '000000',
  "AAAAIFJG" varchar(8) NOT NULL default '00000000',
  "AAAAIFJH" varchar(1) NOT NULL default ' ',
  "AAAAIEZJ" varchar(3) NOT NULL default '000',
  "AAAAIFJI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAAAIEZS" longtext,
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEZV" (
  "AAABJGGL" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIEZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAAAIFAB" varchar(3) NOT NULL default ' ',
  "AAAAIFAC" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  "AAAAIFAL" varchar(8) NOT NULL default '00000000',
  "AAAAIFAM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" longtext,
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIFBG" varchar(3) NOT NULL default ' ',
  "AAABLECD" varchar(3) NOT NULL default ' ',
  "AAABJADR" varchar(4) NOT NULL default ' ',
  "AAABDXHF" varchar(20) NOT NULL default ' ',
  "AAABLTWR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIFBG","AAABLECD","AAABJADR","AAABDXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTWR" varchar(4) NOT NULL default '0000',
  "AAAAMBVB" varchar(8) NOT NULL default '00000000',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAAARLUQ" varchar(1) NOT NULL default '0',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTWR","AAAAMBVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFBU" (
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAAARFNK" varchar(1) NOT NULL default ' ',
  "AAABJSLF" varchar(30) NOT NULL default ' ',
  "AAAAIFBY" varchar(30) NOT NULL default ' ',
  "AAAAIFBZ" varchar(30) NOT NULL default ' ',
  "AAAAIFCA" varchar(30) NOT NULL default ' ',
  "AAAAIFCB" varchar(30) NOT NULL default ' ',
  "AAAAIFCC" varchar(30) NOT NULL default ' ',
  "AAAAIFCD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFKD","AAAARFNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFCG" (
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABLQDE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFCK" (
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAIFCN" varchar(1) NOT NULL default ' ',
  "AAABLVBE" varchar(1) NOT NULL default ' ',
  "AAAAIFCP" varchar(1) NOT NULL default ' ',
  "AAAAIFCQ" varchar(1) NOT NULL default ' ',
  "AAAAVQDY" varchar(30) NOT NULL default ' ',
  "AAAAIFCS" varchar(30) NOT NULL default ' ',
  "AAAAIFCT" varchar(10) NOT NULL default ' ',
  "AAAAIFCU" varchar(10) NOT NULL default ' ',
  "AAABLQDV" varchar(20) NOT NULL default ' ',
  "AAABLQDW" varchar(20) NOT NULL default ' ',
  "AAAAQVJY" varchar(20) NOT NULL default ' ',
  "AAAAIFCY" varchar(1) NOT NULL default ' ',
  "AAABLQDT" varchar(30) NOT NULL default ' ',
  "AAABLQDU" varchar(30) NOT NULL default ' ',
  "AAAAIFDB" varchar(30) NOT NULL default ' ',
  "AAAAIFDC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLQEP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFDF" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABJCAB" varchar(2) NOT NULL default '00',
  "AAAAIFEE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAIFFT","AAABJCAB","AAAAIFEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFDM" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABJCAB" varchar(2) NOT NULL default '00',
  "AAAAIFEE" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAIFDR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFFT","AAABJCAB","AAAAIFEE","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFDW" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAIFEA" varchar(4) NOT NULL default '0000',
  "AAAALPYO" varchar(1) NOT NULL default ' ',
  "AAAAIFEC" varchar(1) NOT NULL default ' ',
  "AAABJCAB" varchar(2) NOT NULL default '00',
  "AAAAIFEE" varchar(2) NOT NULL default '00',
  "AAAAIFEF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFFT","AAABLQEP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQEN" varchar(1) NOT NULL default ' ',
  "AAABLQEO" varchar(30) NOT NULL default ' ',
  "AAABLQEP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLSDL" varchar(1) NOT NULL default ' ',
  "AAABLRXO" varchar(8) NOT NULL default '00000000',
  "AAABLQER" varchar(6) NOT NULL default '000000',
  "AAABLRXN" varchar(8) NOT NULL default '00000000',
  "AAAAIFET" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABLQEI" varchar(1) NOT NULL default ' ',
  "AAABLQEJ" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(60) NOT NULL default ' ',
  "AAABMNNA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQEN","AAABLQEO","AAABLQEP","AAABMREF","AAABLSDL","AAABLRXO","AAABLQER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFFJ" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABJCAB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAIFFT","AAABJCAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFFO" (
  "AAAAIFFT" varchar(30) NOT NULL default ' ',
  "AAABJCAB" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAMFDL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFFT","AAABJCAB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNQK" varchar(10) NOT NULL default ' ',
  "AAAAVLJZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLBW" varchar(10) NOT NULL default ' ',
  "AAABJARY" varchar(12) NOT NULL default ' ',
  "AAABLKJW" varchar(10) NOT NULL default ' ',
  "AAABINER" smallint(6) NOT NULL default '0',
  "AAABINDH" varchar(40) NOT NULL default ' ',
  "AAABINDF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLBW","AAABJARY","AAABLKJW","AAABINER","AAABINDH","AAABINDF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMKFH" varchar(34) NOT NULL default ' ',
  "AAABEINQ" varchar(1) NOT NULL default ' ',
  "AAAAIFGY" varchar(1) NOT NULL default ' ',
  "AAAAIFGZ" varchar(1) NOT NULL default ' ',
  "AAAAVFVO" decimal(15,2) NOT NULL default '0.00',
  "AAAAVFVP" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAIFHE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHI" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHK" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHL" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHM" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHN" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHO" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHP" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHR" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHS" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHT" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHU" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHV" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHW" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHX" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHY" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFHZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIB" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIC" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFID" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIE" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIF" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIG" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIH" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFII" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFIJ" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAABJARD","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAAAQXDR" varchar(34) NOT NULL default ' ',
  "AAAAIFJT" varchar(34) NOT NULL default ' ',
  "AAAAVQGU" varchar(1) NOT NULL default ' ',
  "AAAAIFJV" varchar(10) NOT NULL default ' ',
  "AAAAIFJW" varchar(10) NOT NULL default ' ',
  "AAAAIFJX" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAIFJZ" decimal(10,1) NOT NULL default '0.0',
  "AAAAIFKA" varchar(1) NOT NULL default ' ',
  "AAABKYQH" varchar(8) NOT NULL default '00000000',
  "AAAAIFJF" varchar(6) NOT NULL default '000000',
  "AAAAIFJG" varchar(8) NOT NULL default '00000000',
  "AAAAIFJH" varchar(1) NOT NULL default ' ',
  "AAAAIFJI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  "AAAAQXDR" varchar(34) NOT NULL default ' ',
  "AAAAIFJT" varchar(34) NOT NULL default ' ',
  "AAAAVQGU" varchar(1) NOT NULL default ' ',
  "AAAAIFJV" varchar(10) NOT NULL default ' ',
  "AAAAIFJW" varchar(10) NOT NULL default ' ',
  "AAAAIFJX" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAIFJZ" decimal(10,1) NOT NULL default '0.0',
  "AAAAIFKA" varchar(1) NOT NULL default ' ',
  "AAAAIFKB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIFKD","AAABJARD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFKF" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMMYV" varchar(30) NOT NULL default ' ',
  "AAAAVEXY" varchar(12) NOT NULL default ' ',
  "AAABMKOT" varchar(8) NOT NULL default '00000000',
  "AAAAVEYA" varchar(6) NOT NULL default '000000',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABLMOZ" longblob,
  PRIMARY KEY ("AAABMLPV","AAABLVCU"),
  KEY "AAAAIFKT" ("AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFKV" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAIFKY" varchar(8) NOT NULL default '00000000',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAAAIFLA" varchar(8) NOT NULL default '00000000',
  "AAAAIFLB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMLPV","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFLE" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLPV","AAABLVCU","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFLP" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLVCU","AAABLFZK","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFLZ" (
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUZL" varchar(255) NOT NULL default ' ',
  "AAABHIXN" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFMG" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLNEX" varchar(1) NOT NULL default ' ',
  "AAAAVEXY" varchar(12) NOT NULL default ' ',
  "AAABMKOT" varchar(8) NOT NULL default '00000000',
  "AAAAVEYA" varchar(6) NOT NULL default '000000',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABMMYV" longblob,
  PRIMARY KEY ("AAABMLPV","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFMT" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABLVCU","AAABLFZK","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKPVU" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABLCDY" varchar(10) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABKPVY" varchar(8) NOT NULL default '00000000',
  "AAABKPVZ" varchar(2) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKPWB" varchar(2) NOT NULL default ' ',
  "AAABKPWC" varchar(2) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABKPWF" varchar(10) NOT NULL default ' ',
  "AAABKPWG" varchar(8) NOT NULL default '00000000',
  "AAABKPWH" varchar(1) NOT NULL default ' ',
  "AAABKPWI" varchar(2) NOT NULL default ' ',
  "AAABKPWJ" varchar(2) NOT NULL default ' ',
  "AAABKPYO" varchar(2) NOT NULL default ' ',
  "AAABKPWL" varchar(8) NOT NULL default '00000000',
  "AAABKPWM" varchar(2) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABKPWS" varchar(6) NOT NULL default '000000',
  "AAABKPWT" varchar(1) NOT NULL default ' ',
  "AAABKPWU" varchar(1) NOT NULL default ' ',
  "AAABKPWV" varchar(1) NOT NULL default ' ',
  "AAABKPWW" varchar(1) NOT NULL default ' ',
  "AAABKPWX" varchar(1) NOT NULL default ' ',
  "AAABKPWY" decimal(15,2) NOT NULL default '0.00',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABKPXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSSD"),
  KEY "AAAAIFOP" ("AAABMCXK","AAABMCXL"),
  KEY "AAAAIFOS" ("AAABKRZZ","AAABLMJL"),
  KEY "AAAAIFOV" ("AAABMRIL","AAABKRJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKPVU" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABLCDY" varchar(10) NOT NULL default ' ',
  "AAABLUHU" varchar(8) NOT NULL default '00000000',
  "AAABKPVY" varchar(8) NOT NULL default '00000000',
  "AAABKPVZ" varchar(2) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKPWB" varchar(2) NOT NULL default ' ',
  "AAABKPWC" varchar(2) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABKPWF" varchar(10) NOT NULL default ' ',
  "AAABKPWG" varchar(8) NOT NULL default '00000000',
  "AAABKPWH" varchar(1) NOT NULL default ' ',
  "AAABKPWI" varchar(2) NOT NULL default ' ',
  "AAABKPWJ" varchar(2) NOT NULL default ' ',
  "AAABKPYO" varchar(2) NOT NULL default ' ',
  "AAABKPWL" varchar(8) NOT NULL default '00000000',
  "AAABKPWM" varchar(2) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABKPWS" varchar(6) NOT NULL default '000000',
  "AAABKPWT" varchar(1) NOT NULL default ' ',
  "AAABKPWU" varchar(1) NOT NULL default ' ',
  "AAABKPWV" varchar(1) NOT NULL default ' ',
  "AAABKPWW" varchar(1) NOT NULL default ' ',
  "AAABKPWX" varchar(1) NOT NULL default ' ',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABKPXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSSD"),
  KEY "AAAAIFQJ" ("AAABMCXK","AAABMCXL"),
  KEY "AAAAIFQM" ("AAABKRZZ","AAABLMJL"),
  KEY "AAAAIFQP" ("AAABMRIL","AAABKRJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFQS" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAVEXY" varchar(12) NOT NULL default ' ',
  "AAABMKOT" varchar(8) NOT NULL default '00000000',
  "AAAAVEYA" varchar(6) NOT NULL default '000000',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABHIYE" varchar(8) NOT NULL default '00000000',
  "AAABHIYF" varchar(6) NOT NULL default '000000',
  "AAABLROG" longblob,
  PRIMARY KEY ("AAABMLPV","AAABLVCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFRE" (
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(32) NOT NULL default ' ',
  "AAABLROG" longblob,
  PRIMARY KEY ("AAABMLPV","AAABLVCU","AAABLFZK","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFRO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKPZO" varchar(6) NOT NULL default '000000',
  "AAABKPXJ" varchar(2) NOT NULL default ' ',
  "AAABKPXK" varchar(4) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABKPXM" varchar(1) NOT NULL default ' ',
  "AAABKPXN" varchar(1) NOT NULL default ' ',
  "AAABMFTE" decimal(5,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKPXQ" decimal(9,5) NOT NULL default '0.00000',
  "AAABKPXR" decimal(15,2) NOT NULL default '0.00',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABKPXU" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABKQOV" varchar(12) NOT NULL default '000000000000',
  "AAABKPXY" varchar(5) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLXKG" varchar(1) NOT NULL default ' ',
  "AAABLXKH" varchar(1) NOT NULL default ' ',
  "AAABLXKI" varchar(1) NOT NULL default ' ',
  "AAABLXKJ" varchar(1) NOT NULL default ' ',
  "AAABLXKK" varchar(1) NOT NULL default ' ',
  "AAABLXKL" varchar(1) NOT NULL default ' ',
  "AAABLXKM" varchar(1) NOT NULL default ' ',
  "AAABLXKN" varchar(1) NOT NULL default ' ',
  "AAABLXKO" varchar(1) NOT NULL default ' ',
  "AAABLWYT" varchar(2) NOT NULL default '00',
  "AAABLWYU" varchar(8) NOT NULL default '00000000',
  "AAABKPYM" varchar(8) NOT NULL default '00000000',
  "AAABKPYN" varchar(1) NOT NULL default ' ',
  "AAABKPYO" varchar(2) NOT NULL default ' ',
  "AAABLMEG" varchar(8) NOT NULL default '00000000',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABLXSB" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSC" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSE" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSG" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSH" decimal(13,2) NOT NULL default '0.00',
  "AAABLXTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLJEV" decimal(13,2) NOT NULL default '0.00',
  "AAABLXDF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXRU" varchar(1) NOT NULL default ' ',
  "AAABKPZD" varchar(1) NOT NULL default ' ',
  "AAABLXUW" double NOT NULL default '0',
  "AAABKPZF" varchar(10) NOT NULL default ' ',
  "AAABKPZG" varchar(6) NOT NULL default '000000',
  "AAABLIIW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKPZI" varchar(10) NOT NULL default ' ',
  "AAABKPZJ" varchar(1) NOT NULL default ' ',
  "AAABKPZK" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKSSD","AAABKPZO"),
  KEY "AAAAIFUG" ("AAABMRIL","AAABKQOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKPZO" varchar(6) NOT NULL default '000000',
  "AAABLQPP" varchar(4) NOT NULL default ' ',
  "AAABJLRA" varchar(25) NOT NULL default ' ',
  "AAAAIFUP" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABJLQR" varchar(40) NOT NULL default ' ',
  "AAAAIFUT" varchar(1) NOT NULL default ' ',
  "AAAAIFUU" decimal(15,2) NOT NULL default '0.00',
  "AAABDEBU" varchar(5) NOT NULL default ' ',
  "AAAAIFUW" varchar(1) NOT NULL default ' ',
  "AAAAIFUX" varchar(10) NOT NULL default ' ',
  "AAAAIFUY" varchar(15) NOT NULL default ' ',
  "AAABLXHO" varchar(8) NOT NULL default '00000000',
  "AAAAIFVA" varchar(6) NOT NULL default '000000',
  "AAABLQPQ" varchar(15) NOT NULL default ' ',
  "AAABKXSJ" varchar(10) NOT NULL default ' ',
  "AAAAIFVD" varchar(10) NOT NULL default ' ',
  "AAABMCZU" varchar(10) NOT NULL default ' ',
  "AAABLKKE" varchar(2) NOT NULL default ' ',
  "AAAAIFVG" varchar(1) NOT NULL default ' ',
  "AAAAIFVH" varchar(1) NOT NULL default ' ',
  "AAABLEEQ" varchar(1) NOT NULL default ' ',
  "AAAAIFVJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIFVK" varchar(1) NOT NULL default ' ',
  "AAAAIFVL" varchar(1) NOT NULL default ' ',
  "AAAAIFVM" varchar(1) NOT NULL default ' ',
  "AAAAIFVN" varchar(6) NOT NULL default '000000',
  "AAAAIFVO" varchar(4) NOT NULL default ' ',
  "AAAAIFVP" varchar(4) NOT NULL default ' ',
  "AAAAIFVQ" varchar(4) NOT NULL default ' ',
  "AAAAIFVR" varchar(4) NOT NULL default ' ',
  "AAABLPUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSSD","AAABKPZO"),
  KEY "AAAAIFVW" ("AAABMRIL","AAABJLRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSSD" varchar(10) NOT NULL default ' ',
  "AAABKPZO" varchar(6) NOT NULL default '000000',
  "AAABKPXJ" varchar(2) NOT NULL default ' ',
  "AAABKPXK" varchar(4) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABKPXM" varchar(1) NOT NULL default ' ',
  "AAABKPXN" varchar(1) NOT NULL default ' ',
  "AAABMFTE" decimal(5,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKPXQ" decimal(9,5) NOT NULL default '0.00000',
  "AAABKPXR" decimal(15,2) NOT NULL default '0.00',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABKPXU" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABKQOV" varchar(12) NOT NULL default '000000000000',
  "AAABKPXY" varchar(5) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABLXKG" varchar(1) NOT NULL default ' ',
  "AAABLXKH" varchar(1) NOT NULL default ' ',
  "AAABLXKI" varchar(1) NOT NULL default ' ',
  "AAABLXKJ" varchar(1) NOT NULL default ' ',
  "AAABLXKK" varchar(1) NOT NULL default ' ',
  "AAABLXKL" varchar(1) NOT NULL default ' ',
  "AAABLXKM" varchar(1) NOT NULL default ' ',
  "AAABLXKN" varchar(1) NOT NULL default ' ',
  "AAABLXKO" varchar(1) NOT NULL default ' ',
  "AAABLWYT" varchar(2) NOT NULL default '00',
  "AAABLWYU" varchar(8) NOT NULL default '00000000',
  "AAABKPYM" varchar(8) NOT NULL default '00000000',
  "AAABKPYN" varchar(1) NOT NULL default ' ',
  "AAABKPYO" varchar(2) NOT NULL default ' ',
  "AAABLMEG" varchar(8) NOT NULL default '00000000',
  "AAABLXPL" decimal(15,2) NOT NULL default '0.00',
  "AAABLXRI" decimal(11,2) NOT NULL default '0.00',
  "AAABLXSB" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSC" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSD" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSE" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSG" decimal(13,2) NOT NULL default '0.00',
  "AAABLXSH" decimal(13,2) NOT NULL default '0.00',
  "AAABLXTJ" decimal(11,2) NOT NULL default '0.00',
  "AAABLJEV" decimal(13,2) NOT NULL default '0.00',
  "AAABLXDF" decimal(13,2) NOT NULL default '0.00',
  "AAABLXRU" varchar(1) NOT NULL default ' ',
  "AAABKPZD" varchar(1) NOT NULL default ' ',
  "AAABLXUW" double NOT NULL default '0',
  "AAABKPZF" varchar(10) NOT NULL default ' ',
  "AAABKPZG" varchar(6) NOT NULL default '000000',
  "AAABLIIW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKPZI" varchar(10) NOT NULL default ' ',
  "AAABKPZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSSD","AAABKPZO"),
  KEY "AAAAIFYH" ("AAABMRIL","AAABKQOV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFYK" (
  "AAABLRQQ" varchar(2) NOT NULL default ' ',
  "AAABLBFZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLRQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFYO" (
  "AAABGGGL" varchar(8) NOT NULL default ' ',
  "AAAAKFZL" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFYT" (
  "AAAAIFYW" varchar(30) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIFYW","AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIFYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGBL" varchar(1) NOT NULL default ' ',
  "AAAAIGBM" varchar(10) NOT NULL default ' ',
  "AAAAIGBN" varchar(6) NOT NULL default '000000',
  "AAAAIGAP" varchar(1) NOT NULL default ' ',
  "AAAAIGAQ" varchar(10) NOT NULL default ' ',
  "AAAAIGAR" varchar(6) NOT NULL default '000000',
  "AAABHSJX" varchar(2) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAIGBG" decimal(13,3) NOT NULL default '0.000',
  "AAAAIGBH" varchar(3) NOT NULL default ' ',
  "AAAAIFZK" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZL" varchar(3) NOT NULL default ' ',
  "AAAAIFZM" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZN" varchar(3) NOT NULL default ' ',
  "AAAAIFZO" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZP" varchar(3) NOT NULL default ' ',
  "AAABGACU" varchar(1) NOT NULL default ' ',
  "AAAAIFZR" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZS" varchar(3) NOT NULL default ' ',
  "AAAAIFZT" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZU" varchar(3) NOT NULL default ' ',
  "AAABGACV" varchar(1) NOT NULL default ' ',
  "AAAAIFZW" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZX" varchar(3) NOT NULL default ' ',
  "AAAAIFZY" decimal(13,3) NOT NULL default '0.000',
  "AAAAIFZZ" varchar(3) NOT NULL default ' ',
  "AAABKYIM" varchar(4) NOT NULL default ' ',
  "AAAAIGAB" varchar(2) NOT NULL default ' ',
  "AAABLLVG" varchar(10) NOT NULL default ' ',
  "AAABLRYZ" varchar(1) NOT NULL default ' ',
  "AAAAIGAE" varchar(1) NOT NULL default ' ',
  "AAAAIGBI" varchar(1) NOT NULL default ' ',
  "AAABLXUM" varchar(1) NOT NULL default ' ',
  "AAAAIGAH" varchar(1) NOT NULL default ' ',
  "AAAALYMF" varchar(8) NOT NULL default '00000000',
  "AAAAIGAJ" varchar(12) NOT NULL default ' ',
  "AAAAIGAK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAIGBL","AAAAIGBM","AAAAIGBN","AAAAIGAP","AAAAIGAQ","AAAAIGAR"),
  KEY "AAAAIGAS" ("AAABMRIL","AAABKYIM","AAAAIGBI","AAABMLCZ","AAABLLVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGBL" varchar(1) NOT NULL default ' ',
  "AAAAIGBM" varchar(10) NOT NULL default ' ',
  "AAAAIGBN" varchar(6) NOT NULL default '000000',
  "AAAAIGBT" varchar(1) NOT NULL default ' ',
  "AAAAIGBU" varchar(10) NOT NULL default ' ',
  "AAAAIGBV" varchar(6) NOT NULL default '000000',
  "AAAAIGBG" decimal(13,3) NOT NULL default '0.000',
  "AAAAIGBH" varchar(3) NOT NULL default ' ',
  "AAAAIGBI" varchar(1) NOT NULL default ' ',
  "AAAAWUWQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAIGBL","AAAAIGBM","AAAAIGBN","AAAAIGBT","AAAAIGBU","AAAAIGBV"),
  KEY "AAAAIGBR" ("AAABMRIL","AAAAIGBT","AAAAIGBU","AAAAIGBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAAAIGCB" varchar(8) NOT NULL default '00000000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAIGCD" decimal(13,3) NOT NULL default '0.000',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAIGCF" varchar(8) NOT NULL default '00000000',
  "AAAAIGCG" varchar(1) NOT NULL default ' ',
  "AAAAIGCH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQWB" varchar(2) NOT NULL default ' ',
  "AAAAIGCO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAIGCZ" decimal(6,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGDH" varchar(2) NOT NULL default ' ',
  "AAABMMJT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGDU" varchar(3) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGDU","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGDU" varchar(3) NOT NULL default ' ',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGDU","AAABLUFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGFW" varchar(10) NOT NULL default ' ',
  "AAAAIGFZ" varchar(2) NOT NULL default '00',
  "AAAANRFL" varchar(10) NOT NULL default '0000000000',
  "AAAAIGEX" varchar(20) NOT NULL default ' ',
  "AAAAIGEY" varchar(20) NOT NULL default ' ',
  "AAAAIGGY" varchar(2) NOT NULL default ' ',
  "AAAAIGFO" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABIYQF" varchar(3) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGFW","AAAAIGFZ","AAAANRFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGFW" varchar(10) NOT NULL default ' ',
  "AAAAIGFX" varchar(5) NOT NULL default '00000',
  "AAABAVDA" varchar(4) NOT NULL default '0000',
  "AAAAIGFZ" varchar(2) NOT NULL default '00',
  "AAAANRFL" varchar(10) NOT NULL default '0000000000',
  "AAAAQIZQ" varchar(10) NOT NULL default '0000000000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAAIGEV" varchar(6) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAIGEX" varchar(20) NOT NULL default ' ',
  "AAAAIGEY" varchar(20) NOT NULL default ' ',
  "AAABMKAH" decimal(13,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAAAIGFB" decimal(13,3) NOT NULL default '0.000',
  "AAAAIGFC" varchar(3) NOT NULL default ' ',
  "AAAAIGFD" decimal(13,3) NOT NULL default '0.000',
  "AAAAIGFE" varchar(3) NOT NULL default ' ',
  "AAAAIGFF" decimal(15,0) NOT NULL default '0',
  "AAAAIGFG" decimal(15,0) NOT NULL default '0',
  "AAAAIGFH" decimal(15,0) NOT NULL default '0',
  "AAAAIGFI" decimal(15,0) NOT NULL default '0',
  "AAAAIGFJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIGFK" varchar(5) NOT NULL default ' ',
  "AAAAIGFL" varchar(1) NOT NULL default ' ',
  "AAAAIGGY" varchar(2) NOT NULL default ' ',
  "AAAAIGFN" varchar(1) NOT NULL default ' ',
  "AAAAIGFO" varchar(1) NOT NULL default ' ',
  "AAAAIGHA" varchar(3) NOT NULL default ' ',
  "AAAAIGFQ" varchar(3) NOT NULL default ' ',
  "AAAAIGFR" varchar(3) NOT NULL default ' ',
  "AAAAIGHC" varchar(3) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABEDJP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGFW","AAAAIGFX","AAABAVDA","AAAAIGFZ","AAAANRFL","AAAAQIZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAAAIGGY" varchar(2) NOT NULL default ' ',
  "AAAAIGGJ" varchar(1) NOT NULL default ' ',
  "AAAAIGHB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLURT" varchar(6) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAIGGY" varchar(2) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  "AAAAIGHA" varchar(3) NOT NULL default ' ',
  "AAAAIGHB" varchar(2) NOT NULL default ' ',
  "AAAAIGHC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP","AAABMCVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAAANRFL" varchar(10) NOT NULL default '0000000000',
  "AAAAQIZQ" varchar(10) NOT NULL default '0000000000',
  "AAABLUFD" varchar(4) NOT NULL default ' ',
  "AAABLUHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABMLEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQXER" varchar(15) NOT NULL default ' ',
  "AAABLEJW" varchar(30) NOT NULL default ' ',
  "AAAAIGHZ" varchar(3) NOT NULL default ' ',
  "AAAAIGIA" varchar(1) NOT NULL default ' ',
  "AAAATHPD" varchar(8) NOT NULL default '00000000',
  "AAAATHPE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAQXER","AAABLEJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGII" varchar(2) NOT NULL default '00',
  "AAAAIGIJ" varchar(2) NOT NULL default '00',
  "AAAAIGIZ" varchar(2) NOT NULL default '00',
  "AAAAIGIL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAAAIGIZ" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAIGIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABMLEK","AAAAIGIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGJC" varchar(1) NOT NULL default ' ',
  "AAAAIGJD" varchar(1) NOT NULL default ' ',
  "AAAAIGJE" varchar(1) NOT NULL default ' ',
  "AAAAIGJF" varchar(1) NOT NULL default ' ',
  "AAAAIGJG" varchar(1) NOT NULL default ' ',
  "AAAAIGJH" varchar(32) NOT NULL default ' ',
  "AAAAIGJI" decimal(9,0) NOT NULL default '0',
  "AAAAIGJJ" varchar(10) NOT NULL default ' ',
  "AAAAIGJK" varchar(60) NOT NULL default ' ',
  "AAAAIGJL" varchar(60) NOT NULL default ' ',
  "AAAAIGJM" varchar(60) NOT NULL default ' ',
  "AAAAIGJN" decimal(3,0) NOT NULL default '0',
  "AAAAIGJO" varchar(20) NOT NULL default ' ',
  "AAAAIGJP" int(11) NOT NULL default '0',
  "AAAAIGJQ" varchar(3) NOT NULL default '000',
  "AAAAIGJR" varchar(3) NOT NULL default '000',
  "AAAAIGJS" varchar(1) NOT NULL default ' ',
  "AAAAIGJT" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGKA" varchar(30) NOT NULL default ' ',
  "AAAAIGJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMYV" varchar(3) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABHBMG" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABKNBI" varchar(220) NOT NULL default ' ',
  "AAAAQHKA" varchar(20) NOT NULL default ' ',
  "AAAAQHKB" varchar(6) NOT NULL default '000000',
  "AAABINHC" varchar(50) NOT NULL default ' ',
  "AAABINHD" varchar(50) NOT NULL default ' ',
  "AAABINHE" varchar(50) NOT NULL default ' ',
  "AAABINHF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMYV","AAABMKFU","AAABMKFH","AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGKW" (
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGLO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGLA" (
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIHGI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHDY" varchar(1) NOT NULL default ' ',
  "AAAAIGLL" varchar(40) NOT NULL default ' ',
  "AAAAIGLM" varchar(4) NOT NULL default ' ',
  "AAAAIGLN" varchar(30) NOT NULL default ' ',
  "AAAAIGLO" varchar(2) NOT NULL default '00',
  "AAAAIGVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAIGVN" varchar(20) NOT NULL default ' ',
  "AAAAIGVO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGMU" varchar(10) NOT NULL default ' ',
  "AAAAJQRO" varchar(10) NOT NULL default ' ',
  "AAAAIGMF" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAAAIGMH" varchar(4) NOT NULL default '0000',
  "AAAAIGMI" varchar(1) NOT NULL default ' ',
  "AAAAIGMJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGMU","AAAAJQRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGMU" varchar(10) NOT NULL default ' ',
  "AAAAJQRO" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGMU","AAAAJQRO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGWL" varchar(10) NOT NULL default ' ',
  "AAAAIGWM" varchar(10) NOT NULL default ' ',
  "AAAAIGYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAAAIGWL","AAAAIGWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGWL" varchar(10) NOT NULL default ' ',
  "AAAAIGWM" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAAAIGWL","AAAAIGWM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHRE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAABERXO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGWL" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAAAIGWL","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAAAIGOY" varchar(1) NOT NULL default ' ',
  "AAAAIGOZ" varchar(1) NOT NULL default ' ',
  "AAAAQISZ" varchar(1) NOT NULL default ' ',
  "AAAAQITB" varchar(1) NOT NULL default ' ',
  "AAAAQITC" varchar(1) NOT NULL default ' ',
  "AAAAQITD" varchar(1) NOT NULL default ' ',
  "AAAAQITE" varchar(1) NOT NULL default ' ',
  "AAABMKAI" varchar(1) NOT NULL default ' ',
  "AAAAQITG" varchar(1) NOT NULL default ' ',
  "AAAAQITH" varchar(1) NOT NULL default ' ',
  "AAABCDDP" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABLUFA" varchar(1) NOT NULL default ' ',
  "AAAAQITL" varchar(1) NOT NULL default ' ',
  "AAAAIGPM" varchar(1) NOT NULL default ' ',
  "AAABDGPY" varchar(30) NOT NULL default ' ',
  "AAAAIGPO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDHRE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJWN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(5) NOT NULL default ' ',
  "AAAALJNG" varchar(2) NOT NULL default ' ',
  "AAABDHHV" varchar(1) NOT NULL default ' ',
  "AAABDHHW" varchar(1) NOT NULL default ' ',
  "AAAAIGQF" varchar(1) NOT NULL default ' ',
  "AAABDHHX" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFO","AAAALJNG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGQQ" varchar(2) NOT NULL default '00',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGQQ","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGQX" varchar(3) NOT NULL default ' ',
  "AAAAIGQY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGQX","AAAAIGQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGQZ" (
  "AAAAUZGD" varchar(3) NOT NULL default ' ',
  "AAABGPQK" varchar(26) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAAAUZGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGRE" (
  "AAAAIGRH" varchar(3) NOT NULL default ' ',
  "AAAAIGRI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIGRH","AAAAIGRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGRJ" (
  "AAAAIGRQ" varchar(3) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAAAIGRN" varchar(1) NOT NULL default ' ',
  "AAAAIGRO" varchar(20) NOT NULL default ' ',
  "AAAAIGRP" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAIGRQ","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGRS" (
  "AAAAUZGD" varchar(3) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAAAIGRV" varchar(3) NOT NULL default ' ',
  "AAAAIGRW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAUZGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGSL" (
  "AAAAVBLW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVBLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGSO" (
  "AAAAVBLW" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAVBLW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLROG" varchar(8) NOT NULL default ' ',
  "AAAAIGTE" varchar(10) NOT NULL default ' ',
  "AAAAVBLW" varchar(30) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABBORB" varchar(1) NOT NULL default ' ',
  "AAABFUQE" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLROG","AAAAIGTE","AAAAVBLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGUU" varchar(10) NOT NULL default ' ',
  "AAAAIGTJ" varchar(30) NOT NULL default ' ',
  "AAAAIGTK" varchar(30) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABLRQP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGUU" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGUU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGUT" varchar(10) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAYVAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGUT" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIGUT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIGUT" varchar(10) NOT NULL default ' ',
  "AAAAIGUU" varchar(10) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAIGUT","AAAAIGUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAAAIGUZ" varchar(1) NOT NULL default ' ',
  "AAAAIGVA" varchar(1) NOT NULL default ' ',
  "AAAAIGVB" varchar(1) NOT NULL default ' ',
  "AAAAIGVC" varchar(40) NOT NULL default ' ',
  "AAAAIGVD" varchar(4) NOT NULL default ' ',
  "AAAAIGVE" varchar(30) NOT NULL default ' ',
  "AAAAIGVF" varchar(30) NOT NULL default ' ',
  "AAAAIGVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAIGVN" varchar(20) NOT NULL default ' ',
  "AAAAIGVO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGWL" varchar(10) NOT NULL default ' ',
  "AAAAIGWM" varchar(10) NOT NULL default ' ',
  "AAAAIGYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAAAIGWL","AAAAIGWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGWL" varchar(10) NOT NULL default ' ',
  "AAAAIGWM" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAAAIGWL","AAAAIGWM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDHOZ","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAIGXB" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHOZ","AAABERXX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTTE" varchar(8) NOT NULL default ' ',
  "AAAAIGXJ" varchar(1) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGTTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGTTE" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGTTE","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIGYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABJTGK" varchar(5) NOT NULL default ' ',
  "AAAAIGYO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAABJTGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAABJTGK" varchar(5) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLPSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI","AAABJTGK","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGZC" (
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIHHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAIGZK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHHR","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAAAIGZR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIGZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAAAIHDI" varchar(1) NOT NULL default ' ',
  "AAAAIHDJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHAN" varchar(10) NOT NULL default ' ',
  "AAAAIHAG" varchar(1) NOT NULL default ' ',
  "AAAAIHAH" varchar(1) NOT NULL default ' ',
  "AAAAIHAI" varchar(1) NOT NULL default ' ',
  "AAAAIHAJ" varchar(1) NOT NULL default ' ',
  "AAAAIHAK" varchar(1) NOT NULL default ' ',
  "AAAAIHAL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAIHAS" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHHR","AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABDHPD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHBK" (
  "AAAAIHBZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIHBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHBZ" varchar(5) NOT NULL default ' ',
  "AAABCPHI" varchar(1) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHBU" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAIHBZ" varchar(5) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAIHBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHCA" (
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAUZGD" varchar(3) NOT NULL default ' ',
  "AAAAIHCD" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIHGI","AAAAUZGD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAAAIHCK" varchar(10) NOT NULL default ' ',
  "AAAAIHCL" varchar(10) NOT NULL default ' ',
  "AAAAIHCM" varchar(10) NOT NULL default ' ',
  "AAAAIHCN" varchar(30) NOT NULL default ' ',
  "AAAAIHCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAABMNMP" tinyint(3) unsigned NOT NULL default '0',
  "AAABDHPD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWK" varchar(3) NOT NULL default ' ',
  "AAABMNRG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABJGWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(10) NOT NULL default ' ',
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAAAIHDI" varchar(1) NOT NULL default ' ',
  "AAAAIHDJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAAAIHHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAAATJAQ" varchar(1) NOT NULL default ' ',
  "AAAAIHDR" varchar(1) NOT NULL default ' ',
  "AAAAIHDS" varchar(1) NOT NULL default ' ',
  "AAAAIHDT" varchar(1) NOT NULL default ' ',
  "AAAAIHDU" varchar(1) NOT NULL default ' ',
  "AAAAIHDV" varchar(1) NOT NULL default ' ',
  "AAAAIHDW" varchar(1) NOT NULL default ' ',
  "AAAAIHDX" varchar(1) NOT NULL default ' ',
  "AAAAIHDY" varchar(1) NOT NULL default ' ',
  "AAAAIHDZ" varchar(1) NOT NULL default ' ',
  "AAAAIHEA" varchar(1) NOT NULL default ' ',
  "AAAAIHEB" varchar(1) NOT NULL default ' ',
  "AAAAIHEC" varchar(1) NOT NULL default ' ',
  "AAAAIHED" varchar(1) NOT NULL default ' ',
  "AAAAIHEE" varchar(1) NOT NULL default ' ',
  "AAAAIHEF" varchar(1) NOT NULL default ' ',
  "AAAAIHEG" varchar(1) NOT NULL default ' ',
  "AAAAIHEH" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(1) NOT NULL default ' ',
  "AAAAIHFQ" varchar(1) NOT NULL default ' ',
  "AAAAIHEK" varchar(1) NOT NULL default ' ',
  "AAAAIHEL" varchar(1) NOT NULL default ' ',
  "AAAAIHEM" varchar(1) NOT NULL default ' ',
  "AAAAIHEN" varchar(1) NOT NULL default ' ',
  "AAAAIHEO" varchar(1) NOT NULL default ' ',
  "AAAAIHEP" varchar(1) NOT NULL default ' ',
  "AAAAIHEQ" varchar(1) NOT NULL default ' ',
  "AAAAIHER" varchar(1) NOT NULL default ' ',
  "AAAAIHES" varchar(1) NOT NULL default ' ',
  "AAAAIHET" varchar(1) NOT NULL default ' ',
  "AAAAIHEU" varchar(1) NOT NULL default ' ',
  "AAAAIHEV" varchar(1) NOT NULL default ' ',
  "AAAAIHEW" varchar(1) NOT NULL default ' ',
  "AAAAIHEX" varchar(1) NOT NULL default ' ',
  "AAAAIHEY" varchar(1) NOT NULL default ' ',
  "AAAAIHEZ" varchar(1) NOT NULL default ' ',
  "AAAAIHFA" varchar(1) NOT NULL default ' ',
  "AAAAIHFB" varchar(1) NOT NULL default ' ',
  "AAAAIHFC" varchar(1) NOT NULL default ' ',
  "AAAAIHFD" varchar(1) NOT NULL default ' ',
  "AAAAIHFR" varchar(1) NOT NULL default ' ',
  "AAAAIHFS" varchar(1) NOT NULL default ' ',
  "AAAAIHFT" varchar(1) NOT NULL default ' ',
  "AAAAIHFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAAAIHFN" varchar(1) NOT NULL default ' ',
  "AAAAIHFO" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(1) NOT NULL default ' ',
  "AAAAIHFQ" varchar(1) NOT NULL default ' ',
  "AAAAIHFR" varchar(1) NOT NULL default ' ',
  "AAAAIHFS" varchar(1) NOT NULL default ' ',
  "AAAAIHFT" varchar(1) NOT NULL default ' ',
  "AAAAIHFU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGI" varchar(10) NOT NULL default ' ',
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  "AAAAIHGB" varchar(1) NOT NULL default ' ',
  "AAAAIHGC" varchar(1) NOT NULL default ' ',
  "AAAAIHGD" varchar(1) NOT NULL default ' ',
  "AAABERXQ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHGU" varchar(32) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABLKZT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHGU","AAABLXQQ","AAABKRTQ"),
  KEY "AAAAIHGX" ("AAABMLCZ","AAABKQVZ"),
  KEY "AAAAIHHA" ("AAABMRIL","AAABMLCZ","AAABLXQQ","AAABKQVZ","AAABKRTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQSDS" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAQSDV" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQSDS"),
  KEY "AAAAIHHM" ("AAABMRIL","AAAAQSDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHHP" (
  "AAAAIHHR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAAAIHHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHHS" (
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(40) NOT NULL default ' ',
  "AAABJSHI" varchar(1) NOT NULL default ' ',
  "AAABGGJC" varchar(1) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAABLEBM" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMIRY","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUGJ" varchar(40) NOT NULL default ' ',
  "AAABDECE" varchar(8) NOT NULL default '00000000',
  "AAABLMEU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAYUGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHIJ" (
  "AAAAIHIM" varchar(8) NOT NULL default '00000000',
  "AAAAIHIN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAIHIM","AAAAIHIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHIO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAIHIR" varchar(10) NOT NULL default ' ',
  "AAABGWVP" varchar(8) NOT NULL default '00000000',
  "AAABGWVQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAZQHA" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZQCG" varchar(10) NOT NULL default ' ',
  "AAAAZQCH" varchar(60) NOT NULL default ' ',
  "AAAAZQCI" varchar(60) NOT NULL default ' ',
  "AAAAZQCJ" varchar(60) NOT NULL default ' ',
  "AAAAIHJE" varchar(8) NOT NULL default '00000000',
  "AAAALFHQ" varchar(8) NOT NULL default '00000000',
  "AAAALFHR" varchar(8) NOT NULL default '00000000',
  "AAAAIHQN" varchar(60) NOT NULL default ' ',
  "AAAAIHQO" varchar(60) NOT NULL default ' ',
  "AAAAIHJJ" varchar(60) NOT NULL default ' ',
  "AAAAIHNB" varchar(1) NOT NULL default ' ',
  "AAAAIHOO" varchar(8) NOT NULL default '00000000',
  "AAAAIHOP" varchar(6) NOT NULL default '000000',
  "AAAAIHNC" varchar(12) NOT NULL default ' ',
  "AAAAIHJX" varchar(6) NOT NULL default ' ',
  "AAAAIHJY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHSE"),
  KEY "AAAAIHJS" ("AAABMRIL","AAAAZQHA"),
  KEY "AAAAIHJV" ("AAABMRIL","AAAAIHJX","AAAAIHJY"),
  KEY "AAAAIHJZ" ("AAABMRIL","AAAAIHOO","AAAAIHOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHNB" varchar(1) NOT NULL default ' ',
  "AAAAIHNC" varchar(12) NOT NULL default ' ',
  "AAAAIHOO" varchar(8) NOT NULL default '00000000',
  "AAAAIHOP" varchar(6) NOT NULL default '000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATGXW" varchar(40) NOT NULL default ' ',
  "AAAATGXX" varchar(40) NOT NULL default ' ',
  "AAAATGXZ" varchar(10) NOT NULL default ' ',
  "AAABIXXS" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(60) NOT NULL default ' ',
  "AAAATGXR" varchar(10) NOT NULL default ' ',
  "AAABIXXY" varchar(20) NOT NULL default ' ',
  "AAABIXYA" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAAAIHNR" varchar(40) NOT NULL default ' ',
  "AAAAIHNS" varchar(40) NOT NULL default ' ',
  "AAAAIHNT" varchar(40) NOT NULL default ' ',
  "AAAAIHNU" varchar(40) NOT NULL default ' ',
  "AAAAIHNV" varchar(40) NOT NULL default ' ',
  "AAAAIHNW" varchar(40) NOT NULL default ' ',
  "AAAAIHNX" varchar(40) NOT NULL default ' ',
  "AAAAIHNY" varchar(40) NOT NULL default ' ',
  "AAAAIHNZ" varchar(40) NOT NULL default ' ',
  "AAAAIHOA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHSE","AAAAZPNQ"),
  KEY "AAAAIHLJ" ("AAABMRIL","AAABLEHK","AAABIYJJ"),
  KEY "AAAAIHLN" ("AAABMRIL","AAAAIHNC","AAAAIHOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILWE" varchar(10) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHLV" varchar(1) NOT NULL default ' ',
  "AAAAIHSB" varchar(2) NOT NULL default ' ',
  "AAAAIHLX" varchar(60) NOT NULL default ' ',
  "AAAAIHLY" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABILWE","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJIJW" varchar(3) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABJJHN" varchar(6) NOT NULL default '000000',
  "AAAALFIE" varchar(3) NOT NULL default ' ',
  "AAABJIKB" varchar(2) NOT NULL default ' ',
  "AAAAIHQJ" varchar(1) NOT NULL default ' ',
  "AAAAIHQP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT","AAABJIJW","AAABMQIL","AAABMLTD","AAABMALG","AAABJJHN","AAAALFIE","AAABJIKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHNB" varchar(1) NOT NULL default ' ',
  "AAAAIHNC" varchar(12) NOT NULL default ' ',
  "AAAAIHOO" varchar(8) NOT NULL default '00000000',
  "AAAAIHOP" varchar(6) NOT NULL default '000000',
  "AAAAZQHP" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLZCP" varchar(40) NOT NULL default ' ',
  "AAABLZCR" varchar(40) NOT NULL default ' ',
  "AAABLVHG" varchar(40) NOT NULL default ' ',
  "AAABLVHH" varchar(40) NOT NULL default ' ',
  "AAAATGXP" varchar(40) NOT NULL default ' ',
  "AAAAIHOK" varchar(10) NOT NULL default ' ',
  "AAAAIHOL" varchar(5) NOT NULL default '00000',
  "AAAAIHNO" varchar(3) NOT NULL default ' ',
  "AAAAIHNP" varchar(20) NOT NULL default ' ',
  "AAAAIHNQ" varchar(20) NOT NULL default ' ',
  "AAAAIHNR" varchar(40) NOT NULL default ' ',
  "AAAAIHNS" varchar(40) NOT NULL default ' ',
  "AAAAIHNT" varchar(40) NOT NULL default ' ',
  "AAAAIHNU" varchar(40) NOT NULL default ' ',
  "AAAAIHNV" varchar(40) NOT NULL default ' ',
  "AAAAIHNW" varchar(40) NOT NULL default ' ',
  "AAAAIHNX" varchar(40) NOT NULL default ' ',
  "AAAAIHNY" varchar(40) NOT NULL default ' ',
  "AAAAIHNZ" varchar(40) NOT NULL default ' ',
  "AAAAIHOA" varchar(40) NOT NULL default ' ',
  "AAAAIHOB" varchar(40) NOT NULL default ' ',
  "AAAAIHOC" varchar(40) NOT NULL default ' ',
  "AAAAIHOD" varchar(40) NOT NULL default ' ',
  "AAAAIHOE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHSE","AAAAZPNQ"),
  KEY "AAAAIHOI" ("AAABMRIL","AAAAIHOK","AAAAIHOL"),
  KEY "AAAAIHOM" ("AAABMRIL","AAAAIHOO","AAAAIHOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHOQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJIJW" varchar(3) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAIHSC" varchar(1) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHQJ" varchar(1) NOT NULL default ' ',
  "AAAAIHQK" varchar(1) NOT NULL default ' ',
  "AAAAIHQL" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIHQN" varchar(60) NOT NULL default ' ',
  "AAAAIHQO" varchar(60) NOT NULL default ' ',
  "AAAAIHQP" varchar(10) NOT NULL default ' ',
  "AAAAIHQQ" varchar(10) NOT NULL default ' ',
  "AAAAIHQR" varchar(10) NOT NULL default ' ',
  "AAAAIHQS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTF","AAABLDEN","AAABMPLT","AAABJIJW","AAABMQIL","AAABMLTD","AAABMALG","AAAAIHSE","AAAAIHSC","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABJIJW" varchar(3) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAIHSC" varchar(1) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHQJ" varchar(1) NOT NULL default ' ',
  "AAAAIHQK" varchar(1) NOT NULL default ' ',
  "AAAAIHQL" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIHQN" varchar(60) NOT NULL default ' ',
  "AAAAIHQO" varchar(60) NOT NULL default ' ',
  "AAAAIHQP" varchar(10) NOT NULL default ' ',
  "AAAAIHQQ" varchar(10) NOT NULL default ' ',
  "AAAAIHQR" varchar(10) NOT NULL default ' ',
  "AAAAIHQS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTF","AAABLDEN","AAABMPLT","AAABJIJW","AAABMQIL","AAABMLTD","AAABMALG","AAAAIHSE","AAAAIHSC","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAIHSC" varchar(1) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIHSB" varchar(2) NOT NULL default ' ',
  "AAAAWQWH" varchar(5) NOT NULL default '00000',
  "AAAAIHSD" varchar(40) NOT NULL default ' ',
  "AAAAIHRM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIHSE","AAAAIHSC","AAAAZPNQ","AAAAIHSB","AAAAWQWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIHSB" varchar(2) NOT NULL default ' ',
  "AAAAIHSC" varchar(1) NOT NULL default ' ',
  "AAAAIHSD" varchar(40) NOT NULL default ' ',
  "AAAAIHSE" varchar(10) NOT NULL default ' ',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAIHSB","AAAAIHSC","AAAAIHSD","AAAAIHSE","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHSG" (
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHSN" (
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
CREATE TABLE "AAAAIHSY" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHTI" (
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
  KEY "AAAAIHTW" ("AAABMRFO","AAABMLQA","AAABHZDJ"),
  KEY "AAAAIHUA" ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHUC" (
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
CREATE TABLE "AAAAIHUV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHVC" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAIHVM" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHVQ" (
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
  UNIQUE KEY "AAAAIHWD" ("AAABLLNL"),
  KEY "AAAAIHWF" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHWH" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAIHWU" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHWY" (
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
  UNIQUE KEY "AAAAIHXL" ("AAABLLNL"),
  KEY "AAAAIHXN" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHXP" (
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
CREATE TABLE "AAAAIHYB" (
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
  UNIQUE KEY "AAAAIHYP" ("AAABLLNL"),
  KEY "AAAAIHYR" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIHYT" (
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
CREATE TABLE "AAAAIHZT" (
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
  UNIQUE KEY "AAAAIIAH" ("AAABLLNL"),
  KEY "AAAAIIAJ" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIAL" (
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAIIAV" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIAZ" (
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
  UNIQUE KEY "AAAAIIBM" ("AAABLLNL"),
  KEY "AAAAIIBO" ("AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIBQ" (
  "AAABLLNN" varchar(32) NOT NULL default ' ',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLND" varchar(10) NOT NULL default ' ',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNN","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAIICD" ("AAABLLMD","AAABLLME","AAABLLND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIICH" (
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
  UNIQUE KEY "AAAAIICU" ("AAABLLNL"),
  KEY "AAAAIICW" ("AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIDB" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAAPJAQ" varchar(10) NOT NULL default ' ',
  "AAAAIIDB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAAAPJAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABKYNA" varchar(1) NOT NULL default ' ',
  "AAABKYNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAKT" varchar(10) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  "AAABMEKY" varchar(1) NOT NULL default ' ',
  "AAABHNOU" varchar(30) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABJIJW" varchar(3) NOT NULL default ' ',
  "AAABHNOT" varchar(30) NOT NULL default ' ',
  "AAABFNHW" varchar(40) NOT NULL default ' ',
  "AAAAIIEC" varchar(60) NOT NULL default ' ',
  "AAAAIIED" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTF","AAABLDEN","AAABMQIL","AAABMLTD","AAABMAMY","AAABMQIJ","AAABMRHS","AAABMAKT","AAABMALG","AAABMEKY","AAABHNOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIIFR" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABMAHQ" varchar(25) NOT NULL default ' ',
  "AAAAIIFU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABLYTF","AAABLDEN","AAABLZUS","AAABMLTD","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAAAZPNQ" varchar(5) NOT NULL default '00000',
  "AAAAIIFR" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABMAHQ" varchar(25) NOT NULL default ' ',
  "AAAAIIFU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABLYTF","AAABLDEN","AAABLZUS","AAABMLTD","AAAAZPNQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJTQV" varchar(2) NOT NULL default '00',
  "AAABJTQU" varchar(4) NOT NULL default '0000',
  "AAABJTQW" varchar(1) NOT NULL default '0',
  "AAABJTQI" varchar(20) NOT NULL default ' ',
  "AAABJRSB" varchar(3) NOT NULL default '000',
  "AAAAIIGV" varchar(7) NOT NULL default ' ',
  "AAABLYTF" varchar(6) NOT NULL default ' ',
  "AAABLDEN" decimal(15,0) NOT NULL default '0',
  "AAAAIIGM" varchar(1) NOT NULL default ' ',
  "AAAAYBCK" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJTQV","AAABJTQU","AAABJTQW","AAABJTQI","AAABJRSB","AAAAIIGV","AAABLYTF","AAABLDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKRZZ" varchar(10) NOT NULL default '0000000000',
  "AAABLMJL" varchar(4) NOT NULL default '0000',
  "AAABKSAB" varchar(1) NOT NULL default ' ',
  "AAABKROA" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLCZ","AAABKRZZ","AAABLMJL","AAABKSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLDBD" varchar(10) NOT NULL default ' ',
  "AAAAIIHR" varchar(20) NOT NULL default ' ',
  "AAAAIIHS" varchar(60) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLDBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKYLW" varchar(13) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAAAIIKN" varchar(10) NOT NULL default ' ',
  "AAABGTXN" varchar(8) NOT NULL default '00000000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAAAIIIJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIIK" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIIL" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIIM" decimal(17,2) NOT NULL default '0.00',
  "AAAAIIIN" decimal(17,2) NOT NULL default '0.00',
  "AAAAIIIO" varchar(5) NOT NULL default ' ',
  "AAAAIIIP" varchar(5) NOT NULL default ' ',
  "AAAAIIIQ" varchar(5) NOT NULL default ' ',
  "AAAAIIIR" decimal(12,3) NOT NULL default '0.000',
  "AAAAIIIS" decimal(12,3) NOT NULL default '0.000',
  "AAAAIIIT" decimal(11,5) NOT NULL default '0.00000',
  "AAABKQPS" decimal(14,10) NOT NULL default '0.0000000000',
  "AAAAQBIC" decimal(14,8) NOT NULL default '0.00000000',
  "AAAAIIIW" decimal(14,10) NOT NULL default '0.0000000000',
  "AAAAIIIX" decimal(14,10) NOT NULL default '0.0000000000',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAAAIIKE" VARBINARY(255),
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLADI","AAABMEFT","AAABKYLW","AAABMREX","AAABLAYL","AAAAIIKN","AAABGTXN","AAABESZZ","AAABGXQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKYLW" varchar(13) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAAAIIKN" varchar(10) NOT NULL default ' ',
  "AAABGTXN" varchar(8) NOT NULL default '00000000',
  "AAABESZZ" varchar(4) NOT NULL default ' ',
  "AAABGXQH" varchar(3) NOT NULL default ' ',
  "AAAAIIKR" varchar(5) NOT NULL default ' ',
  "AAAAIIJY" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIJZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIKA" decimal(15,2) NOT NULL default '0.00',
  "AAAAIIKB" decimal(17,2) NOT NULL default '0.00',
  "AAAAIIKC" decimal(17,2) NOT NULL default '0.00',
  "AAAAIIKD" varchar(5) NOT NULL default ' ',
  "AAAAIIKE" VARBINARY(255),
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLADI","AAABMEFT","AAABKYLW","AAABMREX","AAABLAYL","AAAAIIKN","AAABGTXN","AAABESZZ","AAABGXQH","AAAAIIKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABINZS" varchar(5) NOT NULL default ' ',
  "AAAAVLJZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABINZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAABCATV" varchar(3) NOT NULL default ' ',
  "AAAAIKTL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTM" varchar(6) NOT NULL default '000000',
  "AAAAIKNY" varchar(3) NOT NULL default ' ',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTO" varchar(6) NOT NULL default '000000',
  "AAAAIKOB" varchar(3) NOT NULL default ' ',
  "AAAAIKLE" varchar(6) NOT NULL default ' ',
  "AAABIYJF" varchar(40) NOT NULL default ' ',
  "AAAAIKLH" varchar(6) NOT NULL default ' ',
  "AAAAIILR" varchar(40) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAAAIKLN" varchar(1) NOT NULL default ' ',
  "AAAAIKLO" varchar(1) NOT NULL default ' ',
  "AAABMKAH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAIILX" tinyint(3) unsigned NOT NULL default '0',
  "AAAAIJBH" varchar(17) NOT NULL default ' ',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIMA" varchar(8) NOT NULL default ' ',
  "AAAAIKNK" varchar(12) NOT NULL default ' ',
  "AAABCAUH" varchar(30) NOT NULL default ' ',
  "AAABBYTT" varchar(1) NOT NULL default ' ',
  "AAABBYTU" varchar(40) NOT NULL default ' ',
  "AAAAIJAP" varchar(1) NOT NULL default ' ',
  "AAABBYTV" varchar(40) NOT NULL default ' ',
  "AAAAIIMH" varchar(5) NOT NULL default ' ',
  "AAAAIIMI" decimal(10,2) NOT NULL default '0.00',
  "AAAAIJHT" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIMK" varchar(3) NOT NULL default ' ',
  "AAAAIIML" varchar(3) NOT NULL default ' ',
  "AAAAIIMM" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIMN" varchar(5) NOT NULL default ' ',
  "AAAAIIMO" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAAAIIMS" varchar(1) NOT NULL default ' ',
  "AAAAIJBI" varchar(1) NOT NULL default ' ',
  "AAAAIJBJ" varchar(3) NOT NULL default ' ',
  "AAAAIJBK" varchar(3) NOT NULL default ' ',
  "AAAAIJBL" varchar(20) NOT NULL default ' ',
  "AAAAIJBM" varchar(3) NOT NULL default ' ',
  "AAAAIJBN" varchar(3) NOT NULL default ' ',
  "AAAAIJBO" varchar(20) NOT NULL default ' ',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABBZJI" varchar(8) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAAIJBV" varchar(1) NOT NULL default ' ',
  "AAAAIJBZ" varchar(5) NOT NULL default ' ',
  "AAAAIJCA" varchar(1) NOT NULL default ' ',
  "AAABAEJB" varchar(1) NOT NULL default ' ',
  "AAAAONKP" varchar(1) NOT NULL default ' ',
  "AAAAONKQ" varchar(1) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  "AAAAONKS" varchar(12) NOT NULL default ' ',
  "AAAAONKT" varchar(3) NOT NULL default ' ',
  "AAAAONKU" varchar(3) NOT NULL default ' ',
  "AAAAONKV" varchar(3) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAAAIJCJ" varchar(132) NOT NULL default ' ',
  "AAAAIINT" varchar(11) NOT NULL default ' ',
  "AAAAIINU" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF"),
  KEY "AAAAIIOD" ("AAABLSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAABAEJB" varchar(1) NOT NULL default ' ',
  "AAAAONKP" varchar(1) NOT NULL default ' ',
  "AAAAONKQ" varchar(1) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  "AAAAONKS" varchar(12) NOT NULL default ' ',
  "AAAAONKT" varchar(3) NOT NULL default ' ',
  "AAAAONKU" varchar(3) NOT NULL default ' ',
  "AAAAONKV" varchar(3) NOT NULL default ' ',
  "AAAATKJP" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAIIPI" varchar(20) NOT NULL default ' ',
  "AAABCAUG" varchar(12) NOT NULL default ' ',
  "AAABCAUH" varchar(30) NOT NULL default ' ',
  "AAABCAUI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIIUR" varchar(3) NOT NULL default '000',
  "AAAAIIPW" varchar(10) NOT NULL default ' ',
  "AAAAIIPX" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIPY" varchar(5) NOT NULL default ' ',
  "AAAAIIPZ" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIQA" varchar(5) NOT NULL default ' ',
  "AAAAIIQB" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIQC" varchar(5) NOT NULL default ' ',
  "AAAAVDBG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIIUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABMOIB" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIIUR" varchar(3) NOT NULL default '000',
  "AAABLRXL" varchar(2) NOT NULL default ' ',
  "AAABLRPR" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABMOIB","AAAAILAY","AAAAIIUR","AAABLRXL","AAABLRPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAABCAMC" varchar(3) NOT NULL default ' ',
  "AAABCALY" varchar(6) NOT NULL default ' ',
  "AAAAIKXO" varchar(8) NOT NULL default '00000000',
  "AAAAIKXP" varchar(6) NOT NULL default '000000',
  "AAAAIKXQ" varchar(3) NOT NULL default ' ',
  "AAAAIIRT" varchar(2) NOT NULL default ' ',
  "AAAAIKXR" varchar(6) NOT NULL default ' ',
  "AAAAIKXS" varchar(8) NOT NULL default '00000000',
  "AAAAIKXT" varchar(6) NOT NULL default '000000',
  "AAAAIKXU" varchar(3) NOT NULL default ' ',
  "AAAAIIRY" varchar(2) NOT NULL default ' ',
  "AAAAIKXV" varchar(6) NOT NULL default ' ',
  "AAAAIISA" varchar(1) NOT NULL default ' ',
  "AAABCALZ" varchar(1) NOT NULL default ' ',
  "AAABCAMD" varchar(1) NOT NULL default ' ',
  "AAAAIISD" tinyint(3) unsigned NOT NULL default '0',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAAAIISH" varchar(1) NOT NULL default ' ',
  "AAABGLWI" varchar(5) NOT NULL default ' ',
  "AAAAIISJ" varchar(6) NOT NULL default '000000',
  "AAAAIISK" varchar(3) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABCAAK" varchar(8) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAAIJBV" varchar(1) NOT NULL default ' ',
  "AAAAIISR" varchar(1) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAAAIJCJ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF"),
  KEY "AAAAIITC" ("AAABLSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIITE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIIUR" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIIUR","AAABLIMF","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIITX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABMOIB" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIIUR" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAAAIJPJ" varchar(14) NOT NULL default ' ',
  "AAAAIIUI" varchar(3) NOT NULL default ' ',
  "AAAAIJPN" varchar(8) NOT NULL default '00000000',
  "AAAAIJPO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABMOIB","AAAAILAY","AAAAIIUR","AAABLIMF","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAAAIIVZ" varchar(3) NOT NULL default '000',
  "AAAAIIVE" varchar(3) NOT NULL default ' ',
  "AAAAIKXT" varchar(6) NOT NULL default '000000',
  "AAAAIKXP" varchar(6) NOT NULL default '000000',
  "AAAAIIVH" varchar(6) NOT NULL default '000000',
  "AAAAIIVI" varchar(6) NOT NULL default '000000',
  "AAAAIIVJ" varchar(3) NOT NULL default ' ',
  "AAABLRPR" smallint(6) NOT NULL default '0',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAAAIIVM" varchar(4) NOT NULL default ' ',
  "AAAAIIVN" varchar(1) NOT NULL default ' ',
  "AAAATKIE" varchar(1) NOT NULL default ' ',
  "AAAAONHG" varchar(1) NOT NULL default ' ',
  "AAAAIJVT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF","AAAAIIVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAAAIJMF" varchar(4) NOT NULL default ' ',
  "AAAAIIWK" varchar(3) NOT NULL default ' ',
  "AAAAIIWL" varchar(1) NOT NULL default ' ',
  "AAAAIIWM" varchar(1) NOT NULL default ' ',
  "AAAAIIWN" varchar(1) NOT NULL default ' ',
  "AAAAIIWO" varchar(1) NOT NULL default ' ',
  "AAAAIKJT" varchar(3) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIKJV" varchar(132) NOT NULL default ' ',
  "AAABLRPR" smallint(6) NOT NULL default '0',
  "AAAAIIWT" varchar(3) NOT NULL default ' ',
  "AAAAIIWU" varchar(1) NOT NULL default ' ',
  "AAAAIIWV" varchar(21) NOT NULL default ' ',
  "AAAAONHJ" varchar(120) NOT NULL default ' ',
  "AAAAONHL" varchar(120) NOT NULL default ' ',
  "AAABJSBP" varchar(1) NOT NULL default ' ',
  "AAAAONHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAVDBG" varchar(3) NOT NULL default '000',
  "AAAAIIYP" varchar(1) NOT NULL default ' ',
  "AAABILXZ" varchar(8) NOT NULL default '00000000',
  "AAAAIIYR" varchar(1) NOT NULL default ' ',
  "AAAAIIYS" varchar(4) NOT NULL default ' ',
  "AAAAIIYT" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIYU" varchar(5) NOT NULL default ' ',
  "AAAAIIYV" varchar(1) NOT NULL default ' ',
  "AAAAIJKA" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIYX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAVDBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABMOIB" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAVDBG" varchar(3) NOT NULL default '000',
  "AAAAIIYO" varchar(3) NOT NULL default ' ',
  "AAAAIIYP" varchar(1) NOT NULL default ' ',
  "AAABILXZ" varchar(8) NOT NULL default '00000000',
  "AAAAIIYR" varchar(1) NOT NULL default ' ',
  "AAAAIIYS" varchar(4) NOT NULL default ' ',
  "AAAAIIYT" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIYU" varchar(5) NOT NULL default ' ',
  "AAAAIIYV" varchar(1) NOT NULL default ' ',
  "AAAAIIYW" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIYX" varchar(5) NOT NULL default ' ',
  "AAAAIIYY" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIYZ" varchar(5) NOT NULL default ' ',
  "AAAAIIZA" varchar(15) NOT NULL default '000000000000000',
  "AAAAIIZB" varchar(1) NOT NULL default ' ',
  "AAABHMNR" varchar(77) NOT NULL default ' ',
  "AAAAIIZD" varchar(2) NOT NULL default ' ',
  "AAAAIIZE" decimal(10,2) NOT NULL default '0.00',
  "AAAAIIZF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABMOIB","AAAAILAY","AAAAVDBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIIZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAAAIIZW" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAAAIKXS" varchar(8) NOT NULL default '00000000',
  "AAAAIKXO" varchar(8) NOT NULL default '00000000',
  "AAAAIJAA" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABIYJF" varchar(132) NOT NULL default ' ',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABBZWN" varchar(1) NOT NULL default ' ',
  "AAABBYTZ" varchar(1) NOT NULL default ' ',
  "AAAAIKNG" tinyint(3) unsigned NOT NULL default '0',
  "AAAAIJAH" tinyint(3) unsigned NOT NULL default '0',
  "AAABBYUD" varchar(1) NOT NULL default ' ',
  "AAAAIKNJ" varchar(1) NOT NULL default ' ',
  "AAAAIJAK" varchar(3) NOT NULL default ' ',
  "AAAAIKNK" varchar(3) NOT NULL default ' ',
  "AAAAIJAM" varchar(1) NOT NULL default ' ',
  "AAABBYTT" varchar(1) NOT NULL default ' ',
  "AAABBYTU" varchar(40) NOT NULL default ' ',
  "AAAAIJAP" varchar(1) NOT NULL default ' ',
  "AAABBYTV" varchar(40) NOT NULL default ' ',
  "AAABBYTW" varchar(1) NOT NULL default ' ',
  "AAAAIJAS" varchar(6) NOT NULL default '000000',
  "AAAAIJAT" varchar(1) NOT NULL default ' ',
  "AAABBYTX" varchar(1) NOT NULL default ' ',
  "AAABCAUH" varchar(30) NOT NULL default ' ',
  "AAAAIJAW" varchar(1) NOT NULL default ' ',
  "AAABBYTY" varchar(1) NOT NULL default ' ',
  "AAABBYTS" varchar(1) NOT NULL default ' ',
  "AAABKWVR" decimal(10,2) NOT NULL default '0.00',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABKSWQ" varchar(132) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAABCAAT" varchar(1) NOT NULL default ' ',
  "AAABCAAU" varchar(1) NOT NULL default ' ',
  "AAAAIJBH" varchar(17) NOT NULL default ' ',
  "AAAAIJBI" varchar(1) NOT NULL default ' ',
  "AAAAIJBJ" varchar(3) NOT NULL default ' ',
  "AAAAIJBK" varchar(3) NOT NULL default ' ',
  "AAAAIJBL" varchar(20) NOT NULL default ' ',
  "AAAAIJBM" varchar(3) NOT NULL default ' ',
  "AAAAIJBN" varchar(3) NOT NULL default ' ',
  "AAAAIJBO" varchar(20) NOT NULL default ' ',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABCATU" varchar(8) NOT NULL default ' ',
  "AAABCAEE" varchar(1) NOT NULL default ' ',
  "AAABCAAK" varchar(8) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default ' ',
  "AAAAIJBV" varchar(1) NOT NULL default ' ',
  "AAAAIKNL" varchar(1) NOT NULL default ' ',
  "AAAAIJBX" varchar(10) NOT NULL default ' ',
  "AAAAIJBY" varchar(8) NOT NULL default ' ',
  "AAAAIJBZ" varchar(5) NOT NULL default ' ',
  "AAAAIJCA" varchar(1) NOT NULL default ' ',
  "AAABAEJB" varchar(1) NOT NULL default ' ',
  "AAAAONJC" varchar(1) NOT NULL default ' ',
  "AAAAONJD" varchar(1) NOT NULL default ' ',
  "AAABDEEY" varchar(1) NOT NULL default ' ',
  "AAAAONKT" varchar(3) NOT NULL default ' ',
  "AAAAONKU" varchar(3) NOT NULL default ' ',
  "AAAAONKV" varchar(3) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAAAIJCJ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF"),
  KEY "AAAAIJCS" ("AAABLSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABHFND" varchar(3) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABJSTM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTN" decimal(15,12) NOT NULL default '0.000000000000',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABHFND","AAABLKIV","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAXA" varchar(3) NOT NULL default ' ',
  "AAABHFND" varchar(3) NOT NULL default ' ',
  "AAABLKIV" varchar(3) NOT NULL default ' ',
  "AAABGZSJ" varchar(6) NOT NULL default ' ',
  "AAABJSTN" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABJSTP" varchar(40) NOT NULL default ' ',
  "AAABKNIB" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(60) NOT NULL default ' ',
  "AAABCAPI" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABCAPM" varchar(40) NOT NULL default ' ',
  "AAABIYJL" varchar(40) NOT NULL default ' ',
  "AAABIWXH" varchar(40) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAXA","AAABHFND","AAABLKIV","AAABGZSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABLGEF" varchar(3) NOT NULL default '000',
  "AAABAEJB" varchar(1) NOT NULL default ' ',
  "AAAAONJC" varchar(1) NOT NULL default ' ',
  "AAAAONJD" varchar(1) NOT NULL default ' ',
  "AAABDEEY" varchar(1) NOT NULL default ' ',
  "AAAAONKT" varchar(3) NOT NULL default ' ',
  "AAAAONKU" varchar(3) NOT NULL default ' ',
  "AAAAONKV" varchar(3) NOT NULL default ' ',
  "AAAATKJP" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF","AAABLGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJFI" varchar(3) NOT NULL default '000',
  "AAAAIJFJ" varchar(2) NOT NULL default ' ',
  "AAAAIJFK" varchar(1) NOT NULL default ' ',
  "AAAAIJFL" varchar(1) NOT NULL default ' ',
  "AAAAIJFM" varchar(2) NOT NULL default '00',
  "AAAAIJFN" varchar(8) NOT NULL default '00000000',
  "AAAAIJFO" varchar(1) NOT NULL default ' ',
  "AAABLMQH" varchar(4) NOT NULL default ' ',
  "AAAAIJFQ" varchar(8) NOT NULL default '00000000',
  "AAAAIJFR" varchar(4) NOT NULL default ' ',
  "AAAAIJFS" varchar(2) NOT NULL default '00',
  "AAAAIJFT" varchar(8) NOT NULL default '00000000',
  "AAAAIJFU" varchar(2) NOT NULL default '00',
  "AAAAIJFV" varchar(8) NOT NULL default '00000000',
  "AAAAIJFW" varchar(5) NOT NULL default ' ',
  "AAAAIKJT" varchar(5) NOT NULL default ' ',
  "AAAAIJFY" smallint(6) NOT NULL default '0',
  "AAAAIJFZ" smallint(6) NOT NULL default '0',
  "AAAAIJGA" varchar(1) NOT NULL default ' ',
  "AAAAIJGB" varchar(12) NOT NULL default ' ',
  "AAAAIJGC" varchar(8) NOT NULL default '00000000',
  "AAAAIJGD" varchar(12) NOT NULL default ' ',
  "AAAAIJGE" varchar(8) NOT NULL default '00000000',
  "AAAAIJGF" varchar(2) NOT NULL default ' ',
  "AAAAIJGG" varchar(15) NOT NULL default ' ',
  "AAAAIJGH" varchar(8) NOT NULL default '00000000',
  "AAAAIJGI" varchar(6) NOT NULL default '000000',
  "AAAAIJGJ" varchar(8) NOT NULL default '00000000',
  "AAAAIJGK" varchar(6) NOT NULL default '000000',
  "AAAAIJGL" varchar(8) NOT NULL default '00000000',
  "AAAAIJGM" varchar(6) NOT NULL default '000000',
  "AAAAIJGN" varchar(10) NOT NULL default ' ',
  "AAAAIJGO" varchar(10) NOT NULL default ' ',
  "AAAAIJGP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJGY" varchar(2) NOT NULL default '00',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJVD" varchar(4) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIJHD" varchar(2) NOT NULL default '00',
  "AAAAIJSR" varchar(8) NOT NULL default '00000000',
  "AAAAIJSS" varchar(8) NOT NULL default '00000000',
  "AAAAIJST" varchar(5) NOT NULL default ' ',
  "AAAAIJSU" varchar(20) NOT NULL default ' ',
  "AAAAIJHI" varchar(5) NOT NULL default ' ',
  "AAAAIJHJ" varchar(25) NOT NULL default ' ',
  "AAAAIJHK" varchar(40) NOT NULL default ' ',
  "AAAAIJHL" varchar(15) NOT NULL default ' ',
  "AAAAIJHM" varchar(15) NOT NULL default ' ',
  "AAAAIJHN" varchar(15) NOT NULL default ' ',
  "AAAAIJSZ" varchar(1) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAIJHQ" varchar(25) NOT NULL default ' ',
  "AAAAIJTB" varchar(20) NOT NULL default ' ',
  "AAAAIJHS" varchar(25) NOT NULL default ' ',
  "AAAAIJHT" varchar(20) NOT NULL default ' ',
  "AAAAIJHU" varchar(70) NOT NULL default ' ',
  "AAABCARR" varchar(50) NOT NULL default ' ',
  "AAAAIJOH" varchar(30) NOT NULL default ' ',
  "AAAAIJHX" varchar(45) NOT NULL default ' ',
  "AAAAIJHY" varchar(25) NOT NULL default ' ',
  "AAAAIJHZ" varchar(20) NOT NULL default ' ',
  "AAAAIJIA" varchar(10) NOT NULL default ' ',
  "AAAAIJIB" varchar(10) NOT NULL default ' ',
  "AAAAIJIC" varchar(30) NOT NULL default ' ',
  "AAAAIJID" varchar(70) NOT NULL default ' ',
  "AAAAIJIE" varchar(30) NOT NULL default ' ',
  "AAAAIJIF" varchar(10) NOT NULL default ' ',
  "AAAAIJIG" varchar(60) NOT NULL default ' ',
  "AAAAIJIH" varchar(60) NOT NULL default ' ',
  "AAAAIJRH" varchar(90) NOT NULL default ' ',
  "AAAAIJIJ" varchar(20) NOT NULL default ' ',
  "AAAAIJIK" varchar(30) NOT NULL default ' ',
  "AAAAIJIL" varchar(20) NOT NULL default ' ',
  "AAAAIJRJ" varchar(30) NOT NULL default ' ',
  "AAAAIJIN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJIX" varchar(24) NOT NULL default ' ',
  "AAAAIJIY" varchar(5) NOT NULL default ' ',
  "AAAAIJIZ" varchar(4) NOT NULL default '0000',
  "AAAAIJJA" varchar(4) NOT NULL default ' ',
  "AAAAIJJB" varchar(1) NOT NULL default ' ',
  "AAAAIJJC" varchar(4) NOT NULL default ' ',
  "AAAAIJJD" varchar(4) NOT NULL default ' ',
  "AAAAIJJE" varchar(1) NOT NULL default ' ',
  "AAAAIJJF" varchar(1) NOT NULL default ' ',
  "AAAAIJJG" varchar(2) NOT NULL default '00',
  "AAAAIJJH" varchar(2) NOT NULL default '00',
  "AAAAIJJI" varchar(6) NOT NULL default ' ',
  "AAAAIJJJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJJT" varchar(2) NOT NULL default '00',
  "AAAAIJJU" varchar(1) NOT NULL default ' ',
  "AAABGKHN" varchar(5) NOT NULL default ' ',
  "AAAAIJPM" decimal(10,2) NOT NULL default '0.00',
  "AAAAIJJX" varchar(5) NOT NULL default ' ',
  "AAAAIJJY" decimal(10,2) NOT NULL default '0.00',
  "AAAAIJJZ" varchar(5) NOT NULL default ' ',
  "AAAAIJKA" decimal(10,2) NOT NULL default '0.00',
  "AAAAIJKB" varchar(11) NOT NULL default ' ',
  "AAAAIJKC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJLH" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAABBZOV" varchar(3) NOT NULL default '000',
  "AAAAIJKQ" varchar(150) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJLH" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJLH","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJZX" varchar(1) NOT NULL default ' ',
  "AAAAIJYB" varchar(2) NOT NULL default '00',
  "AAAAIJLQ" varchar(4) NOT NULL default '0000',
  "AAAAIJLR" varchar(4) NOT NULL default '0000',
  "AAAAIJLS" varchar(1) NOT NULL default ' ',
  "AAAAIKAB" varchar(1) NOT NULL default ' ',
  "AAAAIJLU" varchar(5) NOT NULL default ' ',
  "AAAAIJLV" varchar(5) NOT NULL default ' ',
  "AAAAIKJT" varchar(5) NOT NULL default ' ',
  "AAAAIJLX" varchar(5) NOT NULL default ' ',
  "AAAAIKAJ" varchar(2) NOT NULL default ' ',
  "AAAAIJLZ" varchar(2) NOT NULL default ' ',
  "AAAAIJMA" varchar(8) NOT NULL default '00000000',
  "AAAAIJMB" varchar(6) NOT NULL default '000000',
  "AAAAIJMC" varchar(8) NOT NULL default '00000000',
  "AAAAIJMD" varchar(6) NOT NULL default '000000',
  "AAAAIKAR" varchar(5) NOT NULL default ' ',
  "AAAAIJMF" varchar(3) NOT NULL default ' ',
  "AAAAIJMG" varchar(1) NOT NULL default '0',
  "AAAAIJMH" varchar(3) NOT NULL default ' ',
  "AAAAIJMI" varchar(1) NOT NULL default ' ',
  "AAAAIJMJ" varchar(3) NOT NULL default ' ',
  "AAAAIJMK" varchar(2) NOT NULL default ' ',
  "AAAAIJML" varchar(6) NOT NULL default '000000',
  "AAAAIJMM" varchar(1) NOT NULL default ' ',
  "AAAAIJMN" varchar(5) NOT NULL default ' ',
  "AAAAIJMO" varchar(1) NOT NULL default ' ',
  "AAAAIJMP" varchar(50) NOT NULL default ' ',
  "AAAAIJMQ" varchar(6) NOT NULL default ' ',
  "AAAAIJUA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJNV" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAABCARS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJNV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJNV" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJNV","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJOZ" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAABBZOV" varchar(2) NOT NULL default '00',
  "AAAAIKJT" varchar(5) NOT NULL default ' ',
  "AAAAIJOH" varchar(30) NOT NULL default ' ',
  "AAAAIJOI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJOZ" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJOZ","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJPU" varchar(4) NOT NULL default '0000',
  "AAAAIJPV" varchar(4) NOT NULL default '0000',
  "AAAAIJPI" varchar(3) NOT NULL default ' ',
  "AAAAIJPJ" varchar(8) NOT NULL default ' ',
  "AAAAIJPK" varchar(8) NOT NULL default ' ',
  "AAAAIJSW" varchar(5) NOT NULL default ' ',
  "AAAAIJPM" varchar(9) NOT NULL default ' ',
  "AAAAIJPN" varchar(8) NOT NULL default '00000000',
  "AAAAIJPO" varchar(8) NOT NULL default '00000000',
  "AAAAIJPP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJPU","AAAAIJPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJSL" varchar(2) NOT NULL default '00',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJVD" varchar(4) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIJSQ" varchar(2) NOT NULL default '00',
  "AAAAIJSR" varchar(8) NOT NULL default '00000000',
  "AAAAIJSS" varchar(8) NOT NULL default '00000000',
  "AAAAIJST" varchar(5) NOT NULL default ' ',
  "AAAAIJSU" varchar(20) NOT NULL default ' ',
  "AAAAIJSV" varchar(8) NOT NULL default ' ',
  "AAAAIJSW" varchar(5) NOT NULL default ' ',
  "AAAAIJSX" varchar(10) NOT NULL default ' ',
  "AAABIWPG" varchar(4) NOT NULL default ' ',
  "AAAAIJSY" varchar(50) NOT NULL default ' ',
  "AAAAIJQQ" varchar(3) NOT NULL default ' ',
  "AAAAIJQR" varchar(10) NOT NULL default ' ',
  "AAAAIJSZ" varchar(1) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAIJTB" varchar(20) NOT NULL default ' ',
  "AAAAIJQV" varchar(20) NOT NULL default ' ',
  "AAAAIJQW" varchar(25) NOT NULL default ' ',
  "AAAAIJQX" varchar(20) NOT NULL default ' ',
  "AAAAIJQY" varchar(35) NOT NULL default ' ',
  "AAAAIJQZ" varchar(20) NOT NULL default ' ',
  "AAAAIJRA" varchar(20) NOT NULL default ' ',
  "AAAAIJRB" varchar(20) NOT NULL default ' ',
  "AAAAIJRC" varchar(35) NOT NULL default ' ',
  "AAAAIJRD" varchar(20) NOT NULL default ' ',
  "AAAAIJRE" varchar(20) NOT NULL default ' ',
  "AAAAIJRF" varchar(20) NOT NULL default ' ',
  "AAAAIJRG" varchar(8) NOT NULL default ' ',
  "AAAAIJRH" varchar(50) NOT NULL default ' ',
  "AAAAIJRI" varchar(1) NOT NULL default ' ',
  "AAAAIJRJ" varchar(30) NOT NULL default ' ',
  "AAAAIJRK" varchar(50) NOT NULL default ' ',
  "AAAAIJRL" varchar(60) NOT NULL default ' ',
  "AAAAIJRM" varchar(60) NOT NULL default ' ',
  "AAAAIJRN" varchar(60) NOT NULL default ' ',
  "AAAAIJRO" varchar(60) NOT NULL default ' ',
  "AAAAIJRP" varchar(60) NOT NULL default ' ',
  "AAAAIJRQ" varchar(30) NOT NULL default ' ',
  "AAAAIJRR" varchar(3) NOT NULL default ' ',
  "AAAAIJRS" varchar(30) NOT NULL default ' ',
  "AAAAIJRT" varchar(20) NOT NULL default ' ',
  "AAAAIJRU" varchar(30) NOT NULL default ' ',
  "AAAAIJRV" varchar(30) NOT NULL default ' ',
  "AAAAIJRW" varchar(30) NOT NULL default ' ',
  "AAAAIKAT" varchar(1) NOT NULL default ' ',
  "AAAAIJRY" varchar(30) NOT NULL default ' ',
  "AAAAIJRZ" varchar(30) NOT NULL default ' ',
  "AAAAIJSA" varchar(30) NOT NULL default ' ',
  "AAAAIJSB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJSL" varchar(2) NOT NULL default '00',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJVD" varchar(4) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIJSQ" varchar(2) NOT NULL default '00',
  "AAAAIJSR" varchar(8) NOT NULL default '00000000',
  "AAAAIJSS" varchar(8) NOT NULL default '00000000',
  "AAAAIJST" varchar(5) NOT NULL default ' ',
  "AAAAIJSU" varchar(20) NOT NULL default ' ',
  "AAAAIJSV" varchar(8) NOT NULL default ' ',
  "AAAAIJSW" varchar(5) NOT NULL default ' ',
  "AAAAIJSX" varchar(11) NOT NULL default ' ',
  "AAAAIJSY" varchar(1) NOT NULL default ' ',
  "AAAAIJSZ" varchar(1) NOT NULL default ' ',
  "AAAAIJTA" varchar(10) NOT NULL default ' ',
  "AAAAIJTB" varchar(20) NOT NULL default ' ',
  "AAAAIJTC" varchar(200) NOT NULL default ' ',
  "AAAAIKAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJTN" varchar(2) NOT NULL default '00',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJVD" varchar(4) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIJXB" varchar(2) NOT NULL default '00',
  "AAAAIJTT" varchar(5) NOT NULL default ' ',
  "AAAAIJTU" varchar(20) NOT NULL default ' ',
  "AAAAIJTV" varchar(5) NOT NULL default ' ',
  "AAAAIJTW" varchar(20) NOT NULL default ' ',
  "AAAAIKAN" varchar(8) NOT NULL default '00000000',
  "AAAAIKAP" varchar(8) NOT NULL default '00000000',
  "AAAAIJXC" varchar(232) NOT NULL default ' ',
  "AAAAIJUA" varchar(1) NOT NULL default ' ',
  "AAABBZVB" varchar(5) NOT NULL default ' ',
  "AAAAIJUC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJUO" varchar(70) NOT NULL default ' ',
  "AAAAIJUP" varchar(4) NOT NULL default ' ',
  "AAAAIJUQ" varchar(30) NOT NULL default ' ',
  "AAAAIJUR" varchar(100) NOT NULL default ' ',
  "AAAAVDNK" varchar(40) NOT NULL default ' ',
  "AAABIYEN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJVD" varchar(5) NOT NULL default ' ',
  "AAAAIJVE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJWO" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJYF" varchar(5) NOT NULL default ' ',
  "AAAAIJYH" varchar(5) NOT NULL default ' ',
  "AAAAIJVT" varchar(1) NOT NULL default ' ',
  "AAAAIJVU" varchar(3) NOT NULL default ' ',
  "AAAAIJVV" varchar(1) NOT NULL default ' ',
  "AAAAIJVW" varchar(1) NOT NULL default ' ',
  "AAAAIJVX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJWO" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJWO","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJXT" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIKHM" varchar(4) NOT NULL default ' ',
  "AAAAIKJT" varchar(5) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIJXB" varchar(1) NOT NULL default '0',
  "AAAAIJXC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJXT" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJXT","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJZX" varchar(1) NOT NULL default ' ',
  "AAAAIJYB" varchar(2) NOT NULL default '00',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIKAB" varchar(1) NOT NULL default ' ',
  "AAAAIJYF" varchar(5) NOT NULL default ' ',
  "AAAAIJYG" varchar(20) NOT NULL default ' ',
  "AAAAIJYH" varchar(5) NOT NULL default ' ',
  "AAAAIJYI" varchar(20) NOT NULL default ' ',
  "AAAAIJYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJZQ" varchar(4) NOT NULL default '0000',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIJYW" varchar(1) NOT NULL default ' ',
  "AAAAIJYX" varchar(3) NOT NULL default '000',
  "AAAAIJYY" varchar(12) NOT NULL default '000000000000',
  "AAAAIJYZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIJZP" varchar(4) NOT NULL default '0000',
  "AAAAIJZQ" varchar(4) NOT NULL default '0000',
  "AAAAIJZR" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIJZP","AAAAIJZQ","AAAAIJZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIJZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKAV" varchar(20) NOT NULL default ' ',
  "AAAAIKAW" varchar(12) NOT NULL default '000000000000',
  "AAAAIKWD" varchar(4) NOT NULL default '0000',
  "AAAAIJZX" varchar(1) NOT NULL default ' ',
  "AAAAIJZY" varchar(3) NOT NULL default ' ',
  "AAAAIJZZ" varchar(4) NOT NULL default '0000',
  "AAAAIKAA" varchar(4) NOT NULL default '0000',
  "AAAAIKAB" varchar(1) NOT NULL default ' ',
  "AAAAIKAC" varchar(5) NOT NULL default ' ',
  "AAAAIKAD" varchar(20) NOT NULL default ' ',
  "AAAAIKAE" varchar(5) NOT NULL default ' ',
  "AAAAIKAF" varchar(20) NOT NULL default ' ',
  "AAABHILF" varchar(2) NOT NULL default ' ',
  "AAAAIKAH" varchar(30) NOT NULL default ' ',
  "AAAAIKAI" varchar(10) NOT NULL default ' ',
  "AAAAIKAJ" varchar(2) NOT NULL default ' ',
  "AAAAIKAK" varchar(30) NOT NULL default ' ',
  "AAAAIKAL" varchar(30) NOT NULL default ' ',
  "AAAAIKAM" varchar(30) NOT NULL default ' ',
  "AAAAIKAN" varchar(8) NOT NULL default '00000000',
  "AAAAIKAO" varchar(6) NOT NULL default '000000',
  "AAAAIKAP" varchar(8) NOT NULL default '00000000',
  "AAAAIKAQ" varchar(6) NOT NULL default '000000',
  "AAAAIKAR" varchar(5) NOT NULL default ' ',
  "AAAAIKAS" varchar(4) NOT NULL default ' ',
  "AAAAIKAT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIKAV","AAAAIKAW","AAAAIKWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(3) NOT NULL default '000',
  "AAABCALH" varchar(2) NOT NULL default ' ',
  "AAABLRXL" varchar(1) NOT NULL default ' ',
  "AAAAIKVS" varchar(1) NOT NULL default ' ',
  "AAAAIKTL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTM" varchar(6) NOT NULL default '000000',
  "AAAAIKNY" varchar(3) NOT NULL default ' ',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTO" varchar(6) NOT NULL default '000000',
  "AAAAIKOB" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAAAIKBQ" decimal(10,2) NOT NULL default '0.00',
  "AAAAIKBR" decimal(10,2) NOT NULL default '0.00',
  "AAABFYUL" decimal(10,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAAIKBU" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLSBE" varchar(1) NOT NULL default ' ',
  "AAABMDBA" varchar(3) NOT NULL default ' ',
  "AAABCAKN" varchar(1) NOT NULL default ' ',
  "AAAAIKPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAABLIMF"),
  KEY "AAAAIKCH" ("AAABLSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKCJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAYFFA" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAMHPY" varchar(3) NOT NULL default '000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAAAIKCS" varchar(30) NOT NULL default ' ',
  "AAABDNKH" varchar(32) NOT NULL default ' ',
  "AAAAIKCU" int(11) NOT NULL default '0',
  "AAABJDAC" int(11) NOT NULL default '0',
  "AAABLEQR" varchar(5) NOT NULL default '00000',
  "AAAAIKCX" varchar(20) NOT NULL default ' ',
  "AAAAIKCY" varchar(20) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUXQ","AAABLUNI","AAABMDEN","AAAAYFFA","AAABMRFQ","AAABMLES","AAAAMHPY","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAAAIKTL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTM" varchar(6) NOT NULL default '000000',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTO" varchar(6) NOT NULL default '000000',
  "AAABHIXN" varchar(59) NOT NULL default ' ',
  "AAABKNCK" varchar(59) NOT NULL default ' ',
  "AAAAIKTP" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAIKES" varchar(8) NOT NULL default '00000000',
  "AAAAIKET" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAAAJAGI" varchar(2) NOT NULL default '00',
  "AAAAIKTL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTM" varchar(6) NOT NULL default '000000',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAIKTO" varchar(6) NOT NULL default '000000',
  "AAABHIXN" varchar(59) NOT NULL default ' ',
  "AAABKNCK" varchar(59) NOT NULL default ' ',
  "AAAAIKTP" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAAAIKES" varchar(8) NOT NULL default '00000000',
  "AAAAIKET" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAAAJAGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAAAIKFH" varchar(3) NOT NULL default ' ',
  "AAAAIKFI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAABIYEN" varchar(40) NOT NULL default ' ',
  "AAAAVDNK" varchar(40) NOT NULL default ' ',
  "AAABIYJF" varchar(60) NOT NULL default ' ',
  "AAABJXDU" varchar(10) NOT NULL default ' ',
  "AAAAIKNC" varchar(40) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAAAVDNK" varchar(40) NOT NULL default ' ',
  "AAABIYEN" varchar(40) NOT NULL default ' ',
  "AAAAIKGV" smallint(6) NOT NULL default '0',
  "AAAAIKGW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAAAIKHM" varchar(4) NOT NULL default ' ',
  "AAAAIKJT" varchar(3) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default '00',
  "AAAAIKHP" varchar(2) NOT NULL default '00',
  "AAAAIKHQ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAAAIKIG" varchar(20) NOT NULL default ' ',
  "AAABIKMD" varchar(20) NOT NULL default ' ',
  "AAAAIKII" varchar(40) NOT NULL default ' ',
  "AAABJAAI" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLRXL" varchar(20) NOT NULL default ' ',
  "AAABBZOY" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAQHWN" varchar(10) NOT NULL default '0000000000',
  "AAABLSKB" varchar(2) NOT NULL default '00',
  "AAABLEST" varchar(2) NOT NULL default '00',
  "AAAAILAY" varchar(2) NOT NULL default '00',
  "AAAAIKKC" smallint(6) NOT NULL default '0',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAAAIKJT" varchar(3) NOT NULL default ' ',
  "AAAAIKJU" varchar(2) NOT NULL default ' ',
  "AAAAIKJV" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAQHWN","AAABLSKB","AAABLEST","AAAAILAY","AAAAIKKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKOW" varchar(5) NOT NULL default '00000',
  "AAAAIKKN" varchar(5) NOT NULL default '00000',
  "AAAAIKKH" varchar(2) NOT NULL default '00',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAAAQQHQ" varchar(8) NOT NULL default '00000000',
  "AAAAQQHR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAIKOW","AAAAIKKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAIKKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIKOW" varchar(5) NOT NULL default '00000',
  "AAAAIKKV" varchar(8) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAIKOW","AAAAIKKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
