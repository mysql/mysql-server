# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl09.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAGUDI" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAAAGUDM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAAAGUDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUDN" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGKUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABMREL","AAABMIRX","AAABMIRY","AAABGKUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUDY" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABCBWH" varchar(2) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABMREL","AAABCBWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUEG" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAAAGUEM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIGEG","AAABHEIG","AAAAGUEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAGUFP" varchar(5) NOT NULL default '00000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLYIE" varchar(8) NOT NULL default '00000000',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAGUEX" varchar(1) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAAAGUFA" varchar(1) NOT NULL default ' ',
  "AAABLUFN" varchar(4) NOT NULL default ' ',
  "AAAAGUFC" varchar(1) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAAAGUFE" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABKPOX" varchar(1) NOT NULL default ' ',
  "AAAAGUFH" varchar(1) NOT NULL default ' ',
  "AAAAGUFI" varchar(1) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABIBIY" varchar(10) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAAAGUFP"),
  KEY "AAAAGUFQ" ("AAABMRIL","AAABMCVP","AAABMCVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUFU" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMQHU","AAABKNGX","AAABMQHY","AAABMQCJ","AAABMQHV","AAABMREX","AAABMQCL","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUGO" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAAAGUHI" varchar(4) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMQHU","AAABKNGX","AAABMQHY","AAABMQCJ","AAABMQHV","AAABETQU","AAABMQCL","AAAAGUHI","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBYDM" varchar(22) NOT NULL default ' ',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAABMKFC" varchar(3) NOT NULL default ' ',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAGUHR" varchar(8) NOT NULL default '00000000',
  "AAAAGUHS" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABBYDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAAAGUKT" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAGUIG" decimal(15,0) NOT NULL default '0',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGRC","AAAAGUKT","AAABMDEN","AAAAGUIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGUKK" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGUKK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAAAGUKK" varchar(30) NOT NULL default ' ',
  "AAAAGUIT" varchar(1) NOT NULL default ' ',
  "AAABMKFC" varchar(3) NOT NULL default ' ',
  "AAABLUZL" varchar(10) NOT NULL default ' ',
  "AAAAGUJW" varchar(30) NOT NULL default ' ',
  "AAAAGUIX" varchar(30) NOT NULL default ' ',
  "AAAAGUIY" varchar(255) NOT NULL default ' ',
  "AAAAGUJQ" varchar(40) NOT NULL default ' ',
  "AAAAGUJR" varchar(14) NOT NULL default ' ',
  "AAABLECQ" decimal(15,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAAAGULS" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULT" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULU" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULV" decimal(15,3) NOT NULL default '0.000',
  "AAAAGUJI" varchar(3) NOT NULL default '000',
  "AAABLUTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGRC"),
  KEY "AAAAGUJM" ("AAABMRIL","AAABLUZL","AAABMKFC","AAAAGUJQ","AAAAGUJR"),
  KEY "AAAAGUJS" ("AAABMRIL","AAABLUZL","AAABMKFC","AAAAGUJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAAAGUME" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGRC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGUKK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGUKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAAAGUKT" decimal(15,0) NOT NULL default '0',
  "AAABMMJT" decimal(15,3) NOT NULL default '0.000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAUGRC","AAAAGUKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGULB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKTX" VARBINARY(255) NOT NULL,
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKTX"(255),"AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGULI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGULM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGULM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGULN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAABLECQ" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULS" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULT" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULU" decimal(15,3) NOT NULL default '0.000',
  "AAAAGULV" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGULZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAUGRC" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAAAGUME" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAUGRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABGKTX" VARBINARY(255) NOT NULL,
  "AAAAGUMM" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABGKTX"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUMR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAGUOM" varchar(10) NOT NULL default '0000000000',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABKGPK" varchar(32) NOT NULL default ' ',
  "AAABGBIE" varchar(30) NOT NULL default ' ',
  "AAABILXZ" varchar(8) NOT NULL default '00000000',
  "AAAAGUOU" varchar(8) NOT NULL default '00000000',
  "AAAAGUOV" varchar(8) NOT NULL default '00000000',
  "AAAAGUOW" varchar(5) NOT NULL default '00000',
  "AAAAGUOX" varchar(5) NOT NULL default '00000',
  "AAABCLWN" varchar(2) NOT NULL default '00',
  "AAAAGUND" varchar(50) NOT NULL default ' ',
  "AAABLZNQ" varchar(4) NOT NULL default ' ',
  "AAAAGUNF" varchar(1) NOT NULL default ' ',
  "AAAAGUNG" varchar(50) NOT NULL default ' ',
  "AAABLZNS" varchar(4) NOT NULL default ' ',
  "AAAAGUNI" varchar(1) NOT NULL default ' ',
  "AAAAGUNJ" varchar(50) NOT NULL default ' ',
  "AAAAGUNK" varchar(4) NOT NULL default ' ',
  "AAAAGUNL" varchar(1) NOT NULL default ' ',
  "AAAAGUNM" varchar(50) NOT NULL default ' ',
  "AAAAGUNN" varchar(4) NOT NULL default ' ',
  "AAAAGUNO" varchar(1) NOT NULL default ' ',
  "AAAAGUNP" varchar(50) NOT NULL default ' ',
  "AAAAGUNQ" varchar(4) NOT NULL default ' ',
  "AAAAGUNR" varchar(1) NOT NULL default ' ',
  "AAAAGUNS" varchar(3) NOT NULL default '000',
  "AAAAGUNT" varchar(3) NOT NULL default '000',
  "AAAAGUNU" varchar(3) NOT NULL default '000',
  "AAAAGUNV" varchar(3) NOT NULL default '000',
  "AAAAGUNW" varchar(3) NOT NULL default '000',
  "AAAAGUNX" varchar(3) NOT NULL default '000',
  "AAAAGUNY" varchar(3) NOT NULL default '000',
  "AAAAGUNZ" varchar(3) NOT NULL default '000',
  "AAAAGUOA" varchar(3) NOT NULL default '000',
  "AAAAGUOB" varchar(3) NOT NULL default '000',
  "AAABLLLP" varchar(30) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAAGUOF" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAPTFN" varchar(6) NOT NULL default '000000',
  "AAAAGUOJ" varchar(1) NOT NULL default ' ',
  "AAABKSUA" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAGUOM"),
  KEY "AAAAGUON" ("AAABLUXQ","AAABLFYC","AAABKGPK","AAABGBIE"),
  KEY "AAAAGUOS" ("AAABILXZ","AAAAGUOU","AAAAGUOV","AAAAGUOW","AAAAGUOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUOY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAAAGUPB" varchar(3) NOT NULL default '000',
  "AAABLBDX" varchar(50) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUPJ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGBIE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKGBL","AAABLFYC","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUPT" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMLQA","AAABKGBL","AAABLFYC","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAAAGUQJ" varchar(8) NOT NULL default '00000000',
  "AAAAGUQK" varchar(1) NOT NULL default ' ',
  "AAABKPRV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSPJ","AAABKSQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAAAGURE" varchar(30) NOT NULL default ' ',
  "AAAAGUTD" varchar(4) NOT NULL default ' ',
  "AAAAGUSB" int(11) NOT NULL default '0',
  "AAAAGUSC" int(11) NOT NULL default '0',
  "AAAAGUVL" varchar(30) NOT NULL default ' ',
  "AAAAGUVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAAAGURE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGURF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAAAGUVL" varchar(30) NOT NULL default ' ',
  "AAAAGUVM" varchar(30) NOT NULL default ' ',
  "AAAAGUVV" varchar(20) NOT NULL default ' ',
  "AAAAGUVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAAAGUVL","AAAAGUVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGURU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAAAGUTD" varchar(4) NOT NULL default ' ',
  "AAAAUERX" varchar(4) NOT NULL default ' ',
  "AAAAGUSB" int(11) NOT NULL default '0',
  "AAAAGUSC" int(11) NOT NULL default '0',
  "AAAAGUSD" int(11) NOT NULL default '0',
  "AAAAGUSE" int(11) NOT NULL default '0',
  "AAAAGUSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAAAGUTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAAAGUTD" varchar(4) NOT NULL default ' ',
  "AAAAGUTE" varchar(4) NOT NULL default ' ',
  "AAAATMFF" int(11) NOT NULL default '0',
  "AAAAGUST" int(11) NOT NULL default '0',
  "AAAAGUSU" int(11) NOT NULL default '0',
  "AAAAGUSV" int(11) NOT NULL default '0',
  "AAABFUQE" int(11) NOT NULL default '0',
  "AAAAGUSX" int(11) NOT NULL default '0',
  "AAABLNRX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAAAGUTD","AAAAGUTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUTF" (
  "AAABLZMT" varchar(60) NOT NULL default ' ',
  "AAABMDXM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANLF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUTN" (
  "AAAAGUTU" varchar(30) NOT NULL default ' ',
  "AAAAGUTV" varchar(12) NOT NULL default ' ',
  "AAAAGUTQ" int(11) NOT NULL default '0',
  "AAAAGUTR" int(11) NOT NULL default '0',
  "AAAAGUTS" int(11) NOT NULL default '0',
  "AAAAGUTT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAAAGUTU","AAAAGUTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAAAGUUC" varchar(4) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABBIWG" varchar(1) NOT NULL default ' ',
  "AAABDKQJ" int(11) NOT NULL default '0',
  "AAAAGUUG" varchar(1) NOT NULL default ' ',
  "AAAAGUUH" varchar(40) NOT NULL default ' ',
  "AAAAMMYR" varchar(20) NOT NULL default ' ',
  "AAAAGUVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(70) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABIFON" varchar(30) NOT NULL default ' ',
  "AAAAGUVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAABMLQA","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAVRPD" varchar(10) NOT NULL default ' ',
  "AAAAGUVT" varchar(1) NOT NULL default ' ',
  "AAAAGUVU" varchar(10) NOT NULL default ' ',
  "AAAAGUVV" varchar(70) NOT NULL default ' ',
  "AAAAGUVL" varchar(30) NOT NULL default ' ',
  "AAAAGUVM" varchar(30) NOT NULL default ' ',
  "AAAAGUVN" varchar(30) NOT NULL default ' ',
  "AAAAGUVO" varchar(30) NOT NULL default ' ',
  "AAAAGUVP" varchar(1) NOT NULL default ' ',
  "AAAAGUVQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAVRPD","AAAAGUVT","AAAAGUVU","AAAAGUVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAAAMWJL" varchar(2) NOT NULL default ' ',
  "AAAAGUWA" varchar(4) NOT NULL default ' ',
  "AAAAGUWB" varchar(4) NOT NULL default ' ',
  "AAAAGUWC" varchar(10) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGUWY" varchar(40) NOT NULL default ' ',
  "AAABLJYA" varchar(1) NOT NULL default ' ',
  "AAAAWSDB" varchar(1) NOT NULL default ' ',
  "AAAAGUXB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABMRHS"),
  KEY "AAAAGUWW" ("AAABMRIL","AAAAGUWY"),
  KEY "AAAAGUWZ" ("AAABMRIL","AAAAGUXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMZTD" varchar(18) NOT NULL default ' ',
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
  "AAAAGUXU" varchar(18) NOT NULL default ' ',
  "AAAAGUXV" varchar(4) NOT NULL default ' ',
  "AAAAGUXW" varchar(18) NOT NULL default ' ',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMZTD"),
  UNIQUE KEY "AAAAGUYB" ("AAABMRIL","AAABMLCZ","AAABLXTF"),
  KEY "AAAAGUYF" ("AAABMRIL","AAABLXTF"),
  KEY "AAAAGUYI" ("AAABMLCZ","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABMCVI","AAABKQVZ","AAABLXQQ","AAABKRTQ"),
  KEY "AAAAGUZC" ("AAABMRIL","AAABLXQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGUZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAAGUZK" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  "AAAAWSNV" varchar(10) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMHEL" varchar(25) NOT NULL default ' ',
  "AAABLIAR" varchar(18) NOT NULL default ' ',
  "AAABMLEV" decimal(13,3) NOT NULL default '0.000',
  "AAABMLEW" varchar(3) NOT NULL default ' ',
  "AAAAMICW" varchar(8) NOT NULL default '00000000',
  "AAAASHGR" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAMICX" varchar(10) NOT NULL default ' ',
  "AAAAGUZZ" varchar(8) NOT NULL default '00000000',
  "AAABLXHT" varchar(8) NOT NULL default '00000000',
  "AAAAGVAB" decimal(13,2) NOT NULL default '0.00',
  "AAAAVRZF" varchar(30) NOT NULL default ' ',
  "AAAAJSRX" varchar(3) NOT NULL default ' ',
  "AAAAGVAE" varchar(30) NOT NULL default ' ',
  "AAAAMICZ" varchar(30) NOT NULL default ' ',
  "AAAAVRZR" varchar(20) NOT NULL default ' ',
  "AAAASMYV" varchar(4) NOT NULL default ' ',
  "AAAAJSRZ" varchar(2) NOT NULL default ' ',
  "AAAAGVAJ" varchar(1) NOT NULL default ' ',
  "AAAAGVAK" varchar(8) NOT NULL default '00000000',
  "AAABBMRD" varchar(8) NOT NULL default '00000000',
  "AAAAMIDB" varchar(18) NOT NULL default ' ',
  "AAAAMIDC" varchar(3) NOT NULL default '000',
  "AAAAGVAO" varchar(8) NOT NULL default '00000000',
  "AAAAGVAP" varchar(8) NOT NULL default '00000000',
  "AAAAGVAQ" varchar(30) NOT NULL default ' ',
  "AAAAGVAR" varchar(9) NOT NULL default '000000000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAMZTD" varchar(18) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAGVCM" varchar(1) NOT NULL default ' ',
  "AAAAGVCL" varchar(18) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABLXTF" varchar(18) NOT NULL default ' ',
  "AAABLFLE" varchar(4) NOT NULL default ' ',
  "AAAAZUKK" varchar(4) NOT NULL default ' ',
  "AAAATVAC" varchar(10) NOT NULL default ' ',
  "AAABLVIN" varchar(10) NOT NULL default ' ',
  "AAABKNPW" varchar(12) NOT NULL default ' ',
  "AAAAGVBE" varchar(12) NOT NULL default ' ',
  "AAABKRRX" varchar(2) NOT NULL default ' ',
  "AAAAGVBG" varchar(20) NOT NULL default ' ',
  "AAAAGVBH" varchar(1) NOT NULL default ' ',
  "AAAAGVBI" varchar(1) NOT NULL default ' ',
  "AAAAGVBJ" varchar(1) NOT NULL default ' ',
  "AAAAGVBK" varchar(1) NOT NULL default ' ',
  "AAAAGVBL" varchar(1) NOT NULL default ' ',
  "AAAAGVBM" varchar(1) NOT NULL default ' ',
  "AAAAGVBN" varchar(1) NOT NULL default ' ',
  "AAAAGVBO" varchar(1) NOT NULL default ' ',
  "AAAAGVCG" varchar(1) NOT NULL default ' ',
  "AAAAGVBQ" varchar(1) NOT NULL default ' ',
  "AAAAWSZD" varchar(1) NOT NULL default ' ',
  "AAABJASU" varchar(2) NOT NULL default ' ',
  "AAABLSTE" varchar(22) NOT NULL default ' ',
  "AAABKYEV" varchar(10) NOT NULL default ' ',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAAAGVBW" varchar(1) NOT NULL default ' ',
  "AAAAGVBX" varchar(1) NOT NULL default ' ',
  "AAAAGVBY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQVW"),
  KEY "AAAAGVCB" ("AAABLXTF","AAABMLCZ"),
  KEY "AAAAGVCE" ("AAABMLCZ","AAAAGVCG","AAABLXTF","AAABMQVW"),
  KEY "AAAAGVCJ" ("AAABMRIL","AAAAGVCL","AAAAGVCM"),
  KEY "AAAAGVCN" ("AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAWIPQ" varchar(8) NOT NULL default '00000000',
  "AAABLYIE" varchar(8) NOT NULL default '00000000',
  "AAABKQVL" varchar(10) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAAAVYTC" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABMCVI","AAAAWIPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQVL" varchar(10) NOT NULL default ' ',
  "AAABKQVM" varchar(3) NOT NULL default '000',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKQUS" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAMRMH" varchar(4) NOT NULL default ' ',
  "AAABLPGZ" decimal(3,0) NOT NULL default '0',
  "AAAAGVDM" decimal(15,3) NOT NULL default '0.000',
  "AAAAGVDN" decimal(15,3) NOT NULL default '0.000',
  "AAAAGVDO" decimal(15,3) NOT NULL default '0.000',
  "AAABKQWF" varchar(4) NOT NULL default ' ',
  "AAAAMEGR" decimal(13,3) NOT NULL default '0.000',
  "AAAAMEGQ" decimal(13,3) NOT NULL default '0.000',
  "AAABLURM" varchar(4) NOT NULL default ' ',
  "AAAAGVDT" varchar(1) NOT NULL default ' ',
  "AAAAGVDU" decimal(13,3) NOT NULL default '0.000',
  "AAABMCUF" varchar(1) NOT NULL default ' ',
  "AAAAPTFL" varchar(2) NOT NULL default '00',
  "AAAAGVDX" varchar(2) NOT NULL default '00',
  "AAABLUFQ" varchar(18) NOT NULL default ' ',
  "AAABLURC" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKQVL","AAABKQVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQVW" varchar(18) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAAAMIDC" varchar(3) NOT NULL default '000',
  "AAAAGVEI" varchar(3) NOT NULL default '000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAGVEN" varchar(6) NOT NULL default '000000',
  "AAAAGVEO" varchar(1) NOT NULL default ' ',
  "AAAAGVEP" varchar(4) NOT NULL default ' ',
  "AAABJEYP" varchar(12) NOT NULL default ' ',
  "AAABLRYQ" varchar(1) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABLLIM" varchar(4) NOT NULL default ' ',
  "AAAAJXKK" varchar(1) NOT NULL default ' ',
  "AAAASHIU" varchar(18) NOT NULL default ' ',
  "AAAAMIDL" varchar(30) NOT NULL default ' ',
  "AAAAGVGF" varchar(18) NOT NULL default ' ',
  "AAAAGVEY" varchar(4) NOT NULL default ' ',
  "AAABLKLM" varchar(3) NOT NULL default ' ',
  "AAAAJXKM" varchar(1) NOT NULL default ' ',
  "AAABKNPK" varchar(2) NOT NULL default ' ',
  "AAABKNPL" varchar(8) NOT NULL default '00000000',
  "AAAAGVFD" varchar(1) NOT NULL default ' ',
  "AAAAGVGD" varchar(25) NOT NULL default ' ',
  "AAAAGVFF" varchar(1) NOT NULL default ' ',
  "AAABKNPE" varchar(12) NOT NULL default ' ',
  "AAAAGVFH" varchar(10) NOT NULL default ' ',
  "AAAAGVFI" varchar(10) NOT NULL default ' ',
  "AAAAGVGL" varchar(10) NOT NULL default ' ',
  "AAAAGVFK" varchar(20) NOT NULL default ' ',
  "AAABGPDC" varchar(9) NOT NULL default ' ',
  "AAAARBIB" varchar(8) NOT NULL default '00000000',
  "AAAAGVFN" varchar(30) NOT NULL default ' ',
  "AAAAGVFO" varchar(9) NOT NULL default '000000000',
  "AAAAJXKR" varchar(1) NOT NULL default ' ',
  "AAABLSCM" varchar(10) NOT NULL default ' ',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAAAGVFS" VARBINARY(255),
  "AAAAPXDW" VARBINARY(255),
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABDHUM" varchar(40) NOT NULL default ' ',
  "AAAAGVFW" varchar(1) NOT NULL default ' ',
  "AAAAGVFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQVW","AAABMCJZ","AAAAMIDC"),
  KEY "AAAAGVGC" ("AAAAGVGD"),
  KEY "AAAAGVGE" ("AAAAGVGF","AAABMCJZ"),
  KEY "AAAAGVGH" ("AAABKNPE","AAABMCJZ"),
  KEY "AAAAGVGK" ("AAAAGVGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVHT" varchar(4) NOT NULL default ' ',
  "AAAAGVIQ" varchar(4) NOT NULL default ' ',
  "AAAAGVID" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAGVGS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGVHT","AAAAGVIQ","AAAAGVID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVHF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAGVHC" varchar(1) NOT NULL default '0',
  "AAABIYDW" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAGVHF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVHT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGVHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGVHT" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAGVHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVIQ" varchar(4) NOT NULL default ' ',
  "AAAAGVIC" varchar(1) NOT NULL default '0',
  "AAAAGVID" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAGVIQ","AAAAGVIC","AAAAGVID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVIQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGVIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGVIQ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAGVIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMZRB" varchar(8) NOT NULL default ' ',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAMZRD" varchar(8) NOT NULL default '00000000',
  "AAAAMZQN" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQO" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQP" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQQ" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQR" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQS" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQT" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQU" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQV" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQW" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQX" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQY" decimal(7,3) NOT NULL default '0.000',
  "AAAAGVJJ" varchar(1) NOT NULL default ' ',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMZRB","AAABLXTG","AAABMCVI","AAAAMZRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAMZRB" varchar(8) NOT NULL default ' ',
  "AAAAMZQN" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQO" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQP" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQQ" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQR" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQS" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQT" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQU" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQV" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQW" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQX" decimal(7,3) NOT NULL default '0.000',
  "AAAAMZQY" decimal(7,3) NOT NULL default '0.000',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAMZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAMZRB" varchar(8) NOT NULL default ' ',
  "AAAAKUUY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAMZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAAAGVLT" varchar(10) NOT NULL default ' ',
  "AAAAGVLU" varchar(5) NOT NULL default '00000',
  "AAABKPRS" varchar(8) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABJRYM" varchar(6) NOT NULL default '000000',
  "AAAAGVKZ" varchar(12) NOT NULL default ' ',
  "AAAAGVLA" varchar(8) NOT NULL default '00000000',
  "AAAAGVLB" varchar(6) NOT NULL default '000000',
  "AAAAGVLC" varchar(12) NOT NULL default ' ',
  "AAAAGVLD" varchar(8) NOT NULL default '00000000',
  "AAAAGVLE" varchar(6) NOT NULL default '000000',
  "AAAAWMHS" varchar(4) NOT NULL default ' ',
  "AAABKPNT" varchar(60) NOT NULL default ' ',
  "AAAALEGV" varchar(20) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABLXPL" decimal(13,2) NOT NULL default '0.00',
  "AAAAGVLK" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAGVLN" varchar(1) NOT NULL default ' ',
  "AAAAGVLO" varchar(1) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABKPRP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUHY","AAAAGVLT","AAAAGVLU","AAABKPRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVLW" (
  "AAABLUSV" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAAAGVMD" int(11) NOT NULL default '0',
  "AAAAGVME" int(11) NOT NULL default '0',
  "AAAAGVMF" int(11) NOT NULL default '0',
  "AAAAGVMG" int(11) NOT NULL default '0',
  "AAAAGVMH" int(11) NOT NULL default '0',
  "AAAAGVMI" int(11) NOT NULL default '0',
  "AAAAGVMJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLUSV","AAABMDEP","AAABJEMU","AAABMREL","AAABLFZK","AAABLKMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVTW" varchar(18) NOT NULL default ' ',
  "AAAAGVMT" varchar(1) NOT NULL default ' ',
  "AAAAGVMU" varchar(1) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAGVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGVTW" varchar(18) NOT NULL default ' ',
  "AAAAGVNH" varchar(40) NOT NULL default ' ',
  "AAAAGVTR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAGVTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGVTW" varchar(18) NOT NULL default ' ',
  "AAAAGVNZ" varchar(5) NOT NULL default '00000',
  "AAABLUII" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAAAGVNS" int(11) NOT NULL default '0',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAAAGVNU" double NOT NULL default '0',
  "AAABIIXH" smallint(6) NOT NULL default '0',
  "AAABIZSL" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAGVTW","AAAAGVNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(2) NOT NULL default ' ',
  "AAAAGVQV" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGZWU" varchar(40) NOT NULL default ' ',
  "AAABMRGO" varchar(40) NOT NULL default ' ',
  "AAABLKIV" varchar(35) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABDBGJ" varchar(14) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABERXO" varchar(3) NOT NULL default '000',
  "AAAAGVPG" varchar(1) NOT NULL default ' ',
  "AAAAGVOQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAAAGVQV","AAABLZKF","AAABMRHS","AAABGZWU","AAABMRGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(2) NOT NULL default ' ',
  "AAABLKIV" varchar(35) NOT NULL default ' ',
  "AAAAGVPC" varchar(255) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAAAGVPE" varchar(255) NOT NULL default ' ',
  "AAAAGVPF" varchar(1) NOT NULL default '0',
  "AAAAGVPG" varchar(1) NOT NULL default ' ',
  "AAAAGVPH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAABLKIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(2) NOT NULL default ' ',
  "AAABLKIV" varchar(35) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCC","AAABLKIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(2) NOT NULL default ' ',
  "AAAAGVQV" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGZWU" varchar(40) NOT NULL default ' ',
  "AAABLIVG" varchar(1) NOT NULL default ' ',
  "AAAAGVQD" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAAAGVQV","AAABLZKF","AAABMRHS","AAABGZWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(2) NOT NULL default ' ',
  "AAAAGVQV" varchar(40) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLROG" varchar(2) NOT NULL default '00',
  "AAAAGVQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAAAGVQV","AAABLZKF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVQY" (
  "AAABGGDM" varchar(6) NOT NULL default '000000',
  "AAABGGDB" varchar(30) NOT NULL default ' ',
  "AAABHJAF" varchar(8) NOT NULL default '00000000',
  "AAABMRFP" varchar(12) NOT NULL default ' ',
  "AAAAGVRD" varchar(1) NOT NULL default ' ',
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
CREATE TABLE "AAAAGVRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATJNE" varchar(18) NOT NULL default ' ',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAAAGVTW" varchar(18) NOT NULL default ' ',
  "AAABAOAG" varchar(8) NOT NULL default '00000000',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABKHPE" varchar(4) NOT NULL default ' ',
  "AAAAGVRV" varchar(1) NOT NULL default ' ',
  "AAAAGVRW" double NOT NULL default '0',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAAAGVRY" varchar(18) NOT NULL default ' ',
  "AAABIIXH" smallint(6) NOT NULL default '0',
  "AAABIZSL" smallint(6) NOT NULL default '0',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAATJNE","AAABFXSU","AAAAGVTW","AAABAOAG","AAABMABE","AAABKHPE"),
  KEY "AAAAGVSO" ("AAABMRIL","AAAATJNE","AAABFXSU","AAABMABE"),
  KEY "AAAAGVST" ("AAABMRIL","AAAATJNE","AAABFXSU","AAABMABE","AAABAOAG"),
  KEY "AAAAGVSZ" ("AAABMRIL","AAAAGVTW"),
  KEY "AAAAGVTC" ("AAABMRIL","AAAAGVTW","AAABAOAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAATJNE" varchar(18) NOT NULL default ' ',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAAAGVTW" varchar(18) NOT NULL default ' ',
  "AAABAOAG" varchar(8) NOT NULL default '00000000',
  "AAABMABE" varchar(3) NOT NULL default ' ',
  "AAABKHPE" varchar(4) NOT NULL default ' ',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABBCPV" varchar(40) NOT NULL default ' ',
  "AAAAGVTR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAATJNE","AAABFXSU","AAAAGVTW","AAABAOAG","AAABMABE","AAABKHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLUFA" varchar(30) NOT NULL default ' ',
  "AAABFNHW" varchar(50) NOT NULL default ' ',
  "AAABLHGN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLUFA","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVUK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEPIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVUP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default ' ',
  "AAABCOES" varchar(10) NOT NULL default ' ',
  "AAABMEEL" varchar(3) NOT NULL default '000',
  "AAAAGVUV" varchar(1) NOT NULL default ' ',
  "AAABLKWD" varchar(1) NOT NULL default ' ',
  "AAAARYLT" varchar(1) NOT NULL default ' ',
  "AAABMEEK" varchar(4) NOT NULL default ' ',
  "AAAAGVUZ" varchar(4) NOT NULL default ' ',
  "AAAAGVVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABEPIN","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVVE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABEPIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAAAGVWS" varchar(2) NOT NULL default '00',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABJTTQ" varchar(1) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAAAGVXB" decimal(4,1) NOT NULL default '0.0',
  "AAABLXPL" decimal(11,2) NOT NULL default '0.00',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAAAGWBU" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDV" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWDW" decimal(13,2) NOT NULL default '0.00',
  "AAAAGVWG" varchar(2) NOT NULL default '00',
  "AAABLJGC" decimal(11,2) NOT NULL default '0.00',
  "AAABJKVC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAAARLLV","AAAAGVWS"),
  KEY "AAAAGVWN" ("AAABMRIL","AAAAGWBU","AAABKPPY","AAAARLLV","AAAAGVWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAAAQQZQ" varchar(2) NOT NULL default '00',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABKPOO" varchar(1) NOT NULL default ' ',
  "AAAAGVXB" decimal(4,1) NOT NULL default '0.0',
  "AAABLXPL" decimal(11,2) NOT NULL default '0.00',
  "AAABKPSQ" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABKPSV" varchar(6) NOT NULL default '000000',
  "AAABKPSW" varchar(4) NOT NULL default '0000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKPTD" varchar(1) NOT NULL default ' ',
  "AAABKPTE" varchar(1) NOT NULL default ' ',
  "AAABKPTF" varchar(1) NOT NULL default ' ',
  "AAABLLZK" varchar(1) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABMCWY" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABLJGC" decimal(11,2) NOT NULL default '0.00',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAABMCXF" varchar(8) NOT NULL default '00000000',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAABMCVR"),
  KEY "AAAAGVYR" ("AAABMRIL","AAABMCRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGVYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAWOLB" varchar(8) NOT NULL default ' ',
  "AAAAGVYZ" varchar(1) NOT NULL default ' ',
  "AAAAGVZA" varchar(1) NOT NULL default ' ',
  "AAABLUJX" varchar(1) NOT NULL default ' ',
  "AAAAGVZC" varchar(1) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAAAGVZE" varchar(8) NOT NULL default '00000000',
  "AAAAGVZF" varchar(8) NOT NULL default '00000000',
  "AAAAQPUJ" varchar(8) NOT NULL default '00000000',
  "AAABJBVT" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAAAWQDK" varchar(1) NOT NULL default ' ',
  "AAAAWEDN" varchar(2) NOT NULL default '00',
  "AAAAGVZN" varchar(1) NOT NULL default ' ',
  "AAAAYVAF" varchar(1) NOT NULL default ' ',
  "AAAAYOOK" varchar(2) NOT NULL default '00',
  "AAAAGVZQ" varchar(1) NOT NULL default ' ',
  "AAAAWQEJ" varchar(1) NOT NULL default ' ',
  "AAAAWEDP" varchar(2) NOT NULL default '00',
  "AAAAGVZT" varchar(1) NOT NULL default ' ',
  "AAAAWQEL" varchar(1) NOT NULL default ' ',
  "AAAAWEDQ" varchar(2) NOT NULL default '00',
  "AAAAGVZW" varchar(1) NOT NULL default ' ',
  "AAAAWQEO" varchar(1) NOT NULL default ' ',
  "AAAAWEDR" varchar(2) NOT NULL default '00',
  "AAAAGVZZ" varchar(1) NOT NULL default ' ',
  "AAAAGWAA" varchar(1) NOT NULL default ' ',
  "AAAAGWAB" varchar(2) NOT NULL default '00',
  "AAAAGWAC" varchar(1) NOT NULL default ' ',
  "AAABGATU" varchar(4) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXMN","AAAAGYQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAGWAN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYXMN","AAAAGYQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAAAGWBQ" varchar(10) NOT NULL default '0000000000',
  "AAAAGWBU" varchar(10) NOT NULL default '0000000000',
  "AAABLXII" varchar(10) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAGWBE" decimal(15,2) NOT NULL default '0.00',
  "AAAAGWET" varchar(3) NOT NULL default ' ',
  "AAAAGWBG" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWBH" varchar(1) NOT NULL default ' ',
  "AAABLUFL" decimal(9,5) NOT NULL default '0.00000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY"),
  KEY "AAAAGWBO" ("AAABMRIL","AAAAGWBQ","AAABKPPY"),
  KEY "AAAAGWBS" ("AAABMRIL","AAAAGWBU","AAABKPPY"),
  KEY "AAAAGWBW" ("AAABMRIL","AAABLUHY","AAABMCVP","AAABMCVQ","AAABKPPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAAAGWCG" varchar(10) NOT NULL default '0000000000',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAAASSCH" varchar(18) NOT NULL default ' ',
  "AAABLLZQ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAUOMU" varchar(8) NOT NULL default ' ',
  "AAAAGWGY" varchar(1) NOT NULL default ' ',
  "AAAAGYPU" varchar(10) NOT NULL default '0000000000',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAGWGW" varchar(18) NOT NULL default ' ',
  "AAAAGWCQ" varchar(18) NOT NULL default ' ',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLXPE" decimal(3,1) NOT NULL default '0.0',
  "AAABLXPD" varchar(1) NOT NULL default ' ',
  "AAAAGWCV" varchar(1) NOT NULL default ' ',
  "AAABLLYV" varchar(1) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  "AAABLLYC" decimal(11,2) NOT NULL default '0.00',
  "AAABLXPL" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWDA" varchar(6) NOT NULL default ' ',
  "AAAAMSAH" varchar(6) NOT NULL default ' ',
  "AAAAGYQH" varchar(40) NOT NULL default ' ',
  "AAABKPOP" varchar(1) NOT NULL default ' ',
  "AAABKPOQ" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBHNP" varchar(25) NOT NULL default ' ',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAAAJALB" varchar(10) NOT NULL default '0000000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAAAJAKQ" varchar(8) NOT NULL default '00000000',
  "AAAAGYNX" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDO" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDP" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDQ" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDR" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDS" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDT" varchar(1) NOT NULL default ' ',
  "AAAAGWDU" varchar(10) NOT NULL default '0000000000',
  "AAAAGWDV" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWDW" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWDX" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWDY" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWDZ" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWEA" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWEB" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWEC" varchar(10) NOT NULL default '0000000000',
  "AAAAZNTN" varchar(1) NOT NULL default ' ',
  "AAABJCYW" varchar(1) NOT NULL default ' ',
  "AAAAGWEF" varchar(1) NOT NULL default ' ',
  "AAABFVHD" varchar(1) NOT NULL default ' ',
  "AAAAGWEH" varchar(1) NOT NULL default ' ',
  "AAABLNYC" varchar(1) NOT NULL default ' ',
  "AAAAGWEJ" varchar(1) NOT NULL default ' ',
  "AAAAGWEK" varchar(1) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAAAGYOF" varchar(1) NOT NULL default ' ',
  "AAABLXOB" varchar(9) NOT NULL default ' ',
  "AAAAGWEP" decimal(11,2) NOT NULL default '0.00',
  "AAABLJGC" decimal(11,2) NOT NULL default '0.00',
  "AAABJUPE" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWES" varchar(10) NOT NULL default ' ',
  "AAAAGWET" varchar(3) NOT NULL default ' ',
  "AAAAGWEU" decimal(8,2) NOT NULL default '0.00',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAAAGWEW" decimal(8,2) NOT NULL default '0.00',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAAAMQSM" varchar(6) NOT NULL default '000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAAAGWFA" varchar(40) NOT NULL default ' ',
  "AAAAGWFB" varchar(12) NOT NULL default ' ',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAWMBD" varchar(10) NOT NULL default ' ',
  "AAAAGWFF" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFG" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFH" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFI" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFJ" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFK" varchar(10) NOT NULL default '0000000000',
  "AAAAGWFL" decimal(13,3) NOT NULL default '0.000',
  "AAAAGWFM" varchar(40) NOT NULL default ' ',
  "AAAAGWFN" varchar(10) NOT NULL default ' ',
  "AAABKOGH" varchar(18) NOT NULL default '000000000000000000',
  "AAAAGWFP" varchar(1) NOT NULL default ' ',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAAAGWFS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAAARLLV"),
  KEY "AAAAGWFW" ("AAABMRIL","AAAAGYQH","AAABKPPY","AAAARLLV"),
  KEY "AAAAGWGB" ("AAABMRIL","AAAASSCH","AAABKPPY","AAAARLLV"),
  KEY "AAAAGWGG" ("AAABMRIL","AAAAYXMN","AAAAGYQO","AAAAGWGW","AAABKPPY","AAAARLLV"),
  KEY "AAAAGWGN" ("AAAAGYPU","AAABKPPY","AAAARLLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAASSCH" varchar(18) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAGWGW" varchar(18) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAGWGY" varchar(1) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAASSCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(10) NOT NULL default '0000000000',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYXMN","AAAAGYQO","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(10) NOT NULL default '0000000000',
  "AAAAZJIC" varchar(10) NOT NULL default '0000000000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAAATHBY" varchar(10) NOT NULL default '0000000000',
  "AAAAGWHW" varchar(10) NOT NULL default '0000000000',
  "AAAAGWHX" varchar(18) NOT NULL default ' ',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAABLHRD" varchar(4) NOT NULL default ' ',
  "AAABLLZQ" varchar(1) NOT NULL default '0',
  "AAAAGWIB" varchar(1) NOT NULL default ' ',
  "AAAAGWIC" varchar(1) NOT NULL default ' ',
  "AAAAGWID" varchar(1) NOT NULL default ' ',
  "AAAAGWIE" varchar(2) NOT NULL default ' ',
  "AAAAGWIF" varchar(1) NOT NULL default ' ',
  "AAAAGWIG" varchar(2) NOT NULL default ' ',
  "AAAAGWIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXMN","AAAAGYQO","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAVCUB" varchar(10) NOT NULL default ' ',
  "AAABMBXY" varchar(50) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAVCUB","AAABMBXY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGWJN" varchar(10) NOT NULL default ' ',
  "AAAAGWIZ" varchar(11) NOT NULL default ' ',
  "AAAAGWJA" varchar(6) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAABENNS" varchar(15) NOT NULL default ' ',
  "AAAAGWKN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAGWJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAGWJN" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAGWJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWJO" (
  "AAAAGWKN" varchar(2) NOT NULL default '00',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAAAGWJU" varchar(1) NOT NULL default ' ',
  "AAAAGWJV" varchar(1) NOT NULL default ' ',
  "AAAAGWJW" varchar(1) NOT NULL default ' ',
  "AAAAGWJX" varchar(1) NOT NULL default ' ',
  "AAAAGWJY" varchar(1) NOT NULL default ' ',
  "AAAAGWJZ" varchar(1) NOT NULL default ' ',
  "AAAAGWKA" varchar(2) NOT NULL default '00',
  "AAAAGWKB" varchar(1) NOT NULL default ' ',
  "AAAAGWKC" varchar(2) NOT NULL default '00',
  "AAAAGWKD" varchar(1) NOT NULL default ' ',
  "AAAAGWKE" varchar(2) NOT NULL default '00',
  "AAAAGWKF" varchar(1) NOT NULL default ' ',
  "AAAAGWKG" varchar(2) NOT NULL default '00',
  "AAAAGWKH" varchar(1) NOT NULL default ' ',
  "AAAAGWKI" varchar(2) NOT NULL default '00',
  "AAAAGWKJ" varchar(1) NOT NULL default ' ',
  "AAAAGWKK" varchar(2) NOT NULL default '00',
  "AAAAGWKL" varchar(1) NOT NULL default ' ',
  "AAAAGWKM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAGWKN","AAABLTLR","AAABMREL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPQQ" varchar(10) NOT NULL default ' ',
  "AAAAGWKU" varchar(16) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAGWKZ" varchar(12) NOT NULL default ' ',
  "AAAAGWLA" varchar(12) NOT NULL default ' ',
  "AAAAGWLB" varchar(25) NOT NULL default ' ',
  "AAAAGWLC" varchar(8) NOT NULL default '00000000',
  "AAAAGWLD" varchar(8) NOT NULL default '00000000',
  "AAAAGWLE" varchar(8) NOT NULL default '00000000',
  "AAAAGWLF" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWLG" decimal(11,2) NOT NULL default '0.00',
  "AAAAGWLH" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAABKPNT" varchar(40) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAAAGWND" varchar(1) NOT NULL default ' ',
  "AAABLUZG" varchar(1) NOT NULL default ' ',
  "AAABKPQC" varchar(2) NOT NULL default ' ',
  "AAAAWMMU" varchar(2) NOT NULL default ' ',
  "AAAAWMJG" varchar(1) NOT NULL default ' ',
  "AAABKPNN" varchar(8) NOT NULL default ' ',
  "AAABKPQD" varchar(1) NOT NULL default ' ',
  "AAAAGWNE" varchar(1) NOT NULL default ' ',
  "AAAAGWLW" decimal(3,0) NOT NULL default '0',
  "AAAAGWLX" decimal(3,0) NOT NULL default '0',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMCNE" varchar(25) NOT NULL default ' ',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABLXTE" varchar(1) NOT NULL default ' ',
  "AAABLXPL" decimal(11,2) NOT NULL default '0.00',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAABKNPW" varchar(12) NOT NULL default ' ',
  "AAABKNPY" varchar(16) NOT NULL default ' ',
  "AAABKNPX" int(11) NOT NULL default '0',
  "AAABJUAC" varchar(10) NOT NULL default ' ',
  "AAABJUAD" varchar(6) NOT NULL default '000000',
  "AAABKOUG" varchar(20) NOT NULL default ' ',
  "AAABKOUH" varchar(20) NOT NULL default ' ',
  "AAABLJGC" decimal(11,2) NOT NULL default '0.00',
  "AAABGATX" varchar(10) NOT NULL default ' ',
  "AAABLXTN" varchar(18) NOT NULL default '000000000000000000',
  "AAAAGWMR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPQQ"),
  KEY "AAAAGWMU" ("AAABMRIL","AAABMCVP","AAABMCVQ","AAABKPQQ"),
  KEY "AAAAGWMZ" ("AAABMRIL","AAABKPNT"),
  KEY "AAAAGWNC" ("AAAAGWND","AAAAGWNE","AAABKPQC","AAAAWMMU","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAABMCMG" varchar(10) NOT NULL default '0000000000',
  "AAAAGYQH" varchar(40) NOT NULL default ' ',
  "AAAAGWNP" varchar(38) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYXMN","AAAAGYQO","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWNV" (
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
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERYV" varchar(10) NOT NULL default ' ',
  "AAABERTG" varchar(3) NOT NULL default '000',
  "AAAAGWOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWOS" ("AAABDHDU"),
  KEY "AAAAGWOU" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWOX" (
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
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERYV" varchar(10) NOT NULL default ' ',
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
  "AAAAGWQM" varchar(1) NOT NULL default ' ',
  "AAAAGWQN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWQR" ("AAABDHDU"),
  KEY "AAAAGWQT" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWQW" (
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
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERYV" varchar(10) NOT NULL default ' ',
  "AAABERWS" varchar(4) NOT NULL default ' ',
  "AAAAGWRP" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRQ" varchar(3) NOT NULL default ' ',
  "AAAAGWRR" varchar(4) NOT NULL default '0000',
  "AAABESAI" varchar(25) NOT NULL default ' ',
  "AAAAGWRT" varchar(25) NOT NULL default ' ',
  "AAAAGWRU" varchar(1) NOT NULL default ' ',
  "AAAAGWRV" varchar(5) NOT NULL default ' ',
  "AAAAGWRW" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRX" varchar(3) NOT NULL default ' ',
  "AAAAGWRY" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWRZ" varchar(3) NOT NULL default ' ',
  "AAAAGWSA" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSB" varchar(3) NOT NULL default ' ',
  "AAAAGWSC" decimal(8,2) NOT NULL default '0.00',
  "AAAAGWSD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWSH" ("AAABDHDU"),
  KEY "AAAAGWSJ" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWSM" (
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
  "AAAAQSFT" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYIN" varchar(20) NOT NULL default '00000000000000000000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABERYV" varchar(10) NOT NULL default ' ',
  "AAAAGWTG" varchar(2) NOT NULL default ' ',
  "AAAAGWTH" varchar(1) NOT NULL default ' ',
  "AAAAGWTI" varchar(1) NOT NULL default ' ',
  "AAABESBB" varchar(3) NOT NULL default ' ',
  "AAAAGYIB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWTO" ("AAABMRIL","AAAAQSFT"),
  KEY "AAAAGWTR" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGWTU" ("AAABMRIL","AAAAGYIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGXOR" varchar(20) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABHMMP" varchar(30) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABAARF" varchar(1) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  "AAABLZNN" varchar(10) NOT NULL default ' ',
  "AAABLZNO" varchar(4) NOT NULL default ' ',
  "AAABLBDX" varchar(16) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGXOR","AAABMKFH","AAABHMMP","AAABERXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAGWVI" varchar(1) NOT NULL default ' ',
  "AAABJLTX" varchar(35) NOT NULL default ' ',
  "AAAAGWVG" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGWVH" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMNMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE","AAABMQDU","AAAAGWVI","AAABJLTX"),
  UNIQUE KEY "AAAAGWVE" ("AAABMRIL","AAAAGWVG","AAAAGWVH","AAAAGWVI"),
  KEY "AAAAGWVJ" ("AAABJLTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRQC" varchar(50) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABDHBB" varchar(3) NOT NULL default '000',
  "AAABDRQB" varchar(1) NOT NULL default ' ',
  "AAABLTUX" varchar(3) NOT NULL default ' ',
  "AAABDHCH" varchar(4) NOT NULL default '0000',
  "AAABDHCI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDRQC","AAABLUSC","AAABDHBB","AAABDRQB","AAABLTUX","AAABDHCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWWB" (
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
  "AAABDHDT" varchar(30) NOT NULL default ' ',
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGAS" varchar(10) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABDNYG" varchar(132) NOT NULL default ' ',
  "AAABLJPA" varchar(1) NOT NULL default ' ',
  "AAAAGXYK" varchar(1) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWXG" ("AAABMRIL","AAABDHDU"),
  KEY "AAAAGWXJ" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGWXM" ("AAABMRIL","AAABDNYG"),
  KEY "AAAAGWXP" ("AAABMRIL","AAABDGAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWXS" (
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
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAAAGWYJ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGWYL" varchar(3) NOT NULL default ' ',
  "AAAAGWYM" varchar(5) NOT NULL default '00000',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAABFMRU" varchar(2) NOT NULL default ' ',
  "AAAAGWYP" varchar(3) NOT NULL default ' ',
  "AAAAGWZI" varchar(70) NOT NULL default ' ',
  "AAABLTXI" varchar(5) NOT NULL default '00000',
  "AAAAGWYS" varchar(5) NOT NULL default '00000',
  "AAABBNPS" varchar(8) NOT NULL default '00000000',
  "AAABDHKC" varchar(8) NOT NULL default '00000000',
  "AAAAGWYV" varchar(14) NOT NULL default ' ',
  "AAAAGWYW" varchar(1) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGWZB" ("AAABMRIL","AAABMMVO","AAABFMRU","AAABDGVH"),
  KEY "AAAAGWZG" ("AAABMRIL","AAAAGWZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGWZJ" (
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
  "AAAAGYBT" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXAD" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGXAG" ("AAABMRIL","AAAAGYBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXAJ" (
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
  "AAAAGXBP" varchar(5) NOT NULL default ' ',
  "AAABDGJU" varchar(10) NOT NULL default ' ',
  "AAABHBMG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXBF" ("AAABMRIL","AAABDGJU","AAAAGXBP"),
  KEY "AAAAGXBJ" ("AAABMRIL","AAABDHOX","AAAAGXBP"),
  KEY "AAAAGXBN" ("AAABMRIL","AAAAGXBP","AAABDGJU","AAABHBMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXBS" (
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
  "AAAAQSFT" varchar(20) NOT NULL default '00000000000000000000',
  "AAABLUNI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXCN" ("AAABMRIL","AAAAQSFT"),
  KEY "AAAAGXCQ" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXCT" (
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
  "AAAAQSFT" varchar(20) NOT NULL default '00000000000000000000',
  "AAABIJFM" varchar(10) NOT NULL default ' ',
  "AAAAGXDL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXDP" ("AAABMRIL","AAAAQSFT"),
  KEY "AAAAGXDS" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXDV" (
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
  "AAABDHDT" varchar(30) NOT NULL default ' ',
  "AAABDHDU" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHDX" varchar(1) NOT NULL default ' ',
  "AAABDHDY" varchar(1) NOT NULL default ' ',
  "AAABDHDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXEV" ("AAABMRIL","AAABDHDU"),
  KEY "AAAAGXEY" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXFB" (
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
  "AAAAGXFR" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGXFT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXFX" (
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
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDGTN" varchar(10) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDGVH" varchar(10) NOT NULL default ' ',
  "AAAAGXGV" varchar(2) NOT NULL default ' ',
  "AAAAGXGW" varchar(1) NOT NULL default ' ',
  "AAAAGXGX" varchar(1) NOT NULL default ' ',
  "AAABILKU" varchar(1) NOT NULL default ' ',
  "AAABILKV" varchar(1) NOT NULL default ' ',
  "AAAAGXHA" varchar(1) NOT NULL default ' ',
  "AAAAGXHB" varchar(1) NOT NULL default ' ',
  "AAAAGXHC" varchar(1) NOT NULL default ' ',
  "AAAAGXLB" varchar(1) NOT NULL default ' ',
  "AAAAGXHE" varchar(1) NOT NULL default ' ',
  "AAAAGXHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXHJ" (
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
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGXIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGXJF" int(11) NOT NULL default '0',
  "AAABDFWL" varchar(2) NOT NULL default ' ',
  "AAAAGXIM" varchar(3) NOT NULL default ' ',
  "AAAAGXIN" varchar(3) NOT NULL default '000',
  "AAAAGXIO" varchar(1) NOT NULL default ' ',
  "AAABDFWM" varchar(10) NOT NULL default ' ',
  "AAAAGXIQ" varchar(3) NOT NULL default ' ',
  "AAABDFYF" varchar(10) NOT NULL default ' ',
  "AAABDFVR" varchar(30) NOT NULL default ' ',
  "AAABDFVS" varchar(30) NOT NULL default ' ',
  "AAAAGXIU" varchar(4) NOT NULL default '0000',
  "AAAAGXIV" varchar(4) NOT NULL default '0000',
  "AAAAGXIW" varchar(1) NOT NULL default ' ',
  "AAABDFVP" varchar(10) NOT NULL default ' ',
  "AAABDFVZ" varchar(1) NOT NULL default ' ',
  "AAAAGXIZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABIMRE","AAABIMRG","AAABIMRF","AAAAGXJF"),
  KEY "AAAAGXJG" ("AAABMRIL","AAABDFWL","AAABDFVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXJK" (
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
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABMALF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXKF" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXKI" (
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
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGXLB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXLF" (
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
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXMA" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGXMD" ("AAABMRIL","AAABMLCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXND" varchar(30) NOT NULL default ' ',
  "AAAAGXNE" varchar(132) NOT NULL default ' ',
  "AAAAGXMM" varchar(132) NOT NULL default ' ',
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAAAGXMR" varchar(8) NOT NULL default '00000000',
  "AAAAGXMS" varchar(8) NOT NULL default '00000000',
  "AAAAGXMT" varchar(1) NOT NULL default ' ',
  "AAAAGXMU" varchar(1) NOT NULL default ' ',
  "AAAAGXMV" varchar(1) NOT NULL default ' ',
  "AAAAGXMW" varchar(1) NOT NULL default ' ',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAAAGXMY" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXNC" ("AAAAGXND","AAAAGXNE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHPD" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDHPE" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXNJ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXNO" (
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
  "AAAAGXOE" varchar(20) NOT NULL default '00000000000000000000',
  "AAABIGIZ" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOG" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOH" varchar(5) NOT NULL default '00000',
  "AAAAGXPN" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXOJ" varchar(3) NOT NULL default ' ',
  "AAAAGXOK" varchar(25) NOT NULL default ' ',
  "AAAAGXOL" varchar(2) NOT NULL default ' ',
  "AAAAGXOM" varchar(3) NOT NULL default ' ',
  "AAAAGXON" varchar(3) NOT NULL default ' ',
  "AAAAGXOO" varchar(25) NOT NULL default ' ',
  "AAAAGXOP" varchar(2) NOT NULL default ' ',
  "AAAAGXOQ" varchar(3) NOT NULL default ' ',
  "AAAAGXOR" varchar(20) NOT NULL default ' ',
  "AAAAGYLA" varchar(1) NOT NULL default ' ',
  "AAAAGYLB" varchar(3) NOT NULL default ' ',
  "AAAAGYLC" varchar(3) NOT NULL default ' ',
  "AAABDHGT" varchar(5) NOT NULL default ' ',
  "AAABDHFV" varchar(5) NOT NULL default ' ',
  "AAABJDNK" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(12) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAAGXPB" varchar(30) NOT NULL default ' ',
  "AAAAGXPC" varchar(1) NOT NULL default '0',
  "AAABGBZZ" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXPH" ("AAABMRIL","AAABMRFO","AAABDHGT"),
  KEY "AAAAGXPL" ("AAABMRIL","AAAAGXPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXPO" (
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
  "AAAAGXTB" varchar(15) NOT NULL default ' ',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  "AAABDFKY" varchar(10) NOT NULL default ' ',
  "AAAAGXQH" varchar(1) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDHQT" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAAAQPFV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXQP" ("AAABMRIL","AAABDFKX","AAAAGXTB"),
  KEY "AAAAGXQT" ("AAABMRIL","AAABDFKX","AAABDFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXQX" (
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
  "AAABDFLL" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXRY" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXRT" ("AAABMRIL","AAABDFLL"),
  KEY "AAAAGXRW" ("AAABMRIL","AAAAGXRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXRZ" (
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
  "AAAAGXSP" varchar(1) NOT NULL default ' ',
  "AAAAGXTB" varchar(15) NOT NULL default ' ',
  "AAABDFKX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXSV" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGXSY" ("AAABMRIL","AAABDFKX","AAAAGXTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXTC" (
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
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGXTT" varchar(40) NOT NULL default ' ',
  "AAAAGXUI" varchar(132) NOT NULL default ' ',
  "AAABLJPA" varchar(1) NOT NULL default ' ',
  "AAAAGXYK" varchar(1) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAAAGXTY" varchar(30) NOT NULL default ' ',
  "AAABDFKL" varchar(1) NOT NULL default ' ',
  "AAABDFMJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXUE" ("AAABMRIL","AAABDHOX","AAABMLQA","AAAAGXUI"),
  KEY "AAAAGXUJ" ("AAABMRIL","AAABDHTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXUM" (
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
  "AAAAGXVJ" varchar(30) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXVH" ("AAABMRIL","AAAAGXVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXVK" (
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
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAGXWB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXWF" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXWI" (
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
  "AAAAGXXN" varchar(8) NOT NULL default '00000000',
  "AAAAGXWZ" varchar(12) NOT NULL default ' ',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAABDHIL" varchar(10) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABDFQK" varchar(60) NOT NULL default ' ',
  "AAABDFEV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXXI" ("AAABMRIL","AAABMMVO"),
  KEY "AAAAGXXL" ("AAABMRIL","AAAAGXXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXXO" (
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
  "AAABDHLV" varchar(10) NOT NULL default ' ',
  "AAABKCHW" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(132) NOT NULL default ' ',
  "AAAAGXZH" varchar(132) NOT NULL default ' ',
  "AAABLJPA" varchar(1) NOT NULL default ' ',
  "AAAAGXYK" varchar(1) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGYEY" varchar(1) NOT NULL default ' ',
  "AAAAGYEZ" varchar(1) NOT NULL default ' ',
  "AAAAGYFA" varchar(1) NOT NULL default ' ',
  "AAABDHOH" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXZP" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGXZS" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUY" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGXZB" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGXZE" ("AAABMRIL","AAABDHLV","AAAAGXZH","AAABKCHW"),
  KEY "AAAAGXZJ" ("AAABMRIL","AAABDGUY","AAAAGXZS"),
  KEY "AAAAGXZN" ("AAABMRIL","AAAAGXZP"),
  KEY "AAAAGXZQ" ("AAABMRIL","AAAAGXZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGXZT" (
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
  "AAAAGYAQ" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYAO" ("AAABMRIL","AAAAGYAQ"),
  KEY "AAAAGYAR" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYAU" (
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
  "AAAAGYBT" varchar(20) NOT NULL default '00000000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYBO" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGYBR" ("AAABMRIL","AAAAGYBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYBU" (
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
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAGYCZ" varchar(5) NOT NULL default ' ',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABDHOZ" varchar(10) NOT NULL default ' ',
  "AAABJFHJ" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABDFFC" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYCU" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGYCX" ("AAABMRIL","AAAAGYCZ","AAABDHOZ","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYDC" (
  "AAAAGYDG" varchar(15) NOT NULL default ' ',
  "AAABLFYC" varchar(3) NOT NULL default ' ',
  "AAABLFYD" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAAAGYDG","AAABLFYC","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRE","AAABIMRG","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGYEB" varchar(12) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAGYEB","AAABIMRE","AAABIMRG","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYEF" (
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
  "AAAAGYFL" varchar(20) NOT NULL default '00000000000000000000',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAABDFNM" varchar(10) NOT NULL default ' ',
  "AAAAGYEY" varchar(1) NOT NULL default ' ',
  "AAAAGYEZ" varchar(1) NOT NULL default ' ',
  "AAAAGYFA" varchar(1) NOT NULL default ' ',
  "AAABDHOH" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYFV" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYFY" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUY" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGUV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYFJ" ("AAABMRIL","AAAAGYFL"),
  KEY "AAAAGYFM" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGYFP" ("AAABMRIL","AAAAGYFY","AAABDGUY"),
  KEY "AAAAGYFT" ("AAABMRIL","AAAAGYFV"),
  KEY "AAAAGYFW" ("AAABMRIL","AAAAGYFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYFZ" (
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
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYGU" ("AAABMRIL","AAABDHOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYGX" (
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
  "AAAAQSFT" varchar(20) NOT NULL default '00000000000000000000',
  "AAABDGAT" varchar(10) NOT NULL default ' ',
  "AAAAGYIN" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYHQ" varchar(5) NOT NULL default ' ',
  "AAAAQITC" decimal(10,4) NOT NULL default '0.0000',
  "AAAAGYHS" varchar(5) NOT NULL default ' ',
  "AAAAQITE" decimal(10,4) NOT NULL default '0.0000',
  "AAAAGYHU" decimal(10,4) NOT NULL default '0.0000',
  "AAAAGYHV" varchar(3) NOT NULL default ' ',
  "AAABERXO" varchar(4) NOT NULL default '0000',
  "AAAAGYHX" int(11) NOT NULL default '0',
  "AAAAGYHY" int(11) NOT NULL default '0',
  "AAAAGYHZ" int(11) NOT NULL default '0',
  "AAABDFQB" varchar(2) NOT NULL default ' ',
  "AAAAGYIB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYIF" ("AAABMRIL","AAAAQSFT"),
  KEY "AAAAGYII" ("AAABMRIL","AAABDHOX"),
  KEY "AAAAGYIL" ("AAABMRIL","AAAAGYIN"),
  KEY "AAAAGYIO" ("AAABMRIL","AAABDGAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYIR" (
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
  "AAABLXUJ" varchar(1) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABLXQQ" varchar(8) NOT NULL default ' ',
  "AAABKRTQ" varchar(2) NOT NULL default ' ',
  "AAABKQVZ" varchar(1) NOT NULL default ' ',
  "AAAAVDCO" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAAATVGC" varchar(6) NOT NULL default '000000',
  "AAAAGYJQ" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE"),
  KEY "AAAAGYJV" ("AAABMRIL","AAABLXUJ","AAABMLCZ","AAABMCVI","AAABLXQQ","AAABKQVZ","AAABKRTQ","AAAAVDCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYKE" (
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
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABIMRE" varchar(25) NOT NULL default ' ',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  "AAABIMRF" varchar(3) NOT NULL default ' ',
  "AAAAGYKY" varchar(7) NOT NULL default '0000000',
  "AAABDHEP" varchar(4) NOT NULL default ' ',
  "AAAAGYLA" varchar(1) NOT NULL default ' ',
  "AAAAGYLB" varchar(3) NOT NULL default ' ',
  "AAAAGYLC" varchar(3) NOT NULL default ' ',
  "AAABKUPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHPD","AAABDHPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEWWR" varchar(10) NOT NULL default ' ',
  "AAABMMVO" varchar(12) NOT NULL default ' ',
  "AAAAGYMO" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYMP" varchar(20) NOT NULL default '00000000000000000000',
  "AAAAGYMQ" varchar(32) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABILGE" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDHON" varchar(8) NOT NULL default '00000000',
  "AAABFOUW" varchar(254) NOT NULL default ' ',
  "AAABFOUX" varchar(254) NOT NULL default ' ',
  "AAABLMOZ" varchar(128) NOT NULL default ' ',
  "AAABESGN" varchar(30) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABMLXR" varchar(132) NOT NULL default ' ',
  "AAABMKXF" varchar(12) NOT NULL default ' ',
  "AAABDRZX" varchar(30) NOT NULL default ' ',
  "AAAAGYME" varchar(20) NOT NULL default '00000000000000000000',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAVIHK" varchar(1) NOT NULL default ' ',
  "AAABLEBN" varchar(3) NOT NULL default ' ',
  "AAAAGYMJ" varchar(1) NOT NULL default ' ',
  "AAAAGYMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEWWR","AAABMMVO","AAAAGYMO","AAAAGYMP","AAAAGYMQ","AAABKAFO","AAABMREL"),
  KEY "AAAAGYMT" ("AAABMRIL","AAABMMVO"),
  KEY "AAAAGYMW" ("AAABMRIL","AAABKUQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABKWRL" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQF" varchar(1) NOT NULL default ' ',
  "AAAAGYQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQH" varchar(40) NOT NULL default ' ',
  "AAAAGYPU" varchar(10) NOT NULL default '0000000000',
  "AAAAGYOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAAARLLV"),
  KEY "AAAAGYNP" ("AAABMRIL","AAAAGYPU","AAABKPPY","AAAARLLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYNU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAAGYNX" varchar(10) NOT NULL default '0000000000',
  "AAAAGYNY" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYNZ" varchar(1) NOT NULL default ' ',
  "AAABCRYS" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYOC" varchar(1) NOT NULL default ' ',
  "AAAAGYOD" varchar(1) NOT NULL default ' ',
  "AAAAGYOE" varchar(1) NOT NULL default ' ',
  "AAAAGYOF" varchar(1) NOT NULL default ' ',
  "AAAAGYOG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYOH" varchar(1) NOT NULL default ' ',
  "AAAAGYOI" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAAAGYOM" decimal(11,2) NOT NULL default '0.00',
  "AAAAGYON" varchar(1) NOT NULL default ' ',
  "AAAAGYOO" decimal(11,2) NOT NULL default '0.00',
  "AAAAGYOP" varchar(1) NOT NULL default ' ',
  "AAABGATX" varchar(10) NOT NULL default ' ',
  "AAAAGYOR" decimal(11,2) NOT NULL default '0.00',
  "AAAAGYOS" varchar(1) NOT NULL default ' ',
  "AAAAGYOT" decimal(3,1) NOT NULL default '0.0',
  "AAAAGYOU" decimal(3,1) NOT NULL default '0.0',
  "AAAAGYOV" decimal(3,1) NOT NULL default '0.0',
  "AAAAGYOW" varchar(1) NOT NULL default ' ',
  "AAAAGYOX" decimal(11,2) NOT NULL default '0.00',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCXD" varchar(15) NOT NULL default ' ',
  "AAABLJGC" decimal(13,2) NOT NULL default '0.00',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAAASSCH" varchar(18) NOT NULL default ' ',
  "AAABKWRL" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQF" varchar(1) NOT NULL default ' ',
  "AAAAGYQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQH" varchar(40) NOT NULL default ' ',
  "AAAAGYPU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAAARLLV"),
  KEY "AAAAGYPS" ("AAABMRIL","AAAAGYPU","AAABKPPY","AAAARLLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPPY" varchar(10) NOT NULL default '0000000000',
  "AAAARLLV" varchar(10) NOT NULL default '0000000000',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAAAYXMN" varchar(3) NOT NULL default ' ',
  "AAAAGYQO" varchar(4) NOT NULL default '0000',
  "AAABKWRL" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQF" varchar(1) NOT NULL default ' ',
  "AAAAGYQG" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYQH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPPY","AAAARLLV"),
  KEY "AAAAGYQL" ("AAABMRIL","AAAAYXMN","AAAAGYQO","AAABKPPY","AAAARLLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMEJ" varchar(30) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMEJ","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMEJ" varchar(30) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAGYRL" varchar(30) NOT NULL default ' ',
  "AAAAGYRG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMEJ","AAABLUAV","AAABKAFO","AAAAGYRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAGYUC" varchar(12) NOT NULL default ' ',
  "AAABMAKT" varchar(12) NOT NULL default ' ',
  "AAAAGYSC" varchar(2) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  "AAABIYEN" varchar(25) NOT NULL default ' ',
  "AAABIYEO" varchar(25) NOT NULL default ' ',
  "AAABLSAL" varchar(10) NOT NULL default ' ',
  "AAAAGYRW" varchar(25) NOT NULL default ' ',
  "AAABGRXB" varchar(241) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENL","AAAAGYUC","AAABMAKT","AAAAGYSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDNQK" varchar(10) NOT NULL default ' ',
  "AAAAVLJZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAGYUC" varchar(12) NOT NULL default ' ',
  "AAABENKB" varchar(60) NOT NULL default ' ',
  "AAAAGYSO" varchar(8) NOT NULL default '00000000',
  "AAAATITL" varchar(8) NOT NULL default '00000000',
  "AAAAGYSQ" varchar(12) NOT NULL default ' ',
  "AAABKIKF" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAAAGYST" varchar(8) NOT NULL default '00000000',
  "AAAAGYSU" smallint(6) NOT NULL default '0',
  "AAABMKFG" varchar(128) NOT NULL default ' ',
  "AAABLKIV" varchar(128) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAAAMMPO" varchar(1) NOT NULL default ' ',
  "AAABLLJG" smallint(6) NOT NULL default '0',
  "AAAAGYTM" varchar(8) NOT NULL default '00000000',
  "AAAAGYTB" varchar(8) NOT NULL default '00000000',
  "AAAAGYTC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENL","AAAAGYUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAGYUC" varchar(12) NOT NULL default ' ',
  "AAAAGYUD" varchar(4) NOT NULL default '0000',
  "AAABASZN" varchar(2) NOT NULL default ' ',
  "AAAAGYTM" varchar(8) NOT NULL default '00000000',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENL","AAAAGYUC","AAAAGYUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLENL" varchar(12) NOT NULL default ' ',
  "AAAAGYUC" varchar(12) NOT NULL default ' ',
  "AAAAGYUD" varchar(4) NOT NULL default '0000',
  "AAAAGYUE" varchar(4) NOT NULL default '0000',
  "AAABGOUO" varchar(2) NOT NULL default ' ',
  "AAABHIES" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLENL","AAAAGYUC","AAAAGYUD","AAAAGYUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(12) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABJCZN" varchar(10) NOT NULL default ' ',
  "AAAAGYWK" varchar(8) NOT NULL default '00000000',
  "AAABLRQL" varchar(8) NOT NULL default '00000000',
  "AAABHHSC" varchar(12) NOT NULL default ' ',
  "AAABFWAW" varchar(24) NOT NULL default ' ',
  "AAAAGYUR" varchar(1) NOT NULL default ' ',
  "AAAAGYUS" varchar(1) NOT NULL default ' ',
  "AAAAGYXX" varchar(1) NOT NULL default ' ',
  "AAAAGYUU" varchar(1) NOT NULL default ' ',
  "AAAAGYUV" varchar(1) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAAAGYUZ" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABHIKM" varchar(12) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAABHILK" varchar(3) NOT NULL default ' ',
  "AAABHIKZ" varchar(2) NOT NULL default '00',
  "AAABHILA" varchar(2) NOT NULL default '00',
  "AAABHILB" varchar(2) NOT NULL default '00',
  "AAABHILC" varchar(2) NOT NULL default '00',
  "AAAAGYVJ" varchar(1) NOT NULL default ' ',
  "AAAAGYVK" varchar(6) NOT NULL default '000000',
  "AAAAGYVL" varchar(5) NOT NULL default ' ',
  "AAAAGYVM" varchar(3) NOT NULL default ' ',
  "AAABHIKR" varchar(8) NOT NULL default '00000000',
  "AAAAGYVO" varchar(1) NOT NULL default ' ',
  "AAABHIHQ" varchar(1) NOT NULL default ' ',
  "AAAAGYVQ" varchar(10) NOT NULL default ' ',
  "AAAAGYVR" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ"),
  KEY "AAAAGYVU" ("AAABMRIL","AAABIVCA","AAABKUQR"),
  KEY "AAAAGYVY" ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE","AAABMCOP"),
  KEY "AAAAGYWE" ("AAABMRIL","AAABHIKR"),
  KEY "AAAAGYWH" ("AAABMRIL","AAABJCZN","AAAAGYWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(12) NOT NULL default ' ',
  "AAABIXAW" varchar(6) NOT NULL default '000000',
  "AAAAGYWP" varchar(6) NOT NULL default '000000',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABHIOC" varchar(15) NOT NULL default ' ',
  "AAABHIKN" varchar(6) NOT NULL default '000000',
  "AAABIFGN" varchar(18) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYVH" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABHILF" varchar(10) NOT NULL default ' ',
  "AAAAGYWY" varchar(10) NOT NULL default ' ',
  "AAABMKAH" decimal(13,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAABLXUR" varchar(18) NOT NULL default ' ',
  "AAABHIKY" varchar(1) NOT NULL default ' ',
  "AAABHILD" varchar(1) NOT NULL default ' ',
  "AAAAGYXE" varchar(1) NOT NULL default ' ',
  "AAABHIKQ" varchar(8) NOT NULL default '00000000',
  "AAABHIKS" varchar(15) NOT NULL default ' ',
  "AAABHIKT" varchar(15) NOT NULL default ' ',
  "AAABHIKU" varchar(15) NOT NULL default ' ',
  "AAABHIKV" varchar(15) NOT NULL default ' ',
  "AAABKGMO" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYXL" decimal(13,2) NOT NULL default '0.00',
  "AAABHILL" decimal(13,2) NOT NULL default '0.00',
  "AAABHILM" decimal(13,2) NOT NULL default '0.00',
  "AAABHIKO" varchar(16) NOT NULL default ' ',
  "AAABHIKP" varchar(10) NOT NULL default ' ',
  "AAAAGYXQ" varchar(18) NOT NULL default ' ',
  "AAABHIKX" varchar(1) NOT NULL default ' ',
  "AAABHINT" varchar(25) NOT NULL default ' ',
  "AAABHINU" varchar(2) NOT NULL default ' ',
  "AAABHINV" varchar(50) NOT NULL default ' ',
  "AAAAGYXV" varchar(10) NOT NULL default ' ',
  "AAAAGYXW" varchar(50) NOT NULL default ' ',
  "AAAAGYXX" varchar(1) NOT NULL default ' ',
  "AAABHIKW" varchar(1) NOT NULL default ' ',
  "AAABHIMB" decimal(11,2) NOT NULL default '0.00',
  "AAABHIMM" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYYB" varchar(2) NOT NULL default ' ',
  "AAABHIMY" varchar(2) NOT NULL default ' ',
  "AAAAGYYD" varchar(25) NOT NULL default ' ',
  "AAAAGYYE" varchar(2) NOT NULL default ' ',
  "AAAAGYYF" decimal(11,2) NOT NULL default '0.00',
  "AAAAGYYG" decimal(13,2) NOT NULL default '0.00',
  "AAABHIKR" varchar(8) NOT NULL default '00000000',
  "AAABHILO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ","AAABIXAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(12) NOT NULL default ' ',
  "AAABIXAW" varchar(6) NOT NULL default '000000',
  "AAAAQGZP" varchar(1) NOT NULL default ' ',
  "AAAAGYYR" decimal(11,2) NOT NULL default '0.00',
  "AAAAKXIF" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYYT" decimal(13,2) NOT NULL default '0.00',
  "AAABBFSH" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYYV" varchar(2) NOT NULL default ' ',
  "AAAAGYYW" decimal(11,2) NOT NULL default '0.00',
  "AAAAGYYX" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYYY" decimal(13,2) NOT NULL default '0.00',
  "AAAAGYYZ" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ","AAABIXAW","AAAAQGZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCJ" varchar(12) NOT NULL default ' ',
  "AAAAGYZI" varchar(3) NOT NULL default '000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYZW" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGYZZ" (
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZAC" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAAAGZAL" varchar(12) NOT NULL default ' ',
  "AAAAQKUA" varchar(8) NOT NULL default '00000000',
  "AAABLXLD" varchar(6) NOT NULL default '000000',
  "AAAAGZAO" varchar(8) NOT NULL default '00000000',
  "AAAAGZAP" varchar(6) NOT NULL default '000000',
  "AAAAGZAQ" varchar(8) NOT NULL default '00000000',
  "AAAAGZAR" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(6) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLPV","AAABLSCE","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZAZ" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(3) NOT NULL default '000',
  "AAABEETX" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLESS","AAABMLPV","AAABMLTT","AAABEETX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZBS" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMKFH" varchar(3) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLWIT" varchar(10) NOT NULL default ' ',
  "AAABLWIV" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMDEN","AAABMKFH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZCF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMMJW" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(30) NOT NULL default ' ',
  "AAABEETX" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMMJW","AAABMLPV","AAABMLTT","AAABEETX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZCZ" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(20) NOT NULL default ' ',
  "AAAAGZDE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABLSCE","AAABMOHM","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZDJ" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFR" varchar(6) NOT NULL default '000000',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIRUN","AAABMKFH","AAABLFYD","AAABMRFQ","AAABMRFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZDV" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAAAGZEC" varchar(12) NOT NULL default ' ',
  "AAAAGZED" varchar(12) NOT NULL default ' ',
  "AAAAGZEE" varchar(8) NOT NULL default '00000000',
  "AAAAGZEF" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMKFH","AAABMRFO","AAABLFYD","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZEN" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABMKFH" varchar(3) NOT NULL default ' ',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAGZFH" varchar(30) NOT NULL default ' ',
  "AAABDWNR" varchar(68) NOT NULL default ' ',
  "AAABJZCI" varchar(2) NOT NULL default '00',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMDEN","AAABLQGL","AAABMKFH","AAABKOUD","AAABMLES","AAAAGZFH","AAABDWNR","AAABJZCI","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZFL" (
  "AAABLUSV" varchar(15) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABGYUM" varchar(1) NOT NULL default ' ',
  "AAABLEJX" varchar(1) NOT NULL default ' ',
  "AAAARYST" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZFS" (
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAAALWUE" varchar(10) NOT NULL default ' ',
  "AAABLISU" varchar(4) NOT NULL default ' ',
  "AAABLISV" varchar(4) NOT NULL default ' ',
  "AAAAGZFX" varchar(40) NOT NULL default ' ',
  "AAAAGZFY" varchar(4) NOT NULL default ' ',
  "AAAAGZFZ" varchar(4) NOT NULL default ' ',
  "AAABLISW" varchar(3) NOT NULL default ' ',
  "AAABLISX" varchar(2) NOT NULL default ' ',
  "AAABALXA" varchar(1) NOT NULL default ' ',
  "AAAAGZGD" varchar(1) NOT NULL default ' ',
  "AAABLISY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZGG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLPWW" varchar(60) NOT NULL default ' ',
  "AAABLITE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAGZGT" varchar(5) NOT NULL default ' ',
  "AAAAGZGU" varchar(5) NOT NULL default ' ',
  "AAAAGZGV" varchar(5) NOT NULL default ' ',
  "AAAAGZGW" varchar(5) NOT NULL default ' ',
  "AAABDBTK" varchar(30) NOT NULL default ' ',
  "AAAAGZGY" varchar(30) NOT NULL default ' ',
  "AAAAGZGZ" varchar(30) NOT NULL default ' ',
  "AAAAGZHA" varchar(30) NOT NULL default ' ',
  "AAAAGZHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMQHV","AAABMREX","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABBJES" varchar(1) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABBVTR" varchar(1) NOT NULL default ' ',
  "AAABBVTT" varchar(1) NOT NULL default ' ',
  "AAABBVTV" varchar(1) NOT NULL default ' ',
  "AAABBVTW" varchar(1) NOT NULL default ' ',
  "AAABGNDC" varchar(3) NOT NULL default '000',
  "AAAAWHKO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMKIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAKHHY" varchar(8) NOT NULL default ' ',
  "AAAAIEPU" varchar(16) NOT NULL default ' ',
  "AAAAIEPV" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHZML" varchar(8) NOT NULL default ' ',
  "AAAAIEPE" varchar(16) NOT NULL default ' ',
  "AAAAHZNU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAKHHY","AAAAIEPU","AAAAIEPV","AAABMDBB","AAABMCNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAAAGZWS" varchar(1) NOT NULL default ' ',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAABKPVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPVP","AAAAGZWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAAAGZWS" varchar(1) NOT NULL default ' ',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAABKPVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNZR","AAABKPVP","AAAAGZWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKPVB" varchar(1) NOT NULL default '0',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKPUX" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABKPVB","AAABKPVP","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAAAGZLU" varchar(2) NOT NULL default '00',
  "AAAAGZLV" varchar(10) NOT NULL default ' ',
  "AAAAGZLW" varchar(10) NOT NULL default ' ',
  "AAAAGZLX" varchar(1) NOT NULL default ' ',
  "AAABEQSQ" varchar(1) NOT NULL default ' ',
  "AAAAGZLZ" varchar(12) NOT NULL default ' ',
  "AAAAGZMA" varchar(10) NOT NULL default ' ',
  "AAAAGZMB" varchar(10) NOT NULL default ' ',
  "AAAAGZMC" varchar(12) NOT NULL default ' ',
  "AAAAGZMD" varchar(1) NOT NULL default ' ',
  "AAAAGZME" varchar(10) NOT NULL default ' ',
  "AAAAGZMF" varchar(1) NOT NULL default ' ',
  "AAAAGZMG" varchar(10) NOT NULL default ' ',
  "AAABJELS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  "AAABCGCB" varchar(40) NOT NULL default ' ',
  "AAAAGZLL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAABCHTP" varchar(20) NOT NULL default ' ',
  "AAABCGCB" varchar(40) NOT NULL default ' ',
  "AAAAGZLL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNZR","AAABMRHS","AAABKPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAAAGZLU" varchar(2) NOT NULL default '00',
  "AAAAGZLV" varchar(10) NOT NULL default ' ',
  "AAAAGZLW" varchar(10) NOT NULL default ' ',
  "AAAAGZLX" varchar(1) NOT NULL default ' ',
  "AAABEQSQ" varchar(1) NOT NULL default ' ',
  "AAAAGZLZ" varchar(12) NOT NULL default ' ',
  "AAAAGZMA" varchar(10) NOT NULL default ' ',
  "AAAAGZMB" varchar(10) NOT NULL default ' ',
  "AAAAGZMC" varchar(12) NOT NULL default ' ',
  "AAAAGZMD" varchar(1) NOT NULL default ' ',
  "AAAAGZME" varchar(10) NOT NULL default ' ',
  "AAAAGZMF" varchar(1) NOT NULL default ' ',
  "AAAAGZMG" varchar(10) NOT NULL default ' ',
  "AAABJELS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMNZR","AAABKPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABGLDC" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABGLCZ" varchar(4) NOT NULL default ' ',
  "AAABGLDA" varchar(1) NOT NULL default ' ',
  "AAABGLDB" varchar(1) NOT NULL default ' ',
  "AAABGLEV" varchar(2) NOT NULL default ' ',
  "AAABGLEW" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
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
  "AAABGLBV" varchar(2) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMRIL","AAABGLCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAAAGZRU" varchar(2) NOT NULL default ' ',
  "AAAAGZRV" varchar(3) NOT NULL default '000',
  "AAAAGZRW" varchar(3) NOT NULL default '000',
  "AAAAGZRX" varchar(2) NOT NULL default '00',
  "AAAAGZRY" varchar(2) NOT NULL default '00',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAAAGZRK" varchar(10) NOT NULL default ' ',
  "AAABCGCB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAAAGZRK" varchar(10) NOT NULL default ' ',
  "AAABCGCB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNZR","AAABMRHS","AAABKPVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAAAGZRU" varchar(2) NOT NULL default ' ',
  "AAAAGZRV" varchar(3) NOT NULL default '000',
  "AAAAGZRW" varchar(3) NOT NULL default '000',
  "AAAAGZRX" varchar(2) NOT NULL default '00',
  "AAAAGZRY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMNZR","AAABKPVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAABKPVK" varchar(1) NOT NULL default ' ',
  "AAABKPVL" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABKPVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAABKPVK" varchar(1) NOT NULL default ' ',
  "AAABKPVL" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAGZXT","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  "AAABKPVK" varchar(1) NOT NULL default ' ',
  "AAABKPVL" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABLXTG","AAAAGZXT","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAGZUF" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAAAGZXT","AAAAGZWT","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKSUH" varchar(12) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAAAGZUF" double NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKSUH","AAABLXTG","AAAAGZXT","AAAAGZWT","AAABMNZR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZUT" varchar(1) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAGZUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWS" varchar(1) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAGZXT","AAAAGZWS","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAGZXT","AAABMLHQ","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABMCRJ","AAAAGZXT","AAABMLHQ","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWS" varchar(1) NOT NULL default ' ',
  "AAAAGZWT" varchar(1) NOT NULL default '0',
  "AAABKPVJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRFQ","AAABMCRJ","AAAAGZXT","AAAAGZWS","AAAAGZWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAAAGZWY" varchar(2) NOT NULL default ' ',
  "AAAAGZWZ" varchar(2) NOT NULL default ' ',
  "AAAAGZXA" varchar(3) NOT NULL default ' ',
  "AAAAGZXB" varchar(12) NOT NULL default ' ',
  "AAAAGZXC" varchar(1) NOT NULL default ' ',
  "AAAAGZXD" varchar(1) NOT NULL default ' ',
  "AAAAGZXE" varchar(1) NOT NULL default ' ',
  "AAAAGZXF" varchar(1) NOT NULL default ' ',
  "AAAAGZXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAAAGZXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAGZXT" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAAAGZXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZXU" (
  "AAAAHAIV" varchar(2) NOT NULL default '00',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLTWR" varchar(2) NOT NULL default ' ',
  "AAAAHAIH" varchar(2) NOT NULL default '00',
  "AAAAHAIF" varchar(40) NOT NULL default ' ',
  "AAAAGZYA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHAIV","AAABLFYD","AAABLTWR","AAAAHAIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZYF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAGZYH" int(11) NOT NULL default '0',
  "AAAAGZYI" int(11) NOT NULL default '0',
  "AAAAGZYJ" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAGZYL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZYN" (
  "AAAAGZYQ" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAGZYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZYR" (
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAAAGZYT" decimal(20,0) NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAGZYV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABLRTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAGZYZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(8) NOT NULL default '00000000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMEGE" varchar(2) NOT NULL default '00',
  "AAABBAHD" varchar(1) NOT NULL default ' ',
  "AAABMINW" varchar(20) NOT NULL default ' ',
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZZN" (
  "AAABIITK" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABIITK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAGZZQ" (
  "AAAAHAIV" varchar(2) NOT NULL default '00',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABLTWR" varchar(2) NOT NULL default ' ',
  "AAAAHAIH" varchar(2) NOT NULL default '00',
  "AAABLUNI" varchar(4) NOT NULL default ' ',
  "AAAAHAIF" varchar(40) NOT NULL default ' ',
  "AAAAGZZX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHAIV","AAABLFYD","AAABLTWR","AAAAHAIH","AAABLUNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABDGPG" varchar(4) NOT NULL default ' ',
  "AAAAHAEG" varchar(4) NOT NULL default ' ',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAAAHAAK" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAAAXEKY" varchar(60) NOT NULL default ' ',
  "AAAAHAAN" varchar(10) NOT NULL default ' ',
  "AAAAHAAO" varchar(1) NOT NULL default ' ',
  "AAAAVJSF" varchar(1) NOT NULL default ' ',
  "AAAAHAAQ" varchar(1) NOT NULL default ' ',
  "AAAAHAAR" varchar(1) NOT NULL default ' ',
  "AAAAHAAS" varchar(10) NOT NULL default ' ',
  "AAAAHAAT" varchar(32) NOT NULL default ' ',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAAAHAAV" varchar(1) NOT NULL default ' ',
  "AAAAHAAW" varchar(20) NOT NULL default ' ',
  "AAAATWYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABGCBO","AAABDGPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHABC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABDGPG" varchar(4) NOT NULL default ' ',
  "AAABLLLP" varchar(30) NOT NULL default ' ',
  "AAAANCRQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABMLQA","AAABGCBO","AAABDGPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHABP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAAAHABT" varchar(2) NOT NULL default ' ',
  "AAAAHABU" varchar(10) NOT NULL default ' ',
  "AAAAHABV" varchar(1) NOT NULL default ' ',
  "AAABEZCL" varchar(1) NOT NULL default ' ',
  "AAAAHABX" varchar(30) NOT NULL default ' ',
  "AAAAHABY" varchar(1) NOT NULL default ' ',
  "AAAAHABZ" varchar(30) NOT NULL default ' ',
  "AAAAHACA" varchar(1) NOT NULL default ' ',
  "AAAAHACB" varchar(1) NOT NULL default ' ',
  "AAAAHACC" varchar(4) NOT NULL default ' ',
  "AAAAHACD" varchar(10) NOT NULL default ' ',
  "AAABLLNE" varchar(12) NOT NULL default ' ',
  "AAABGGIC" varchar(8) NOT NULL default '00000000',
  "AAABGGIF" varchar(12) NOT NULL default ' ',
  "AAABGGIG" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABGCBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHACL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABLLLP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABMLQA","AAABGCBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHACV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAAAHADF" varchar(8) NOT NULL default ' ',
  "AAAAHADG" varchar(40) NOT NULL default ' ',
  "AAAAHADH" varchar(4) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAAAHADF","AAAAHADG","AAAAHADH","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHADJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAAAHAEG" varchar(4) NOT NULL default ' ',
  "AAAAHADO" varchar(1) NOT NULL default '0',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABGCBO","AAAAHAEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHADV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAAAHAEG" varchar(4) NOT NULL default ' ',
  "AAABLLLP" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABMLQA","AAABGCBO","AAAAHAEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABJSLL" varchar(1) NOT NULL default ' ',
  "AAAAVLSR" varchar(1) NOT NULL default ' ',
  "AAABJJLX" varchar(10) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAAAVLSP" varchar(1) NOT NULL default ' ',
  "AAAAVLQZ" varchar(1) NOT NULL default ' ',
  "AAABFDFC" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABGCBO","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABFNHW" varchar(32) NOT NULL default ' ',
  "AAABLLLP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABMLQA","AAABGCBO","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAGK" varchar(1) NOT NULL default ' ',
  "AAABGCBO" varchar(8) NOT NULL default ' ',
  "AAABDGPG" varchar(4) NOT NULL default ' ',
  "AAAAHAGN" varchar(1) NOT NULL default ' ',
  "AAAAREII" varchar(32) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default '00',
  "AAAAHAGQ" varchar(32) NOT NULL default ' ',
  "AAAAVLSP" varchar(1) NOT NULL default ' ',
  "AAAAHAFU" varchar(1) NOT NULL default ' ',
  "AAABCKVF" varchar(10) NOT NULL default ' ',
  "AAAAHAFW" varchar(30) NOT NULL default ' ',
  "AAAAHAFX" varchar(30) NOT NULL default ' ',
  "AAAAHAFY" varchar(1) NOT NULL default ' ',
  "AAAAHAFZ" varchar(1) NOT NULL default ' ',
  "AAAAHAGA" varchar(1) NOT NULL default ' ',
  "AAAAHAGB" varchar(32) NOT NULL default ' ',
  "AAAAHAGC" varchar(255) NOT NULL default ' ',
  "AAAAHAGD" varchar(1) NOT NULL default ' ',
  "AAABJWYW" varchar(10) NOT NULL default ' ',
  "AAAAHAGF" varchar(30) NOT NULL default ' ',
  "AAAAHAGG" varchar(30) NOT NULL default ' ',
  "AAAAHAGH" varchar(1) NOT NULL default ' ',
  "AAAAHAGI" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAGK","AAABGCBO","AAABDGPG","AAAAHAGN","AAAAREII","AAABMNRG","AAAAHAGQ","AAAAVLSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAGS" (
  "AAAAHAHE" varchar(1) NOT NULL default '0',
  "AAAAHAHF" smallint(6) NOT NULL default '0',
  "AAABCFHW" varchar(128) NOT NULL default ' ',
  "AAAAHAHH" varchar(1) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAAAHAGY" varchar(2) NOT NULL default '00',
  "AAAAHAGZ" varchar(2) NOT NULL default '00',
  "AAABHSIQ" varchar(128) NOT NULL default ' ',
  "AAAAHAHB" varchar(60) NOT NULL default ' ',
  "AAAAHAHC" varchar(8) NOT NULL default '00000000',
  "AAAAHAHD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAAAHAHE","AAAAHAHF","AAABCFHW","AAAAHAHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAHI" (
  "AAABFGTW" varchar(1) NOT NULL default ' ',
  "AAAAHAHK" varchar(16) NOT NULL default ' ',
  "AAAAHAHL" varchar(16) NOT NULL default ' ',
  "AAAAHAHM" varchar(2) NOT NULL default ' ',
  "AAAAHAHN" varchar(16) NOT NULL default ' ',
  "AAAAHAHO" varchar(16) NOT NULL default ' ',
  "AAAAHAHP" varchar(2) NOT NULL default ' ',
  "AAAAHAHQ" varchar(16) NOT NULL default ' ',
  "AAAAHAHR" varchar(16) NOT NULL default ' ',
  "AAAAHAHS" varchar(2) NOT NULL default ' ',
  "AAAAHAHT" varchar(16) NOT NULL default ' ',
  "AAAAHAHU" varchar(2) NOT NULL default ' ',
  "AAAAHAHV" varchar(3) NOT NULL default ' ',
  "AAAAHAHW" varchar(80) NOT NULL default ' ',
  "AAAAHAHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAHZ" (
  "AAAAHAIV" varchar(2) NOT NULL default '00',
  "AAAAHAIH" varchar(2) NOT NULL default '00',
  "AAABLENA" varchar(4) NOT NULL default ' ',
  "AAAAHAIJ" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(80) NOT NULL default ' ',
  "AAAAHAIF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHAIV","AAAAHAIH","AAABLENA","AAAAHAIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAIK" (
  "AAAAHAIO" varchar(6) NOT NULL default ' ',
  "AAAAHAIM" varchar(2) NOT NULL default ' ',
  "AAABMMRH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHAIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAIP" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAHAIV" varchar(2) NOT NULL default '00',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKNBW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAHAIV","AAABLFYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAHAJC" varchar(30) NOT NULL default ' ',
  "AAAAHAJD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL","AAAAHCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABIAWX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAAAWIBR" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABICIU","AAAAWIBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABICIR" varchar(2) NOT NULL default '00',
  "AAAAHAKO" varchar(1) NOT NULL default ' ',
  "AAAAHAKP" varchar(1) NOT NULL default ' ',
  "AAAAHAKQ" varchar(1) NOT NULL default ' ',
  "AAAAHAKR" varchar(1) NOT NULL default ' ',
  "AAAAVSHF" varchar(1) NOT NULL default ' ',
  "AAAAVSHG" varchar(1) NOT NULL default ' ',
  "AAAAVSHH" varchar(1) NOT NULL default ' ',
  "AAAAVSHI" varchar(1) NOT NULL default ' ',
  "AAAAVSHJ" varchar(1) NOT NULL default ' ',
  "AAAAVSHK" varchar(1) NOT NULL default ' ',
  "AAAAVSHL" varchar(1) NOT NULL default ' ',
  "AAAAVSHM" varchar(1) NOT NULL default ' ',
  "AAAAVSHN" varchar(1) NOT NULL default ' ',
  "AAAAVSHO" varchar(1) NOT NULL default ' ',
  "AAAAVSIE" varchar(1) NOT NULL default ' ',
  "AAABBOEJ" varchar(1) NOT NULL default ' ',
  "AAAAVSIG" varchar(1) NOT NULL default ' ',
  "AAAAVSIP" varchar(1) NOT NULL default ' ',
  "AAAAVSIQ" varchar(1) NOT NULL default ' ',
  "AAAAVSIR" varchar(1) NOT NULL default ' ',
  "AAAAVSIS" varchar(1) NOT NULL default ' ',
  "AAAAVSIT" varchar(1) NOT NULL default ' ',
  "AAABIATM" varchar(1) NOT NULL default ' ',
  "AAABIATQ" varchar(1) NOT NULL default ' ',
  "AAABIATW" varchar(1) NOT NULL default ' ',
  "AAABIATX" varchar(1) NOT NULL default ' ',
  "AAABIATO" varchar(1) NOT NULL default ' ',
  "AAAAVSIZ" varchar(1) NOT NULL default ' ',
  "AAAAVSJA" varchar(1) NOT NULL default ' ',
  "AAABIATL" varchar(1) NOT NULL default ' ',
  "AAABIATP" varchar(1) NOT NULL default ' ',
  "AAABIATY" varchar(1) NOT NULL default ' ',
  "AAABIATZ" varchar(1) NOT NULL default ' ',
  "AAABIATN" varchar(1) NOT NULL default ' ',
  "AAAAVSJG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMQVT","AAABMQVU","AAABICIR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABLLJR" varchar(6) NOT NULL default '000000',
  "AAAAWCJG" varchar(20) NOT NULL default ' ',
  "AAABIASI" varchar(8) NOT NULL default '00000000',
  "AAAAHAMJ" varchar(1) NOT NULL default ' ',
  "AAAAHAMK" varchar(1) NOT NULL default ' ',
  "AAAAHAML" varchar(1) NOT NULL default ' ',
  "AAAAHAMM" varchar(1) NOT NULL default ' ',
  "AAABDSTC" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABLLJR","AAAAWCJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHAMY" int(11) NOT NULL default '0',
  "AAABLITM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMCVV","AAABGAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHANN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHANR" varchar(8) NOT NULL default '00000000',
  "AAAAHANS" varchar(8) NOT NULL default '00000000',
  "AAAAHAPW" varchar(2) NOT NULL default '00',
  "AAAAQGCV" varchar(2) NOT NULL default '00',
  "AAAAHANV" varchar(4) NOT NULL default '0000',
  "AAAAHANW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHAPW" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABJZCI" varchar(255) NOT NULL default ' ',
  "AAABKWQO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMNDU","AAABMREL","AAAAHAPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHAPW" varchar(2) NOT NULL default '00',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAAHAOU" varchar(2) NOT NULL default ' ',
  "AAAAHAOV" varchar(3) NOT NULL default ' ',
  "AAAAHAOW" varchar(3) NOT NULL default ' ',
  "AAABIYUA" varchar(3) NOT NULL default ' ',
  "AAAAHAOY" varchar(1) NOT NULL default '0',
  "AAAAHAOZ" varchar(3) NOT NULL default '000',
  "AAAAHBGV" int(11) NOT NULL default '0',
  "AAAAHBGW" int(11) NOT NULL default '0',
  "AAAAHBGX" int(11) NOT NULL default '0',
  "AAAAHAPD" varchar(10) NOT NULL default '0000000000',
  "AAAAHAPE" varchar(10) NOT NULL default '0000000000',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default ' ',
  "AAABLMFS" int(11) NOT NULL default '0',
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAAAHAPN" int(11) NOT NULL default '0',
  "AAAAHAPO" varchar(4) NOT NULL default ' ',
  "AAAAHAPP" varchar(4) NOT NULL default ' ',
  "AAAAHAPQ" varchar(255) NOT NULL default ' ',
  "AAAAHCAE" varchar(1) NOT NULL default ' ',
  "AAABFVIF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL","AAAAHAPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAAAWBNH" varchar(4) NOT NULL default ' ',
  "AAABLUSK" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAAAWBNH","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHAQL" varchar(1) NOT NULL default ' ',
  "AAAAHAQM" varchar(1) NOT NULL default ' ',
  "AAAAHAQN" int(11) NOT NULL default '0',
  "AAAAHCAP" int(11) NOT NULL default '0',
  "AAABGCXZ" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHAQQ" varchar(73) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAAAHAQS" varchar(8) NOT NULL default '00000000',
  "AAAAHAQT" varchar(8) NOT NULL default '00000000',
  "AAAAHAQU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(5) NOT NULL default '00000',
  "AAAAHARB" varchar(1) NOT NULL default ' ',
  "AAABIVNA" varchar(255) NOT NULL default ' ',
  "AAABIVNB" varchar(255) NOT NULL default ' ',
  "AAABIVNC" varchar(255) NOT NULL default ' ',
  "AAABIVND" varchar(255) NOT NULL default ' ',
  "AAABIVNE" varchar(255) NOT NULL default ' ',
  "AAAAHARH" varchar(255) NOT NULL default ' ',
  "AAAAHARI" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREL","AAABMLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHARM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
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
  "AAAAVTXW" varchar(22) NOT NULL default ' ',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAABIBFR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABKRXE","AAABLXTG","AAABMDBB","AAABKRXF","AAABKRXK","AAABKOTY","AAABMCXV","AAABJGDL","AAABMCRA","AAABMCPW","AAAAVTXW","AAABIBGA","AAABIBFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHASR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABLXTZ" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABLXTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHASY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAHATG" varchar(5) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAHATI" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHATK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABLXTG","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHATP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKRXO" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABLXTG","AAABMDBB","AAABKRXK","AAABKRXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAAAHAVJ" varchar(10) NOT NULL default ' ',
  "AAAAHAVK" varchar(3) NOT NULL default ' ',
  "AAAAHAVL" varchar(10) NOT NULL default ' ',
  "AAAAHAVM" varchar(10) NOT NULL default ' ',
  "AAAAHAVN" varchar(3) NOT NULL default ' ',
  "AAAAHAVO" varchar(10) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPGQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAWK" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDUGN" varchar(3) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  "AAAAHAWV" decimal(9,0) NOT NULL default '0',
  "AAAAHAWW" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDUGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDUGN" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABDUGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAXI" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABLRJL" varchar(1) NOT NULL default ' ',
  "AAABCXXG" varchar(30) NOT NULL default ' ',
  "AAABMKXX" varchar(30) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABIRTT" varchar(8) NOT NULL default ' ',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAAAHAXU" varchar(30) NOT NULL default ' ',
  "AAAAHAXV" varchar(1) NOT NULL default ' ',
  "AAAAHAXW" varchar(1) NOT NULL default ' ',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAABHPNG" varchar(30) NOT NULL default ' ',
  "AAAAHAXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMIZS","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMPSW" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  "AAAATQKH" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHAYL" varchar(5) NOT NULL default ' ',
  "AAAAHAYM" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHAZF" varchar(3) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABCNCA" varchar(2) NOT NULL default ' ',
  "AAABCNCB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHAZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAHAZF" varchar(3) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAAAHAZF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHAZL" varchar(255) NOT NULL default ' ',
  "AAAAHAZM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHAZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAWVUT" varchar(50) NOT NULL default ' ',
  "AAAAWVUU" varchar(50) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAAAHCFE","AAAAWVUT","AAAAWVUU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBAC" (
  "AAAAHBAF" varchar(3) NOT NULL default ' ',
  "AAABBDPM" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAHBAF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAAAHBBD" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHBAV" varchar(1) NOT NULL default ' ',
  "AAABMBIU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBAX" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMDBB","AAABLZBN","AAAAHBBD","AAABMCPV","AAABGAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDGA" varchar(20) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAHBFP" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFR" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFS" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFT" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFU" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFV" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFW" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMQCD","AAABMDGA","AAABMOAH","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAAAZWVX" varchar(1) NOT NULL default ' ',
  "AAAAHBDB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDC" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBEO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDF" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBEP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDH" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDI" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBEQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBDL" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBER" decimal(15,2) NOT NULL default '0.00',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMOHM","AAABMRCU","AAABMREX","AAABLZBN","AAABMCPV","AAABMQHP","AAABMDBB","AAABMPVB","AAAAZWVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBN" varchar(16) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAAAZWVX" varchar(1) NOT NULL default ' ',
  "AAAAHBEO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBEP" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBEQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHBER" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMOHM","AAABMQHV","AAABMRCU","AAABMREX","AAABLZBN","AAABMCPV","AAABMQHP","AAABMDBB","AAABMPVB","AAAAZWVX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMDGA" varchar(20) NOT NULL default ' ',
  "AAAAHBFP" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFR" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFS" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFT" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFU" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFV" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBFW" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABMPWP","AAABMQCD","AAABMDGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAHBGK" varchar(4) NOT NULL default ' ',
  "AAAAHBGL" varchar(10) NOT NULL default ' ',
  "AAAAHBGM" varchar(4) NOT NULL default '0000',
  "AAAAHBGN" varchar(4) NOT NULL default ' ',
  "AAAAHBGO" varchar(10) NOT NULL default ' ',
  "AAAAHBGP" varchar(4) NOT NULL default '0000',
  "AAAAHBGQ" varchar(4) NOT NULL default ' ',
  "AAAAHBGR" varchar(10) NOT NULL default ' ',
  "AAAAHBGS" varchar(4) NOT NULL default '0000',
  "AAAAHBGT" varchar(1) NOT NULL default ' ',
  "AAAAHBGU" varchar(1) NOT NULL default ' ',
  "AAAAHBGV" int(11) NOT NULL default '0',
  "AAAAHBGW" int(11) NOT NULL default '0',
  "AAAAHBGX" int(11) NOT NULL default '0',
  "AAAAHBGY" varchar(6) NOT NULL default '000000',
  "AAAAHBGZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAABMCPM" varchar(8) NOT NULL default '00000000',
  "AAAAHBJG" varchar(10) NOT NULL default '0000000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABJWRV" varchar(16) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCXM" varchar(8) NOT NULL default '00000000',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRO" varchar(10) NOT NULL default ' ',
  "AAABMCRP" varchar(2) NOT NULL default '00',
  "AAABMCRQ" varchar(4) NOT NULL default '0000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAABMCSH" varchar(1) NOT NULL default ' ',
  "AAABMCQD" varchar(5) NOT NULL default ' ',
  "AAABMCQC" decimal(13,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXW" decimal(13,2) NOT NULL default '0.00',
  "AAABMCXX" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBIV" varchar(10) NOT NULL default ' ',
  "AAAAHBIW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMRCU","AAABMREX","AAABMPWP","AAABLZBN","AAABMCPO","AAABMCPM","AAAAHBJG","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRA" varchar(6) NOT NULL default ' ',
  "AAABMCPR" varchar(1) NOT NULL default ' ',
  "AAABMCPS" varchar(1) NOT NULL default ' ',
  "AAABMCQD" varchar(5) NOT NULL default ' ',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMCQC" decimal(15,2) NOT NULL default '0.00',
  "AAABMCPZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMCXW" decimal(15,2) NOT NULL default '0.00',
  "AAABMCXX" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMPWP","AAABLZBN","AAABMOAH","AAABMCPV","AAABMCRA","AAABMCPR","AAABMCPS","AAABMCQD","AAABMPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBKL" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAABMNJF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAHCDO","AAAAHCDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBKV" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAAANAKS" varchar(4) NOT NULL default '0000',
  "AAAAHCCW" varchar(1) NOT NULL default ' ',
  "AAAAHCCX" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAHCDO","AAAAHCDP","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBLI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAVPSW" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABLZOZ" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAVPSW","AAABMREL","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBLX" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAVPSW" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAANAKS" varchar(4) NOT NULL default '0000',
  "AAAAHCCW" varchar(1) NOT NULL default ' ',
  "AAAAHCCX" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAVPSW","AAABMREL","AAABMIZS","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBMM" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAVPSW" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAANAKS" varchar(4) NOT NULL default '0000',
  "AAAAHCCW" varchar(1) NOT NULL default ' ',
  "AAAAHCCX" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAVPSW","AAABMREL","AAABMIZS","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBNB" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAVPSW" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABLZOZ" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAAAVPSW","AAABMREL","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABKYSQ" varchar(5) NOT NULL default ' ',
  "AAAATQKH" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABCCXV" varchar(8) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABCCXV","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHBOQ" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default ' ',
  "AAAAHBON" varchar(1) NOT NULL default ' ',
  "AAAAHBOO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHBOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABBPGQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAHBPU" varchar(10) NOT NULL default ' ',
  "AAAAHBPV" varchar(10) NOT NULL default ' ',
  "AAAAHBPW" varchar(3) NOT NULL default ' ',
  "AAAAHBPX" varchar(3) NOT NULL default ' ',
  "AAAAHBPY" varchar(3) NOT NULL default ' ',
  "AAAAHBPZ" varchar(3) NOT NULL default ' ',
  "AAAAHBQA" varchar(3) NOT NULL default ' ',
  "AAAAMFIO" varchar(1) NOT NULL default ' ',
  "AAAAHBQC" varchar(1) NOT NULL default ' ',
  "AAAAHBQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCAP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABEHHR" varchar(2) NOT NULL default '00',
  "AAAAHBRR" varchar(4) NOT NULL default '0000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMMJT" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBRA" decimal(13,2) NOT NULL default '0.00',
  "AAAAHBRB" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCQQ" varchar(3) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMOAH","AAABMCPV","AAABMDGC","AAABGAJY","AAABEHHR","AAAAHBRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAABEMZP" varchar(50) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHBSY" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAAAHCFE","AAABEMZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAHBTD" varchar(50) NOT NULL default ' ',
  "AAAAHBTE" varchar(50) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAAAHCFE","AAAAHBTD","AAAAHBTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBSQ" (
  "AAAAHBTA" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAHBTD" varchar(50) NOT NULL default ' ',
  "AAAAHBTE" varchar(50) NOT NULL default ' ',
  "AAAAHBTF" varchar(50) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHBSY" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAHBTA","AAABDABU","AAAAHCFE","AAAAHBTD","AAAAHBTE","AAAAHBTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBTG" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMONG","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBTL" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAVMEY" varchar(4) NOT NULL default ' ',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAHCAY" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAAAHBTT" varchar(1) NOT NULL default ' ',
  "AAAAHBTU" varchar(5) NOT NULL default ' ',
  "AAAAHBTV" varchar(3) NOT NULL default '000',
  "AAAAHBTW" varchar(4) NOT NULL default '0000',
  "AAAAHBTX" varchar(1) NOT NULL default ' ',
  "AAAAHBTY" varchar(1) NOT NULL default ' ',
  "AAAAHBTZ" varchar(1) NOT NULL default ' ',
  "AAAAHBUA" varchar(1) NOT NULL default ' ',
  "AAAAHBUB" varchar(1) NOT NULL default ' ',
  "AAAAHBUC" varchar(1) NOT NULL default ' ',
  "AAAAHBUD" varchar(1) NOT NULL default ' ',
  "AAAAHBUE" varchar(10) NOT NULL default ' ',
  "AAAAHBUF" varchar(10) NOT NULL default ' ',
  "AAAAHBUG" varchar(10) NOT NULL default ' ',
  "AAAAHBUH" varchar(10) NOT NULL default ' ',
  "AAAAHBUI" varchar(10) NOT NULL default ' ',
  "AAAAHBUJ" varchar(10) NOT NULL default ' ',
  "AAAAHBUK" varchar(10) NOT NULL default ' ',
  "AAAAHBUL" varchar(10) NOT NULL default ' ',
  "AAAAHBUM" varchar(10) NOT NULL default ' ',
  "AAAAHBUN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAAAHBVB" varchar(50) NOT NULL default ' ',
  "AAAALBQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABLRST","AAABMKFH","AAAAHBVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBVC" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABIMUR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAAKMDP" varchar(40) NOT NULL default ' ',
  "AAAAHBVQ" varchar(1) NOT NULL default ' ',
  "AAABHUHC" varchar(1) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAAAHBWC" varchar(1) NOT NULL default ' ',
  "AAABMLES" varchar(6) NOT NULL default ' ',
  "AAABMLPV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(10) NOT NULL default ' ',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAAAKMDP" varchar(40) NOT NULL default ' ',
  "AAABMRFO" varchar(3) NOT NULL default ' ',
  "AAAALOLS" varchar(8) NOT NULL default '00000000',
  "AAAAHBXA" varchar(6) NOT NULL default ' ',
  "AAAAHBXB" varchar(8) NOT NULL default '00000000',
  "AAAAHBXC" varchar(6) NOT NULL default ' ',
  "AAABKDLP" varchar(40) NOT NULL default ' ',
  "AAAAHBXE" varchar(255) NOT NULL default ' ',
  "AAABMQIX" int(11) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABHFAO","AAABMNDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCAP" int(11) NOT NULL default '0',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAHBXT" int(11) NOT NULL default '0',
  "AAAAHBXU" int(11) NOT NULL default '0',
  "AAABMQIX" int(11) NOT NULL default '0',
  "AAAAHCAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMNDU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMMYW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMMYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBYI" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABIMUR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABMRHS","AAABDABU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAVZFC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREX","AAABMCSO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMIZS" varchar(30) NOT NULL default ' ',
  "AAAAHBZG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL","AAABMIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAAAHCFE","AAABMQHV","AAABMREX","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHBZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMNDU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAHCAP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCAY" varchar(1) NOT NULL default ' ',
  "AAAAHCAZ" varchar(1) NOT NULL default ' ',
  "AAAAHCBA" varchar(1) NOT NULL default ' ',
  "AAAAHCBB" varchar(1) NOT NULL default ' ',
  "AAAAHCBC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCBG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABERBG" varchar(4) NOT NULL default ' ',
  "AAABKWVR" decimal(6,2) NOT NULL default '0.00',
  "AAAAHCBK" int(11) NOT NULL default '0',
  "AAAAHCBL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL","AAABERBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCBO" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAABMNJF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAABMREL","AAAAHCDO","AAAAHCDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCCA" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAAANAKS" varchar(4) NOT NULL default '0000',
  "AAAAHCCW" varchar(1) NOT NULL default ' ',
  "AAAAHCCX" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAABMREL","AAAAHCDO","AAAAHCDP","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCCP" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAAANAKS" varchar(4) NOT NULL default '0000',
  "AAAAHCCW" varchar(1) NOT NULL default ' ',
  "AAAAHCCX" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAABMREL","AAAAHCDO","AAAAHCDP","AAAANAKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCDE" (
  "AAABMONG" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAHCDO" varchar(2) NOT NULL default ' ',
  "AAAAHCDP" varchar(2) NOT NULL default '00',
  "AAABMNJF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMONG","AAABDABU","AAABMREL","AAAAHCDO","AAAAHCDP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAABMQHV","AAABMREX","AAABETQU","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCEJ" (
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAHCEL" varchar(30) NOT NULL default ' ',
  "AAAAHCEM" varchar(30) NOT NULL default ' ',
  "AAAAHCEN" varchar(30) NOT NULL default ' ',
  "AAAAHCEO" varchar(30) NOT NULL default ' ',
  "AAAAHCEP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCER" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAAAHCEU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDABU" varchar(3) NOT NULL default '000',
  "AAAAHCFE" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDABU","AAAAHCFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABFOBW" varchar(30) NOT NULL default ' ',
  "AAABETRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCFM" (
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGARW" int(11) NOT NULL default '0',
  "AAAAHCIJ" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAAAHCHI" varchar(1) NOT NULL default ' ',
  "AAAAHCHJ" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAABLENR" varchar(2) NOT NULL default ' ',
  "AAABGAQM" varchar(2) NOT NULL default ' ',
  "AAABGAQN" varchar(2) NOT NULL default ' ',
  "AAABGAQO" varchar(2) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIYCS","AAABMIRX","AAABMIRY","AAABGARW","AAAAHCIJ","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCGO" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAABGARW" int(11) NOT NULL default '0',
  "AAAAHCIJ" varchar(3) NOT NULL default '000',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAABIYCS","AAABMIRX","AAABMIRY","AAABGARW","AAAAHCIJ","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCHE" (
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAAAHCIJ" varchar(3) NOT NULL default '000',
  "AAAAHCHI" varchar(1) NOT NULL default ' ',
  "AAAAHCHJ" varchar(1) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAABLENR" varchar(2) NOT NULL default ' ',
  "AAABGAQM" varchar(2) NOT NULL default ' ',
  "AAABGAQN" varchar(2) NOT NULL default ' ',
  "AAABGAQO" varchar(2) NOT NULL default ' ',
  "AAABLNYT" varchar(12) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABHZPG" varchar(12) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMIRX","AAABMIRY","AAAAHCIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCIA" (
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(1) NOT NULL default ' ',
  "AAAAHCIJ" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFZK","AAABMIRX","AAABMIRY","AAAAHCIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCIK" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCIN" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUGRQ" VARBINARY(255) NOT NULL,
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABDVPT" varchar(1) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABDNUF" varchar(1) NOT NULL default ' ',
  "AAABDNUG" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABDNYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUGRQ"(255),"AAABMBXY"(255),"AAABDVPT","AAABMKFU","AAABDNUF","AAABDNUG","AAABLZKF","AAABDNYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCJG" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAUGRQ" VARBINARY(255) NOT NULL,
  "AAABMBXY" VARBINARY(255) NOT NULL,
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABDNUF" varchar(1) NOT NULL default ' ',
  "AAABDNUG" varchar(1) NOT NULL default ' ',
  "AAABDNYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAUGRQ"(255),"AAABMBXY"(255),"AAABKRQD","AAABDNUF","AAABDNUG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCJU" (
  "AAABMJXI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMJXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCJX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMJXI" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMJXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCKD" (
  "AAABIMRZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCKG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIMRZ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIMRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCKM" (
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAABLUVJ" varchar(32) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABIISW" varchar(10) NOT NULL default ' ',
  "AAABIYQL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIITH","AAABIITI","AAABLUVJ","AAABLENH","AAABLMNC","AAABIISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCLA" (
  "AAABJAAI" varchar(30) NOT NULL default ' ',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJAAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABLXYU","AAABMNZR","AAABMCJZ","AAABLYAS","AAABLXYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHCLV" varchar(1) NOT NULL default ' ',
  "AAAAHCMJ" varchar(1) NOT NULL default ' ',
  "AAAAHCMI" varchar(1) NOT NULL default ' ',
  "AAAAHCMK" varchar(1) NOT NULL default ' ',
  "AAAAHCLZ" varchar(1) NOT NULL default ' ',
  "AAAAHCMA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAHCMI" varchar(1) NOT NULL default ' ',
  "AAAAHCMJ" varchar(1) NOT NULL default ' ',
  "AAAAHCMK" varchar(1) NOT NULL default ' ',
  "AAAAHCML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFZRW" varchar(30) NOT NULL default ' ',
  "AAAAMWPL" varchar(30) NOT NULL default ' ',
  "AAABILRT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABFZRW","AAAAMWPL","AAABILRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCMZ" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJARY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF","AAABMREL","AAABJARY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCNG" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFZRW" varchar(10) NOT NULL default ' ',
  "AAAAMWPL" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABFZRW","AAAAMWPL","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFZRW" varchar(10) NOT NULL default ' ',
  "AAAAMWPL" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(40) NOT NULL default ' ',
  "AAAAUQQT" varchar(20) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABFZRW","AAAAMWPL","AAABMMJT"),
  KEY "AAAAHCOC" ("AAABLZKF","AAABMRFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHCOM" varchar(32) NOT NULL default ' ',
  "AAABJOTS" varchar(4) NOT NULL default ' ',
  "AAABBRJN" varchar(12) NOT NULL default ' ',
  "AAAAHCOQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHCOM"),
  KEY "AAAAHCON" ("AAABJOTS","AAABBRJN","AAAAHCOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHCRC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAAAHCPH" varchar(1) NOT NULL default ' ',
  "AAABLPSK" varchar(2) NOT NULL default ' ',
  "AAAAHCPJ" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABCPQI" varchar(1) NOT NULL default ' ',
  "AAABMDFT" varchar(10) NOT NULL default ' ',
  "AAABMGXU" varchar(4) NOT NULL default ' ',
  "AAABMDFU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBA","AAABMPWO","AAABMQCA","AAABMQCB"),
  KEY "AAAAHCPV" ("AAABMRIL","AAABMQCC","AAABMQCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHCRC" varchar(4) NOT NULL default ' ',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAAAQNMW" varchar(8) NOT NULL default '00000000',
  "AAAAHCQE" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  "AAAAHCQH" varchar(1) NOT NULL default ' ',
  "AAAAHCQI" varchar(1) NOT NULL default ' ',
  "AAAAHCQJ" varchar(1) NOT NULL default ' ',
  "AAAAHCQK" varchar(1) NOT NULL default ' ',
  "AAAAHCQL" varchar(1) NOT NULL default ' ',
  "AAAAHCQM" varchar(1) NOT NULL default ' ',
  "AAAAHCQN" varchar(1) NOT NULL default ' ',
  "AAAAHCQO" varchar(1) NOT NULL default ' ',
  "AAAAHCQP" varchar(1) NOT NULL default ' ',
  "AAAAHCQQ" varchar(11) NOT NULL default ' ',
  "AAAAHCQR" varchar(1) NOT NULL default ' ',
  "AAAAHCQS" varchar(1) NOT NULL default ' ',
  "AAAAHCQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHCRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHCRC" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHCRC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAHCRI" varchar(1) NOT NULL default ' ',
  "AAAARMUT" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCRN" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAAAIOLH" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQBS","AAABJKTY","AAABMQHP","AAABMQHQ","AAABMQCD"),
  UNIQUE KEY "AAAAHCSB" ("AAAAIOLH","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCSE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAAAIOLI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQDU"),
  UNIQUE KEY "AAAAHCSQ" ("AAAAIOLI","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCST" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAHDEU" varchar(10) NOT NULL default ' ',
  "AAAAIOLJ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHZ","AAAAHDEU"),
  UNIQUE KEY "AAAAHCTB" ("AAAAIOLJ","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCTE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAAAIOLK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQDO","AAABBTTF","AAABMCWT","AAABIBHM","AAABIBNO","AAABMCRJ","AAABLRPK"),
  UNIQUE KEY "AAAAHCTW" ("AAAAIOLK","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCTZ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAAAIOLL" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU"),
  UNIQUE KEY "AAAAHCUD" ("AAAAIOLL","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCUG" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAAAIOLM" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU"),
  UNIQUE KEY "AAAAHCUK" ("AAAAIOLM","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCUN" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABJKXT" varchar(10) NOT NULL default ' ',
  "AAAAIOLN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAAAITNS","AAABJKUI","AAABIBHQ","AAABIBHP","AAABBPPK","AAABJKXT"),
  UNIQUE KEY "AAAAHCVD" ("AAAAIOLN","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCVG" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAAAIOLO" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU"),
  UNIQUE KEY "AAAAHCVK" ("AAAAIOLO","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCVN" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAAAIOLP" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU"),
  UNIQUE KEY "AAAAHCVR" ("AAAAIOLP","AAABMQHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCVU" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAHDEU" varchar(10) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABJKXT" varchar(10) NOT NULL default ' ',
  "AAABMPVO" decimal(23,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(23,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(23,2) NOT NULL default '0.00',
  "AAAAINYW" decimal(23,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(23,3) NOT NULL default '0.000',
  "AAAAIVXA" decimal(23,2) NOT NULL default '0.00',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  "AAAAIVXB" varchar(5) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABBQDY" varchar(5) NOT NULL default ' ',
  "AAAAIVWZ" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHY","AAABMQCJ","AAABMQHV","AAABMQDO","AAABMQCO"),
  UNIQUE KEY "AAAAHCXU" ("AAABMQHU","AAABMDBA","AAABMQDO","AAABMDBB","AAABMPWO","AAABMQHV","AAABMQCO"),
  KEY "AAAAHCYC" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ","AAABMPVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHCYI" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQCO" varchar(6) NOT NULL default ' ',
  "AAABMQCL" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAHDEU" varchar(10) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABJKXT" varchar(10) NOT NULL default ' ',
  "AAABMQDX" decimal(23,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(23,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(23,2) NOT NULL default '0.00',
  "AAABMQED" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQER" decimal(23,2) NOT NULL default '0.00',
  "AAABMQES" decimal(23,2) NOT NULL default '0.00',
  "AAABMQET" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJV" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJW" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJX" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJY" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJZ" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKB" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKC" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKD" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKE" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKF" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKG" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKH" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKI" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKJ" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKK" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(23,3) NOT NULL default '0.000',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMQBU" varchar(8) NOT NULL default '00000000',
  "AAABMQCV" varchar(10) NOT NULL default ' ',
  "AAABMQDA" varchar(6) NOT NULL default '000000',
  "AAABMQCX" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHY","AAABMQCJ","AAABMQHV","AAABMQDO","AAABMQCO","AAABMQCL","AAABMQHS"),
  KEY "AAAAHDDK" ("AAABMQCV","AAABMQHY","AAABMQCX","AAABMQDO","AAABMQHV","AAABMQDA"),
  KEY "AAAAHDDR" ("AAABMQHV","AAABMQDO","AAABMQHY","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDDW" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAAAIOLH" int(11) NOT NULL default '0',
  "AAAAIOLI" int(11) NOT NULL default '0',
  "AAAAIOLJ" int(11) NOT NULL default '0',
  "AAAAIOLK" int(11) NOT NULL default '0',
  "AAAAIOLL" int(11) NOT NULL default '0',
  "AAAAIOLM" int(11) NOT NULL default '0',
  "AAAAIOLN" int(11) NOT NULL default '0',
  "AAAAIOLO" int(11) NOT NULL default '0',
  "AAAAIOLP" int(11) NOT NULL default '0',
  "AAABMQHR" varchar(1) NOT NULL default ' ',
  "AAABMQHS" varchar(3) NOT NULL default '000',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMQHP" varchar(5) NOT NULL default ' ',
  "AAABMQHQ" varchar(3) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAAAHDEU" varchar(10) NOT NULL default ' ',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABBTTF" varchar(10) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABIBHM" varchar(16) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAAAITNS" varchar(10) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABIBHQ" varchar(16) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABJKXT" varchar(10) NOT NULL default ' ',
  "AAABMQDX" decimal(23,2) NOT NULL default '0.00',
  "AAABMQDY" decimal(23,2) NOT NULL default '0.00',
  "AAABMQDZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEA" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEB" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEC" decimal(23,2) NOT NULL default '0.00',
  "AAABMQED" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEE" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEF" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEG" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEH" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEI" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEK" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEM" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEN" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEO" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEP" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEQ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQER" decimal(23,2) NOT NULL default '0.00',
  "AAABMQES" decimal(23,2) NOT NULL default '0.00',
  "AAABMQET" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEU" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEV" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEW" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEX" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEY" decimal(23,2) NOT NULL default '0.00',
  "AAABMQEZ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFA" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFB" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFC" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFD" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFE" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFF" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFG" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFH" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFI" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFJ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFK" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFM" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFN" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFO" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFP" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFQ" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFR" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFS" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFT" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFU" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFV" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJV" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJW" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJX" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJY" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOJZ" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKA" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKB" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKC" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKD" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKE" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKF" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKG" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKH" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKI" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKJ" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKK" decimal(23,2) NOT NULL default '0.00',
  "AAAAIOKL" decimal(23,2) NOT NULL default '0.00',
  "AAABMQFW" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFX" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFY" decimal(23,3) NOT NULL default '0.000',
  "AAABMQFZ" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGA" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGB" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGC" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGD" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGE" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGF" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGG" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGH" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGI" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGJ" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGK" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGL" decimal(23,3) NOT NULL default '0.000',
  "AAABMQGM" decimal(23,3) NOT NULL default '0.000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMQHU","AAABMQHY","AAAAIOLH","AAAAIOLI","AAAAIOLJ","AAAAIOLK","AAAAIOLL","AAAAIOLM","AAAAIOLN","AAAAIOLO","AAAAIOLP","AAABMQHR","AAABMQHS"),
  KEY "AAAAHDJD" ("AAABMQHV","AAABMQDO","AAABMQHZ"),
  KEY "AAAAHDJH" ("AAABMQHV","AAABMQDO","AAABMCWT"),
  KEY "AAAAHDJL" ("AAABMQHV","AAABMQDO","AAABBTTF"),
  KEY "AAAAHDJP" ("AAABMQHV","AAABLRPK","AAABMQDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAAAHDKC" varchar(16) NOT NULL default ' ',
  "AAAAHDJY" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAAAHEAT","AAAAHDKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAABLUSV" varchar(6) NOT NULL default '000000',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABLTQF" varchar(6) NOT NULL default '000000',
  "AAABLTQG" varchar(6) NOT NULL default '000000',
  "AAAAHDKL" varchar(6) NOT NULL default '000000',
  "AAABKVPX" varchar(2) NOT NULL default '00',
  "AAABGFUB" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABBMWL" varchar(1) NOT NULL default ' ',
  "AAABMCMG" varchar(1) NOT NULL default '0',
  "AAAAHDLB" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAABMRHS","AAAAHEAT","AAABBMWL","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABBNEI" varchar(2) NOT NULL default ' ',
  "AAAAHDLN" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAAAHEAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAABMRHS","AAAAHEAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAHDMK" varchar(10) NOT NULL default ' ',
  "AAAAHDMH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAAAHDMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAAAHDMZ" varchar(10) NOT NULL default ' ',
  "AAAAHDMR" varchar(10) NOT NULL default ' ',
  "AAAAHDMS" varchar(1) NOT NULL default ' ',
  "AAAAHDMT" varchar(1) NOT NULL default ' ',
  "AAABCDET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXK","AAAAHEAT","AAABLZBM","AAAAHDMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPEV" varchar(20) NOT NULL default ' ',
  "AAABEPEY" varchar(20) NOT NULL default ' ',
  "AAAAHDNF" varchar(20) NOT NULL default ' ',
  "AAAAHDNG" varchar(20) NOT NULL default ' ',
  "AAAAHDNH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAAAHDNN" varchar(1) NOT NULL default ' ',
  "AAAAHDNO" varchar(1) NOT NULL default ' ',
  "AAABBQBY" varchar(10) NOT NULL default ' ',
  "AAAAHDNQ" varchar(8) NOT NULL default '00000000',
  "AAAAHDNR" varchar(8) NOT NULL default '00000000',
  "AAAAHEDK" varchar(1) NOT NULL default ' ',
  "AAAAHEDI" varchar(1) NOT NULL default ' ',
  "AAAAHDNU" varchar(1) NOT NULL default ' ',
  "AAAAHDNV" varchar(1) NOT NULL default ' ',
  "AAAAHDNW" varchar(8) NOT NULL default '00000000',
  "AAAAHDNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDNZ" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDOC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDOI" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQHV","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABHMRN" varchar(2) NOT NULL default '00',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAUENE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHDQM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABGAJY","AAABCBUI","AAABBACP","AAABMRFQ","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(3) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABHMRN" varchar(2) NOT NULL default '00',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAAAUENE" decimal(15,2) NOT NULL default '0.00',
  "AAAAHDQM" varchar(2) NOT NULL default '00',
  "AAAAHDQN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMREX","AAABMCSO","AAABGAJY","AAABCBUI","AAABBACP","AAABMRFQ","AAABMPWL","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDO" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEXIH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMQDO","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBJIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDRK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQHV","AAABERMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHV","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDSC" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAAAHDSH" varchar(30) NOT NULL default ' ',
  "AAAAHDSF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAAAHDSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDSI" (
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default '00',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABLUEU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABERMO","AAABERMQ","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDSS" (
  "AAABERMO" varchar(30) NOT NULL default ' ',
  "AAABERMQ" varchar(30) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default '00',
  "AAABERMP" varchar(30) NOT NULL default ' ',
  "AAABERMM" varchar(30) NOT NULL default ' ',
  "AAABLUEU" varchar(3) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAAAHDTC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABERMO","AAABERMQ","AAABMMYV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDTG" (
  "AAAAHDTJ" varchar(30) NOT NULL default ' ',
  "AAABBJIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHDTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDTK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLUXQ","AAABMQHV","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDTU" (
  "AAABLECD" varchar(2) NOT NULL default ' ',
  "AAABIASH" varchar(3) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABIASH","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  "AAABLUSV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDUH" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDUK" (
  "AAABLILW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDUN" (
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDUT" (
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMMYV" varchar(2) NOT NULL default '00',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLILW","AAABMMYV","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLRPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAHDYO" varchar(6) NOT NULL default '000000',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABLRPK" varchar(10) NOT NULL default ' ',
  "AAABMCPW" varchar(4) NOT NULL default ' ',
  "AAABJKUI" varchar(10) NOT NULL default ' ',
  "AAABJKXT" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABMCXF" varchar(8) NOT NULL default '00000000',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABLXSX" varchar(8) NOT NULL default '00000000',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMCRT" varchar(3) NOT NULL default ' ',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAABMPLS" varchar(1) NOT NULL default ' ',
  "AAABMPSC" varchar(2) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABMCSI" varchar(1) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMCQD" varchar(5) NOT NULL default ' ',
  "AAABMCQC" decimal(13,2) NOT NULL default '0.00',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAABMDBB","AAABMREX","AAABMPWO","AAAAHDYO"),
  KEY "AAAAHDXT" ("AAABKJVG","AAABMQCD","AAABMCOP","AAABMQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAHDYO" varchar(6) NOT NULL default '000000',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  "AAABIEIT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAABMDBB","AAABMREX","AAABMPWO","AAAAHDYO","AAABGAJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHDYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDYW" (
  "AAAAHDYZ" varchar(1) NOT NULL default '0',
  "AAAAHDYY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHDYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAHDZP" varchar(1) NOT NULL default ' ',
  "AAABBQEG" varchar(30) NOT NULL default ' ',
  "AAAAHDZF" varchar(1) NOT NULL default ' ',
  "AAAAHDZQ" varchar(1) NOT NULL default ' ',
  "AAAAHDZH" varchar(1) NOT NULL default ' ',
  "AAAAHDZI" varchar(1) NOT NULL default ' ',
  "AAAAHDZJ" varchar(1) NOT NULL default ' ',
  "AAAAHDZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDZN" (
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAHDZP" varchar(1) NOT NULL default ' ',
  "AAAAHDZQ" varchar(1) NOT NULL default ' ',
  "AAABBQEG" varchar(30) NOT NULL default ' ',
  "AAAAHDZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDZU" (
  "AAABIASH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABIASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHDZX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIASH" varchar(3) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABCBUI" varchar(2) NOT NULL default ' ',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  "AAAAHEAT" varchar(10) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHEAK" varchar(4) NOT NULL default ' ',
  "AAAAHEAL" varchar(4) NOT NULL default ' ',
  "AAAAHEAM" varchar(10) NOT NULL default ' ',
  "AAAAHEAN" varchar(10) NOT NULL default ' ',
  "AAAAHEAO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABCBUI","AAABKRXK","AAAAHEAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABBPHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFVNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABERIM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVNI","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHECD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABGZVC" decimal(23,2) NOT NULL default '0.00',
  "AAABGZVD" decimal(23,2) NOT NULL default '0.00',
  "AAABGZVE" decimal(8,0) NOT NULL default '0',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABGZVG" decimal(8,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABMQHV","AAABMREX","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHECU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABKQUP" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMLTD" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMLCZ","AAABKQUP","AAABMCRM","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHEDI" varchar(1) NOT NULL default ' ',
  "AAAAHEDJ" varchar(1) NOT NULL default ' ',
  "AAAAHEDK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABLPVQ" varchar(30) NOT NULL default ' ',
  "AAAAHEDR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABLPVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEDV" (
  "AAAAHEFQ" varchar(32) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABHCDU" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAHEEC" varchar(32) NOT NULL default ' ',
  "AAABGHLL" varchar(8) NOT NULL default '00000000',
  "AAAAHEEE" varchar(6) NOT NULL default '000000',
  "AAABGHLM" varchar(8) NOT NULL default '00000000',
  "AAAALYGU" varchar(6) NOT NULL default '000000',
  "AAABLESX" varchar(32) NOT NULL default ' ',
  "AAABGBZZ" varchar(32) NOT NULL default ' ',
  "AAAAHEEJ" varchar(20) NOT NULL default ' ',
  "AAAAHEEK" varchar(8) NOT NULL default '00000000',
  "AAAAHEEL" varchar(6) NOT NULL default '000000',
  "AAAAHEEM" varchar(8) NOT NULL default '00000000',
  "AAAAHEEN" varchar(6) NOT NULL default '000000',
  "AAAAHEEO" varchar(1) NOT NULL default ' ',
  "AAAAHEEP" varchar(1) NOT NULL default ' ',
  "AAAAHEEQ" varchar(1) NOT NULL default ' ',
  "AAAAHEER" varchar(8) NOT NULL default '00000000',
  "AAAAHEES" varchar(6) NOT NULL default '000000',
  "AAAAHEET" varchar(8) NOT NULL default '00000000',
  "AAAAHEEU" varchar(6) NOT NULL default '000000',
  "AAAAHEEV" varchar(8) NOT NULL default '00000000',
  "AAAAHEEW" varchar(6) NOT NULL default '000000',
  "AAAAHEEX" varchar(8) NOT NULL default '00000000',
  "AAAAHEEY" varchar(6) NOT NULL default '000000',
  "AAAAHEEZ" varchar(1) NOT NULL default ' ',
  "AAAAHEFA" varchar(1) NOT NULL default ' ',
  "AAABENBS" varchar(1) NOT NULL default ' ',
  "AAAAHEFC" varchar(1) NOT NULL default ' ',
  "AAABHDHE" varchar(1) NOT NULL default ' ',
  "AAAAHEFE" varchar(1) NOT NULL default ' ',
  "AAAAHEFF" varchar(1) NOT NULL default ' ',
  "AAABLUTZ" varchar(1) NOT NULL default ' ',
  "AAAAHEFH" varchar(1) NOT NULL default ' ',
  "AAAAHEFI" varchar(2) NOT NULL default '00',
  "AAAAHEFJ" varchar(2) NOT NULL default '00',
  "AAAAHEFK" varchar(3) NOT NULL default '000',
  "AAAAHEFL" varchar(2) NOT NULL default '00',
  "AAAAHEFM" varchar(2) NOT NULL default '00',
  "AAAAVLST" varchar(40) NOT NULL default ' ',
  "AAAAHEFO" varchar(128) NOT NULL default ' ',
  "AAABCRUO" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHEFQ","AAAAZOEQ"),
  KEY "AAAAHEFS" ("AAAAZOEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEFU" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
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
  "AAAAHEQC" varchar(8) NOT NULL default '00000000',
  "AAABLQCR" varchar(30) NOT NULL default ' ',
  "AAAALVVO" varchar(18) NOT NULL default ' ',
  "AAABAHTV" varchar(3) NOT NULL default '000',
  "AAABJFLS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHEHK" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABLMLH","AAABETQU","AAAAVUNB","AAABMQHV","AAABMQCO"),
  KEY "AAAAHEHT" ("AAABMQHV","AAABETQU","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAHEHZ" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQDA"),
  KEY "AAAAHEII" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHEIR" ("AAABMRIL","AAABMQDU","AAABLMLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABEZHC" varchar(6) NOT NULL default ' ',
  "AAAAIPKZ" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAAAIPMH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHEJI" ("AAABGPZS","AAABEZHC","AAAAIPKZ","AAABMRIL","AAAAIPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEJO" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAHELP" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQCO","AAABMQHS"),
  KEY "AAAAHELZ" ("AAABMQHV","AAABETQU","AAABMQHY","AAABMQHZ"),
  KEY "AAAAHEME" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQDA"),
  KEY "AAAAHEMN" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEMP" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAHEOK" ("AAABMQHV","AAABETQU","AAABMQHZ"),
  KEY "AAAAHEOO" ("AAABMQHV","AAABETQU"),
  KEY "AAAAHEOR" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEOT" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
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
  "AAAAHEQC" varchar(8) NOT NULL default '00000000',
  "AAABLQCR" varchar(30) NOT NULL default ' ',
  "AAAALVVO" varchar(18) NOT NULL default ' ',
  "AAABAHTV" varchar(3) NOT NULL default '000',
  "AAABJFLS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHEQJ" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABLMLH","AAABETQU","AAAAVUNB","AAABMQHV","AAABMQCO"),
  KEY "AAAAHEQS" ("AAABMQHV","AAABETQU","AAABMQHY","AAABMQHZ","AAABMPVB"),
  KEY "AAAAHEQY" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQDA"),
  KEY "AAAAHERH" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHERJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHERQ" ("AAABMRIL","AAABMQDU","AAABLMLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHERU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABEZHC" varchar(6) NOT NULL default ' ',
  "AAAAIPKZ" varchar(4) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAAAIPMH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHESH" ("AAABGPZS","AAAAIPMH","AAABEZHC","AAAAIPKZ","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHESN" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
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
  UNIQUE KEY "AAAAHEUO" ("AAABMQHU","AAABMQCJ","AAABMQHY","AAABMQCL","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQCO","AAABMQHS"),
  KEY "AAAAHEUY" ("AAABMQHV","AAABETQU","AAABMQHY","AAABMQHZ"),
  KEY "AAAAHEVD" ("AAABMQHU","AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQDA"),
  KEY "AAAAHEVM" ("AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEVO" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
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
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHR","AAABMQHS"),
  KEY "AAAAHEXJ" ("AAABMQHV","AAABMQHX","AAABMQHY","AAABETQU","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEXP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAAAHEXS" varchar(1) NOT NULL default ' ',
  "AAAAHEXT" varchar(1) NOT NULL default ' ',
  "AAAAOJNP" varchar(2) NOT NULL default ' ',
  "AAAAHEXV" varchar(1) NOT NULL default ' ',
  "AAABEPVK" varchar(1) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAAAHEXY" varchar(2) NOT NULL default ' ',
  "AAAAHEXZ" varchar(4) NOT NULL default ' ',
  "AAAAHEYA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEYD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABGBXV" varchar(32) NOT NULL default ' ',
  "AAAAHEYJ" varchar(1) NOT NULL default ' ',
  "AAAAHEYK" varchar(1) NOT NULL default ' ',
  "AAAAHEYL" varchar(1) NOT NULL default ' ',
  "AAAAOJNP" varchar(2) NOT NULL default ' ',
  "AAAAHEYN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABETQU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEYS" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABFJGM" varchar(8) NOT NULL default '00000000',
  "AAAAMIPQ" varchar(6) NOT NULL default '000000',
  "AAABLXUD" varchar(8) NOT NULL default '00000000',
  "AAAAJQZV" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABETQU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEZH" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABLEPD" varchar(30) NOT NULL default ' ',
  "AAABLEPE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEZQ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHDEU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHEZY" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFAD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(32) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFAL" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAAAHFAO" varchar(30) NOT NULL default ' ',
  "AAAAWMGO" varchar(30) NOT NULL default ' ',
  "AAAAHFAQ" varchar(30) NOT NULL default ' ',
  "AAAAHFAR" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAAAHFIB" varchar(4) NOT NULL default ' ',
  "AAAAHFAV" varchar(1) NOT NULL default ' ',
  "AAAAXDVP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFAZ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABBMJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFBH" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAHFBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFBP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAAAHFCR" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAAAHFCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFBW" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAAAHFCR" varchar(5) NOT NULL default '00000',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABBMJS","AAAAHFCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFCG" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAAAHFCR" varchar(5) NOT NULL default '00000',
  "AAAAHFCS" varchar(5) NOT NULL default '00000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAHFCN" varchar(10) NOT NULL default ' ',
  "AAAAHFCO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAAAHFCR","AAAAHFCS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFCU" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABEHLM" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABEHLM","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFDE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHFDI" varchar(3) NOT NULL default '000',
  "AAAAHFDJ" varchar(5) NOT NULL default ' ',
  "AAAAHFDK" decimal(17,2) NOT NULL default '0.00',
  "AAAAHFDL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFDP" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(3) NOT NULL default '000',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABKAFO","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFDZ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(3) NOT NULL default '000',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABBMJS","AAABKAFO","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFEL" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFES" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABFTRT" varchar(10) NOT NULL default ' ',
  "AAAAUXJC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABFTRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFFA" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABFTRT" varchar(10) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABBMJS","AAABFTRT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFFK" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABBMJS" varchar(3) NOT NULL default ' ',
  "AAABFTRT" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFRUM" varchar(30) NOT NULL default ' ',
  "AAAAHFFQ" varchar(2) NOT NULL default ' ',
  "AAABFRUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABBMJS","AAABFTRT","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFFW" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(32) NOT NULL default ' ',
  "AAABIAEU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABLECD","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFGD" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLECD" varchar(32) NOT NULL default ' ',
  "AAABIAEU" varchar(10) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABLECD","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFGN" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(32) NOT NULL default ' ',
  "AAABIAEU" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAAAHFGS" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(32) NOT NULL default ' ',
  "AAABMNNA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABLECD","AAABIAEU","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFGZ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFIB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABGMEA","AAAAHFIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFHG" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFIB" varchar(4) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABGMEA","AAAAHFIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFHQ" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFIB" varchar(4) NOT NULL default ' ',
  "AAAAXDVP" varchar(32) NOT NULL default ' ',
  "AAAAOTHM" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAHFHX" varchar(10) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABGMEA","AAAAHFIB","AAAAXDVP","AAAAOTHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFIE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABGMEA","AAAAHFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFIL" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABGMEA","AAAAHFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFIV" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAABAWWI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABGMEA","AAAAHFKY","AAABAWWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFJE" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAABAWWI" varchar(10) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  "AAABMECK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABGMEA","AAAAHFKY","AAABAWWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFJR" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAABAWWI" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABDHZZ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABGMEA","AAAAHFKY","AAABAWWI","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFKF" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAAAHFKZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABGMEA","AAAAHFKY","AAAAHFKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFKO" (
  "AAABMQHU" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGMEA" varchar(32) NOT NULL default ' ',
  "AAAAHFKY" varchar(5) NOT NULL default ' ',
  "AAAAHFKZ" varchar(5) NOT NULL default ' ',
  "AAABFLMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMLQA","AAABGMEA","AAAAHFKY","AAAAHFKZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFLA" (
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAAAHFLQ" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(72) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABGKXE","AAAAHFLQ","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFLI" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGKXE" varchar(8) NOT NULL default ' ',
  "AAAAHFLQ" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(72) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGKXE","AAAAHFLQ","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABIBEG" varchar(3) NOT NULL default '000',
  "AAABIBEC" varchar(3) NOT NULL default '000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMCNU","AAABMDBA"),
  KEY "AAAAHFMI" ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRXE" varchar(4) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAAANDRS" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABIHLK" varchar(8) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAIBGY" varchar(22) NOT NULL default ' ',
  "AAAAHTIK" varchar(8) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAAAHTIE" decimal(13,2) NOT NULL default '0.00',
  "AAAAHTJE" decimal(13,2) NOT NULL default '0.00',
  "AAAAHXED" decimal(13,2) NOT NULL default '0.00',
  "AAAAHTJF" decimal(13,2) NOT NULL default '0.00',
  "AAAAHFNM" decimal(13,2) NOT NULL default '0.00',
  "AAAAHFNN" decimal(13,2) NOT NULL default '0.00',
  "AAAAWFAP" varchar(4) NOT NULL default ' ',
  "AAAAHFQM" varchar(4) NOT NULL default '0000',
  "AAAAHFQK" varchar(10) NOT NULL default ' ',
  "AAAAHFNR" varchar(3) NOT NULL default '000',
  "AAAAWFAR" varchar(4) NOT NULL default ' ',
  "AAAAHFPT" varchar(4) NOT NULL default '0000',
  "AAABKTSF" varchar(10) NOT NULL default ' ',
  "AAAAHFNV" varchar(3) NOT NULL default '000',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAAAHFNZ" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAAAMVOG" varchar(1) NOT NULL default ' ',
  "AAABMCZN" varchar(10) NOT NULL default ' ',
  "AAABMCZO" varchar(3) NOT NULL default '000',
  "AAAAHTIU" varchar(2) NOT NULL default '00',
  "AAAAHTIV" varchar(2) NOT NULL default '00',
  "AAAAHTIY" varchar(10) NOT NULL default ' ',
  "AAAAHTIZ" varchar(10) NOT NULL default ' ',
  "AAAAHTJA" varchar(10) NOT NULL default ' ',
  "AAAAHTJB" varchar(10) NOT NULL default ' ',
  "AAAAHTJC" varchar(10) NOT NULL default ' ',
  "AAAAHTJD" varchar(10) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAAAHTJG" varchar(1) NOT NULL default ' ',
  "AAAAHTJH" varchar(1) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAAAHTJK" varchar(10) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAAAHXDZ" varchar(2) NOT NULL default '00',
  "AAAAHXEA" varchar(2) NOT NULL default '00',
  "AAAAHXSZ" varchar(2) NOT NULL default '00',
  "AAAAHFOX" varchar(10) NOT NULL default ' ',
  "AAAAHFOY" varchar(3) NOT NULL default '000',
  "AAAAHFOZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAHFPA" decimal(13,2) NOT NULL default '0.00',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAHVEJ" varchar(1) NOT NULL default ' ',
  "AAAAXZDS" varchar(4) NOT NULL default ' ',
  "AAAAHFPE" varchar(8) NOT NULL default '00000000',
  "AAAAHRTV" varchar(1) NOT NULL default ' ',
  "AAAAHFPG" varchar(10) NOT NULL default ' ',
  "AAAAHFPH" varchar(3) NOT NULL default '000',
  "AAAAHXFN" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRXE","AAABMCNU","AAABMDBA","AAABMPWO"),
  KEY "AAAAHFPP" ("AAABMRIL","AAABKTSF","AAAAWFAR","AAAAHFPT"),
  KEY "AAAAHFPU" ("AAABMCVP","AAABKRXE","AAABMRIL"),
  KEY "AAAAHFPY" ("AAABIHLK","AAABMCNU","AAABKRXE","AAABMRIL"),
  KEY "AAAAHFQD" ("AAABLXTG","AAABMCZG","AAABMCNU","AAABKRXE","AAABMRIL"),
  KEY "AAAAHFQJ" ("AAAAHFQK","AAAAWFAP","AAAAHFQM","AAABKRXE","AAABMRIL"),
  KEY "AAAAHFQP" ("AAABMCZN","AAABMCZO","AAABKRXE","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAAAJMCY" varchar(10) NOT NULL default ' ',
  "AAAAHFQY" varchar(2) NOT NULL default ' ',
  "AAAAHFQZ" varchar(2) NOT NULL default ' ',
  "AAAAHFRA" varchar(2) NOT NULL default ' ',
  "AAAAHFRB" varchar(2) NOT NULL default ' ',
  "AAAAHFRC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCNU","AAAAJMCY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKTX" varchar(20) NOT NULL default ' ',
  "AAABKGBX" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKTX","AAABKGBX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKTX" varchar(20) NOT NULL default ' ',
  "AAABIZWL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKTX","AAABIZWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFCP" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKTX" varchar(20) NOT NULL default ' ',
  "AAAAHFSB" varchar(1) NOT NULL default ' ',
  "AAAAHFSC" varchar(1) NOT NULL default ' ',
  "AAAAHFSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGKTX" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAASAKB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGKTX","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAAARFNK" varchar(1) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABFMHH" int(11) NOT NULL default '0',
  "AAABKOTY" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAAARFNK","AAABJARD","AAABFMHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAIFKD" varchar(2) NOT NULL default ' ',
  "AAAARFNK" varchar(1) NOT NULL default ' ',
  "AAABJARD" varchar(6) NOT NULL default '000000',
  "AAABFMHH" int(11) NOT NULL default '0',
  "AAABMKFH" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAAAIFKD","AAAARFNK","AAABJARD","AAABFMHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(3) NOT NULL default ' ',
  "AAABLECD" varchar(3) NOT NULL default ' ',
  "AAABDXHF" varchar(20) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAAAHFTW" varchar(2) NOT NULL default ' ',
  "AAAAHFTX" varchar(2) NOT NULL default ' ',
  "AAAAHFTY" varchar(2) NOT NULL default ' ',
  "AAAAHFTZ" varchar(2) NOT NULL default ' ',
  "AAAAHFUA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC","AAABLECD","AAABDXHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKFH" varchar(22) NOT NULL default ' ',
  "AAAAHFUI" varchar(8) NOT NULL default '00000000',
  "AAAAHFUJ" varchar(8) NOT NULL default '00000000',
  "AAAAQZPC" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAAAHFUM" varchar(80) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABGVKI" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAAAHFUZ" varchar(1) NOT NULL default ' ',
  "AAAAHFVA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(2) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAAAHFVJ" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(18) NOT NULL default ' ',
  "AAABMCVW" varchar(10) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABKRJU" varchar(8) NOT NULL default '00000000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAHFYC" varchar(22) NOT NULL default ' ',
  "AAABMCXK" varchar(10) NOT NULL default '0000000000',
  "AAABMCXL" varchar(8) NOT NULL default '00000000',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABKRRC" varchar(6) NOT NULL default ' ',
  "AAABKQFI" varchar(8) NOT NULL default '00000000',
  "AAAAHFVZ" varchar(10) NOT NULL default '0000000000',
  "AAAAHFWA" varchar(8) NOT NULL default '00000000',
  "AAAAHFWB" varchar(4) NOT NULL default ' ',
  "AAAAHFWC" varchar(6) NOT NULL default ' ',
  "AAABKQQE" varchar(10) NOT NULL default '0000000000',
  "AAABKQQF" varchar(8) NOT NULL default '00000000',
  "AAABLJTO" varchar(10) NOT NULL default '0000000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABLUTZ" varchar(3) NOT NULL default '000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAAAQJFP" varchar(4) NOT NULL default ' ',
  "AAAAHFWO" varchar(4) NOT NULL default ' ',
  "AAAAHFWP" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAAAHFWR" varchar(10) NOT NULL default ' ',
  "AAAAHFWS" varchar(12) NOT NULL default ' ',
  "AAAAHFWT" varchar(22) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABDJEI" varchar(10) NOT NULL default ' ',
  "AAAAWUMN" varchar(1) NOT NULL default ' ',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABKKET" varchar(1) NOT NULL default ' ',
  "AAAAHFXA" decimal(15,3) NOT NULL default '0.000',
  "AAAAHFXB" decimal(15,3) NOT NULL default '0.000',
  "AAAAHFXC" decimal(15,3) NOT NULL default '0.000',
  "AAAAHFXD" decimal(15,3) NOT NULL default '0.000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKNRS" varchar(3) NOT NULL default ' ',
  "AAABLLXX" decimal(5,0) NOT NULL default '0',
  "AAABLLXY" decimal(5,0) NOT NULL default '0',
  "AAAAHFXI" varchar(1) NOT NULL default ' ',
  "AAAAHFXJ" varchar(1) NOT NULL default ' ',
  "AAAAHFXK" varchar(1) NOT NULL default ' ',
  "AAAAHFXL" varchar(10) NOT NULL default ' ',
  "AAAAHFYF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDBA"),
  KEY "AAAAHFXP" ("AAABMRIL","AAABMOHM","AAABLXTG","AAAAHFYC"),
  KEY "AAAAHFXU" ("AAABMRIL","AAABMOHM","AAABMLCZ","AAABMCVI"),
  KEY "AAAAHFXZ" ("AAABMRIL","AAABMOHM","AAAAHFYC"),
  KEY "AAAAHFYD" ("AAABMRIL","AAAAHFYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMCMG" varchar(6) NOT NULL default '000000',
  "AAABEZHC" varchar(18) NOT NULL default ' ',
  "AAAAHFYL" varchar(4) NOT NULL default ' ',
  "AAAAHFYM" varchar(10) NOT NULL default ' ',
  "AAAAHFYN" varchar(22) NOT NULL default ' ',
  "AAABJDMI" decimal(15,3) NOT NULL default '0.000',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABIBFS" decimal(15,2) NOT NULL default '0.00',
  "AAABIBGO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHFYS" varchar(5) NOT NULL default ' ',
  "AAAAHFYT" varchar(5) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(14) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMDBA","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWTHC" varchar(3) NOT NULL default ' ',
  "AAAAHFZD" varchar(1) NOT NULL default ' ',
  "AAAAHFZE" varchar(1) NOT NULL default ' ',
  "AAAAHFZF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWTHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHFZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFHR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAIGFX" varchar(3) NOT NULL default '000',
  "AAABAEVU" varchar(12) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAAAHFZX" varchar(1) NOT NULL default ' ',
  "AAAAHFZY" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAWFHR","AAABMREX","AAAAWFGB","AAABLXJS","AAABGYAB","AAABKYXV","AAABMRFQ","AAABKVUP","AAABMCPV","AAAAIGFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGRAE" varchar(32) NOT NULL default ' ',
  "AAABLAXK" VARBINARY(255),
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAAAHGAP" varchar(2) NOT NULL default ' ',
  "AAAAHGAQ" varchar(1) NOT NULL default ' ',
  "AAAAHGAR" varchar(10) NOT NULL default ' ',
  "AAAAHGAS" varchar(10) NOT NULL default ' ',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAAAHGAU" varchar(4) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABGYJY" varchar(3) NOT NULL default ' ',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZES" decimal(23,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGRAE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLCCS" varchar(8) NOT NULL default '00000000',
  "AAABLADX" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABLAAD" varchar(1) NOT NULL default ' ',
  "AAABLABE" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABLABB" varchar(10) NOT NULL default ' ',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABLAXJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLCCS","AAABLADX","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWFHR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABDUKY" varchar(1) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAABAEVU" varchar(12) NOT NULL default ' ',
  "AAAAHGEO" varchar(8) NOT NULL default '00000000',
  "AAAAHGEP" varchar(12) NOT NULL default ' ',
  "AAAAQPUJ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABLADO" varchar(25) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABLADN" varchar(16) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAPJXH" varchar(10) NOT NULL default ' ',
  "AAAAHHAW" varchar(4) NOT NULL default ' ',
  "AAAAHHAY" varchar(8) NOT NULL default '00000000',
  "AAAAVUHR" varchar(5) NOT NULL default '00000',
  "AAAAXNLN" varchar(8) NOT NULL default '00000000',
  "AAAAHGFB" varchar(10) NOT NULL default ' ',
  "AAAAYDKC" decimal(7,3) NOT NULL default '0.000',
  "AAABLTUX" varchar(1) NOT NULL default ' ',
  "AAAAHGFE" varchar(1) NOT NULL default ' ',
  "AAAAHGFF" varchar(8) NOT NULL default '00000000',
  "AAAAHGFG" varchar(10) NOT NULL default '0000000000',
  "AAAAHGFH" varchar(1) NOT NULL default ' ',
  "AAAAHGFI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWFHR","AAABMREX","AAAAWFGB","AAABLXJS","AAABGYAB","AAABKYXV","AAABMRFQ","AAABKVUP","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOHI" varchar(10) NOT NULL default ' ',
  "AAAAWFHR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABDUKY" varchar(1) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAABAEVU" varchar(12) NOT NULL default ' ',
  "AAAAHGEO" varchar(8) NOT NULL default '00000000',
  "AAAAHGEP" varchar(12) NOT NULL default ' ',
  "AAAAQPUJ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABLADO" varchar(25) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABLADN" varchar(16) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAPJXH" varchar(10) NOT NULL default ' ',
  "AAAAHHAW" varchar(4) NOT NULL default ' ',
  "AAAAHHAY" varchar(8) NOT NULL default '00000000',
  "AAAAVUHR" varchar(5) NOT NULL default '00000',
  "AAAAXNLN" varchar(8) NOT NULL default '00000000',
  "AAAAHGFB" varchar(10) NOT NULL default ' ',
  "AAAAYDKC" decimal(7,3) NOT NULL default '0.000',
  "AAABLTUX" varchar(1) NOT NULL default ' ',
  "AAAAHGFE" varchar(1) NOT NULL default ' ',
  "AAAAHGFF" varchar(8) NOT NULL default '00000000',
  "AAAAHGFG" varchar(10) NOT NULL default '0000000000',
  "AAAAHGFH" varchar(1) NOT NULL default ' ',
  "AAAAHGFI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAWOHI","AAAAWFHR","AAABMREX","AAAAWFGB","AAABLXJS","AAABGYAB","AAABKYXV","AAABMRFQ","AAABKVUP","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCOM" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCRG" decimal(15,2) NOT NULL default '0.00',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCOM","AAABMDBA","AAABMDBB","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCXE" varchar(8) NOT NULL default ' ',
  "AAABLCCS" varchar(8) NOT NULL default '00000000',
  "AAABLADX" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCXE","AAABLCCS","AAABLADX","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGHL" (
  "AAABGEZT" varchar(5) NOT NULL default ' ',
  "AAAAHGHW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABGEZT","AAAAHGHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGHQ" (
  "AAABGEZT" varchar(5) NOT NULL default ' ',
  "AAAAHGHW" int(11) NOT NULL default '0',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABGEZT","AAAAHGHW","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABGZJM" varchar(10) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABGZJM","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATDSZ" varchar(12) NOT NULL default ' ',
  "AAAAHGIS" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAAAIVEY" varchar(10) NOT NULL default ' ',
  "AAAAJXCR" varchar(10) NOT NULL default ' ',
  "AAAAHGIO" varchar(4) NOT NULL default ' ',
  "AAAAJXDG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAATDSZ","AAAAHGIS","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGIU" (
  "AAABLUFA" varchar(12) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGIY" (
  "AAABLUFA" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUFA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHGJK" varchar(20) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABFVDS" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHGJK","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJHF" varchar(10) NOT NULL default ' ',
  "AAABLJCX" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABMCSW" varchar(8) NOT NULL default '00000000',
  "AAABLIYO" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLLYC" decimal(15,2) NOT NULL default '0.00',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCRG" decimal(13,2) NOT NULL default '0.00',
  "AAAAHGMC" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLJHF","AAABLJCX","AAABLWHI","AAABMCSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(1) NOT NULL default ' ',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAABMCVP" varchar(10) NOT NULL default ' ',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABMCVQ" varchar(5) NOT NULL default '00000',
  "AAABMCVR" varchar(2) NOT NULL default '00',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABLTXE" varchar(3) NOT NULL default '000',
  "AAABLTXF" varchar(2) NOT NULL default '00',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAAHGLY" varchar(5) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGMC" decimal(15,2) NOT NULL default '0.00',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABJSBR","AAABMCVP","AAAAKZUL","AAAAKZUN","AAABMCVQ","AAABMCVR","AAABMLEK","AAABLTXE","AAABLTXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAABKSPJ" varchar(10) NOT NULL default ' ',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAABKSQI" varchar(5) NOT NULL default '00000',
  "AAABKPUH" varchar(2) NOT NULL default '00',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAAHGLY" varchar(5) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRG" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGMC" decimal(15,2) NOT NULL default '0.00',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABLUHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJSBR","AAABKSPJ","AAAAKZUL","AAAAKZUN","AAABKSQI","AAABKPUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABJSBR" varchar(5) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAAAKZUL" varchar(10) NOT NULL default ' ',
  "AAAAKZUN" varchar(10) NOT NULL default ' ',
  "AAAALBZM" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAALBYA" decimal(15,2) NOT NULL default '0.00',
  "AAAAKZYU" decimal(15,2) NOT NULL default '0.00',
  "AAAAKZZU" decimal(15,2) NOT NULL default '0.00',
  "AAAAKZZY" decimal(15,2) NOT NULL default '0.00',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABMCSR" varchar(10) NOT NULL default ' ',
  "AAABFDOM" varchar(10) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABMCRG" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABJSBR","AAABMDBA","AAAAKZUL","AAAAKZUN","AAAALBZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHGQE" varchar(32) NOT NULL default ' ',
  "AAAAHGQF" varchar(3) NOT NULL default '000',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHGQE","AAAAHGQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMAKT" varchar(10) NOT NULL default '0000000000',
  "AAABLZCP" varchar(40) NOT NULL default ' ',
  "AAABLXIZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCC","AAABMPWP","AAABLZBN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABKGPK" varchar(70) NOT NULL default ' ',
  "AAABIAET" varchar(2) NOT NULL default ' ',
  "AAAAHGQE" varchar(32) NOT NULL default ' ',
  "AAABGYGV" varchar(10) NOT NULL default ' ',
  "AAAAHGQC" varchar(1) NOT NULL default ' ',
  "AAAAHGQB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQDU","AAABLFYC","AAABKGPK","AAABIAET","AAAAHGQE"),
  KEY "AAAAHGPB" ("AAABMRIL","AAAAHGQE","AAABIAET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHGQE" varchar(32) NOT NULL default ' ',
  "AAAAHGQF" varchar(3) NOT NULL default '000',
  "AAABDAWX" varchar(2) NOT NULL default '00',
  "AAABLFOX" varchar(10) NOT NULL default ' ',
  "AAAAVASH" varchar(70) NOT NULL default ' ',
  "AAAAHGPM" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLFYC" varchar(10) NOT NULL default ' ',
  "AAABKGPK" varchar(70) NOT NULL default ' ',
  "AAAAHGPQ" varchar(10) NOT NULL default ' ',
  "AAAAHGPR" varchar(10) NOT NULL default ' ',
  "AAAAHGPS" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPT" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPU" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPV" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPW" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPX" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGPY" varchar(5) NOT NULL default ' ',
  "AAABMKFU" decimal(15,0) NOT NULL default '0',
  "AAABLEKS" varchar(12) NOT NULL default ' ',
  "AAAAHGQB" varchar(1) NOT NULL default ' ',
  "AAAAHGQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHGQE","AAAAHGQF","AAABDAWX"),
  KEY "AAAAHGQH" ("AAABMRIL","AAABLFOX","AAAAVASH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAATOKJ" varchar(32) NOT NULL default ' ',
  "AAAAHGQO" varchar(1) NOT NULL default ' ',
  "AAAAHGQP" varchar(8) NOT NULL default '00000000',
  "AAAAHGQQ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAATOKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGQT" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAAAHGQV" varchar(3) NOT NULL default '000',
  "AAAAHGQW" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGQX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQIL" varchar(10) NOT NULL default ' ',
  "AAABMLTD" varchar(6) NOT NULL default '000000',
  "AAABMLEK" varchar(4) NOT NULL default '0000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRFB" varchar(5) NOT NULL default ' ',
  "AAABMCNV" varchar(5) NOT NULL default ' ',
  "AAABLWHG" varchar(10) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABMMRQ" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMQIK" varchar(8) NOT NULL default '00000000',
  "AAABKSRX" varchar(8) NOT NULL default '00000000',
  "AAABKSRU" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLYWS" varchar(10) NOT NULL default ' ',
  "AAABLYFT" varchar(6) NOT NULL default '000000',
  "AAAAHGRT" decimal(15,3) NOT NULL default '0.000',
  "AAAASCJD" decimal(13,3) NOT NULL default '0.000',
  "AAAASCJE" decimal(13,3) NOT NULL default '0.000',
  "AAAAHGRW" decimal(13,3) NOT NULL default '0.000',
  "AAABLYIF" varchar(8) NOT NULL default '00000000',
  "AAAAHGRY" decimal(13,3) NOT NULL default '0.000',
  "AAABKXWD" varchar(8) NOT NULL default '00000000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAHGSB" decimal(13,3) NOT NULL default '0.000',
  "AAABKPOJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHGSD" decimal(15,2) NOT NULL default '0.00',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABLCFT" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQIL","AAABMLTD","AAABMLEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAAAKZQX" varchar(10) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAAAHGTO" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLWHI" varchar(10) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABMCRH" varchar(8) NOT NULL default '00000000',
  "AAABKRXN" varchar(5) NOT NULL default ' ',
  "AAABMCRE" varchar(2) NOT NULL default ' ',
  "AAAASZJE" decimal(15,3) NOT NULL default '0.000',
  "AAABMBZP" varchar(3) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABKJVG","AAABMQCE","AAABMCOP","AAAAKZQX","AAABJGDL","AAAAHGTO","AAABMREX","AAABMCPV","AAABLWHI","AAABMCRF","AAABMCRH","AAABKRXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZFR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZZFR","AAABMREX","AAAAWFGB","AAABGYAB","AAABKYXV","AAABMRFQ","AAAAOZWL","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOHI" varchar(10) NOT NULL default ' ',
  "AAAAZZFR" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAWOHI","AAAAZZFR","AAABMREX","AAAAWFGB","AAABGYAB","AAABKYXV","AAABMRFQ","AAAAOZWL","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLXJS","AAABGYAB","AAABKYXV","AAABMRFQ","AAAAOZWL","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAWOHI" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAOZWL" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAWOHI","AAABMREX","AAABLXJS","AAABGYAB","AAABKYXV","AAABMRFQ","AAAAOZWL","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLADI" varchar(13) NOT NULL default ' ',
  "AAABKXOZ" varchar(5) NOT NULL default '00000',
  "AAABKXML" varchar(8) NOT NULL default '00000000',
  "AAABKXMM" varchar(6) NOT NULL default '000000',
  "AAABKXKH" varchar(4) NOT NULL default '0000',
  "AAAAHGYD" varchar(1) NOT NULL default '0',
  "AAABKZWQ" varchar(1) NOT NULL default ' ',
  "AAABLABE" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABLABB" varchar(10) NOT NULL default ' ',
  "AAABKXIN" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKXIP" varchar(5) NOT NULL default ' ',
  "AAABKXIO" decimal(13,2) NOT NULL default '0.00',
  "AAABKXIQ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLADI","AAABKXOZ","AAABKXML","AAABKXMM","AAABKXKH","AAAAHGYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHGYZ" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAHGZD" varchar(8) NOT NULL default '00000000',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABBNLN" varchar(1) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAAAHHAW" varchar(4) NOT NULL default ' ',
  "AAAAPJXH" varchar(10) NOT NULL default ' ',
  "AAABDSYZ" varchar(25) NOT NULL default ' ',
  "AAAAHGYT" varchar(60) NOT NULL default ' ',
  "AAAAHGYU" varchar(18) NOT NULL default ' ',
  "AAAAHGYV" varchar(22) NOT NULL default ' ',
  "AAABENBS" varchar(1) NOT NULL default ' ',
  "AAAAHGYX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHGYZ","AAABMREX","AAABKVUP","AAABMCPV","AAAAHGZD","AAABIEXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHGZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABLCCS" varchar(8) NOT NULL default '00000000',
  "AAABLADX" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABLAAD" varchar(1) NOT NULL default ' ',
  "AAABLABE" varchar(2) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABMCRF" varchar(10) NOT NULL default ' ',
  "AAABLABB" varchar(10) NOT NULL default ' ',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABLAXJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMEFT","AAABLAYL","AAABLCCS","AAABLADX","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAQVTK" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABGXGV" varchar(10) NOT NULL default ' ',
  "AAABLXJS" varchar(10) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABGYAB" varchar(2) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABKYXV" varchar(5) NOT NULL default ' ',
  "AAAAHHAU" decimal(15,2) NOT NULL default '0.00',
  "AAAAQVYB" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHAW" varchar(4) NOT NULL default ' ',
  "AAAAPJXH" varchar(10) NOT NULL default ' ',
  "AAAAHHAY" varchar(8) NOT NULL default '00000000',
  "AAAAHHAZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAQVTK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAAAHHBS" varchar(3) NOT NULL default '000',
  "AAAAHHBH" varchar(4) NOT NULL default ' ',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAAAYATC" varchar(16) NOT NULL default ' ',
  "AAAAHHBK" varchar(10) NOT NULL default ' ',
  "AAAAHHBL" varchar(1) NOT NULL default ' ',
  "AAAAHHBM" varchar(5) NOT NULL default ' ',
  "AAAAHHBN" varchar(20) NOT NULL default ' ',
  "AAAAHHBO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABKGSX","AAAAHHBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAAAHHBX" varchar(1) NOT NULL default ' ',
  "AAAAHHGR" varchar(1) NOT NULL default ' ',
  "AAAAHHBZ" varchar(1) NOT NULL default ' ',
  "AAAAHHGS" varchar(1) NOT NULL default ' ',
  "AAAAHHCB" varchar(1) NOT NULL default ' ',
  "AAABCWSJ" varchar(2) NOT NULL default ' ',
  "AAABFJAC" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPPG" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCQW" varchar(8) NOT NULL default '00000000',
  "AAAAHHCJ" varchar(6) NOT NULL default ' ',
  "AAAAHHCK" varchar(2) NOT NULL default ' ',
  "AAABLONK" varchar(5) NOT NULL default ' ',
  "AAABJRIY" decimal(13,2) NOT NULL default '0.00',
  "AAAAHHCN" decimal(13,2) NOT NULL default '0.00',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAABHFBB" decimal(13,2) NOT NULL default '0.00',
  "AAAAHHCQ" decimal(13,2) NOT NULL default '0.00',
  "AAABHFBD" varchar(3) NOT NULL default ' ',
  "AAABGCUQ" varchar(4) NOT NULL default ' ',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAWEXJ" varchar(4) NOT NULL default ' ',
  "AAABJRHX" varchar(4) NOT NULL default ' ',
  "AAAAHHCW" varchar(1) NOT NULL default ' ',
  "AAAAWHJO" varchar(10) NOT NULL default ' ',
  "AAAAWEWS" varchar(4) NOT NULL default ' ',
  "AAABJRHY" varchar(50) NOT NULL default ' ',
  "AAAAHHDA" varchar(5) NOT NULL default '00000',
  "AAABMDGB" varchar(16) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAABJRHZ" varchar(3) NOT NULL default ' ',
  "AAABJRIA" varchar(15) NOT NULL default ' ',
  "AAAAHHDF" varchar(11) NOT NULL default ' ',
  "AAABJRIB" varchar(18) NOT NULL default ' ',
  "AAABJRKN" varchar(55) NOT NULL default ' ',
  "AAABMQNG" varchar(2) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABMDGQ" varchar(13) NOT NULL default ' ',
  "AAABMKSP" varchar(1) NOT NULL default ' ',
  "AAAAHHDO" varchar(10) NOT NULL default ' ',
  "AAAAPOSH" varchar(13) NOT NULL default ' ',
  "AAAAHHDQ" varchar(2) NOT NULL default ' ',
  "AAABJRID" varchar(40) NOT NULL default ' ',
  "AAAAHHDS" varchar(1) NOT NULL default ' ',
  "AAABLENN" varchar(30) NOT NULL default ' ',
  "AAABLENO" varchar(30) NOT NULL default ' ',
  "AAAAMOKP" varchar(16) NOT NULL default ' ',
  "AAAAWHJP" varchar(3) NOT NULL default '000',
  "AAAAHHDX" varchar(10) NOT NULL default ' ',
  "AAAAHHDY" varchar(10) NOT NULL default ' ',
  "AAAAHHDZ" varchar(10) NOT NULL default ' ',
  "AAABLCDH" varchar(10) NOT NULL default ' ',
  "AAAAHHEB" varchar(1) NOT NULL default ' ',
  "AAAAHHEC" varchar(10) NOT NULL default ' ',
  "AAABMLFE" varchar(3) NOT NULL default ' ',
  "AAAAIGFX" varchar(3) NOT NULL default '000',
  "AAABMCNG" decimal(9,5) NOT NULL default '0.00000',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAHHEH" varchar(8) NOT NULL default '00000000',
  "AAAAHHEI" varchar(3) NOT NULL default ' ',
  "AAAAHHEJ" varchar(10) NOT NULL default ' ',
  "AAABLXRH" varchar(35) NOT NULL default ' ',
  "AAAAHHIM" varchar(20) NOT NULL default ' ',
  "AAAAHHIN" varchar(5) NOT NULL default ' ',
  "AAAAHHEN" decimal(13,2) NOT NULL default '0.00',
  "AAAAWEWE" varchar(2) NOT NULL default '00',
  "AAABMDAG" varchar(30) NOT NULL default ' ',
  "AAAAWSEN" varchar(35) NOT NULL default ' ',
  "AAAAHHER" varchar(35) NOT NULL default ' ',
  "AAABMCUU" varchar(1) NOT NULL default ' ',
  "AAABFRUM" varchar(132) NOT NULL default ' ',
  "AAAAHHEU" varchar(132) NOT NULL default ' ',
  "AAAAHHEV" varchar(1) NOT NULL default ' ',
  "AAABFRUN" varchar(132) NOT NULL default ' ',
  "AAAAHHEX" varchar(132) NOT NULL default ' ',
  "AAAAHHEY" varchar(1) NOT NULL default ' ',
  "AAABFRUO" varchar(132) NOT NULL default ' ',
  "AAAAHHFA" varchar(132) NOT NULL default ' ',
  "AAAAHHFB" varchar(1) NOT NULL default ' ',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAAAHHFD" varchar(6) NOT NULL default ' ',
  "AAAAHHFE" varchar(4) NOT NULL default ' ',
  "AAAAHHFF" varchar(1) NOT NULL default ' ',
  "AAAAHHFG" varchar(70) NOT NULL default ' ',
  "AAAAHHFH" varchar(5) NOT NULL default ' ',
  "AAAAHHFI" varchar(6) NOT NULL default ' ',
  "AAAAHHFJ" varchar(4) NOT NULL default ' ',
  "AAAAHHFK" varchar(1) NOT NULL default ' ',
  "AAAAHHFL" varchar(70) NOT NULL default ' ',
  "AAAAHHFM" varchar(5) NOT NULL default ' ',
  "AAAAHHFN" varchar(34) NOT NULL default ' ',
  "AAABMCZL" varchar(12) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABKGSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHFT" (
  "AAAAHHGA" varchar(8) NOT NULL default ' ',
  "AAAAHHGB" varchar(27) NOT NULL default ' ',
  "AAABMDCZ" varchar(20) NOT NULL default ' ',
  "AAABMDDF" varchar(20) NOT NULL default ' ',
  "AAABMOQQ" varchar(50) NOT NULL default ' ',
  "AAAAHHFZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHHGA","AAAAHHGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(10) NOT NULL default ' ',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFDQ" varchar(4) NOT NULL default ' ',
  "AAAAHHLB" varchar(50) NOT NULL default ' ',
  "AAABJRHE" varchar(20) NOT NULL default ' ',
  "AAAAHHIW" varchar(20) NOT NULL default ' ',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABJDQB" varchar(1) NOT NULL default ' ',
  "AAABJXFC" varchar(1) NOT NULL default ' ',
  "AAAAHHGR" varchar(1) NOT NULL default ' ',
  "AAAAHHGS" varchar(1) NOT NULL default ' ',
  "AAAAHHGT" varchar(1) NOT NULL default ' ',
  "AAABJSIR" varchar(8) NOT NULL default '00000000',
  "AAABLQBX" varchar(12) NOT NULL default ' ',
  "AAAAHHGW" varchar(1) NOT NULL default ' ',
  "AAAAHHGX" varchar(12) NOT NULL default ' ',
  "AAAAHHGY" varchar(1) NOT NULL default ' ',
  "AAAAHHGZ" varchar(8) NOT NULL default '00000000',
  "AAAAHHHA" varchar(6) NOT NULL default '000000',
  "AAAAHHHB" varchar(1) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABJRHR" varchar(18) NOT NULL default ' ',
  "AAABJRHS" varchar(35) NOT NULL default ' ',
  "AAAAHHHF" varchar(35) NOT NULL default ' ',
  "AAABKGST" varchar(5) NOT NULL default '00000',
  "AAAAHHHH" varchar(3) NOT NULL default '000',
  "AAABKGSU" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAZAZF" varchar(1) NOT NULL default ' ',
  "AAAAHHHN" varchar(1) NOT NULL default ' ',
  "AAAAHHHO" decimal(17,2) NOT NULL default '0.00',
  "AAABGZVC" decimal(17,2) NOT NULL default '0.00',
  "AAABGZVD" decimal(17,2) NOT NULL default '0.00',
  "AAAAHHHR" varchar(1) NOT NULL default ' ',
  "AAAAHHHS" varchar(1) NOT NULL default ' ',
  "AAAAHHHT" decimal(17,2) NOT NULL default '0.00',
  "AAAAHHHU" decimal(17,2) NOT NULL default '0.00',
  "AAAAQWMZ" varchar(8) NOT NULL default '00000000',
  "AAAAQWNA" varchar(8) NOT NULL default '00000000',
  "AAAAQWNB" decimal(9,3) NOT NULL default '0.000',
  "AAABFBBR" varchar(20) NOT NULL default ' ',
  "AAAAHHHZ" varchar(23) NOT NULL default ' ',
  "AAAAHHIA" varchar(5) NOT NULL default '00000',
  "AAABLXTW" varchar(8) NOT NULL default ' ',
  "AAABJRHV" varchar(1) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABFMVJ" varchar(12) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABJRLW" varchar(20) NOT NULL default ' ',
  "AAABJKWN" varchar(1) NOT NULL default ' ',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAABCDET" varchar(1) NOT NULL default ' ',
  "AAAAHHIM" varchar(20) NOT NULL default ' ',
  "AAAAHHIN" varchar(5) NOT NULL default ' ',
  "AAAAHHIO" varchar(4) NOT NULL default '0000',
  "AAAAHHIP" varchar(1) NOT NULL default ' ',
  "AAAAHHIQ" varchar(2) NOT NULL default ' ',
  "AAAAHHIR" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFDQ","AAAAHHLB","AAABJRHE","AAAAHHIW"),
  UNIQUE KEY "AAAAHHIX" ("AAABMRIL","AAABKGSS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABBCMU" varchar(5) NOT NULL default '00000',
  "AAAAHHJE" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABBCMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAAAWFGB" varchar(10) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKVUP" varchar(10) NOT NULL default '0000000000',
  "AAABIEXL" varchar(2) NOT NULL default ' ',
  "AAAAHHJV" varchar(8) NOT NULL default '00000000',
  "AAABEIAQ" varchar(6) NOT NULL default '000000',
  "AAAAWFCG" varchar(27) NOT NULL default ' ',
  "AAAAHHJY" decimal(13,2) NOT NULL default '0.00',
  "AAAAHHJZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAHHKA" decimal(13,2) NOT NULL default '0.00',
  "AAABLPQZ" decimal(13,2) NOT NULL default '0.00',
  "AAAAOSFG" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABKGSX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGSS" varchar(8) NOT NULL default '00000000',
  "AAABKGSX" varchar(5) NOT NULL default '00000',
  "AAABKRZZ" varchar(3) NOT NULL default '000',
  "AAAAHHKL" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGSS","AAABKGSX","AAABKRZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFFDQ" varchar(4) NOT NULL default ' ',
  "AAAAHHLB" varchar(50) NOT NULL default ' ',
  "AAABJRHE" varchar(20) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVK" varchar(60) NOT NULL default ' ',
  "AAABHEXF" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFFDQ","AAAAHHLB","AAABJRHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHLD" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAAAROVJ" varchar(30) NOT NULL default ' ',
  "AAABHJGM" varchar(30) NOT NULL default ' ',
  "AAABLQCP" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB","AAAAROVJ","AAABHJGM","AAABLQCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABLMHB" varchar(6) NOT NULL default ' ',
  "AAAAWDTQ" varchar(10) NOT NULL default ' ',
  "AAABJLNK" varchar(13) NOT NULL default '0000000000000',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABLMHB","AAAAWDTQ","AAABJLNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHHLZ" varchar(1) NOT NULL default ' ',
  "AAAAHHMA" varchar(1) NOT NULL default ' ',
  "AAAAHHMB" varchar(1) NOT NULL default ' ',
  "AAAAHHMC" varchar(1) NOT NULL default ' ',
  "AAABMLNG" varchar(1) NOT NULL default '0',
  "AAAAHHME" varchar(1) NOT NULL default ' ',
  "AAABHAHM" varchar(3) NOT NULL default '000',
  "AAAARSRF" tinyint(3) unsigned NOT NULL default '0',
  "AAABFOQZ" varchar(32) NOT NULL default ' ',
  "AAAAHHMI" varchar(1) NOT NULL default ' ',
  "AAAAHHMJ" varchar(1) NOT NULL default ' ',
  "AAAAHHMK" varchar(1) NOT NULL default ' ',
  "AAAAHHML" varchar(1) NOT NULL default ' ',
  "AAAAHHMM" varchar(2) NOT NULL default ' ',
  "AAAAHHMN" varchar(2) NOT NULL default ' ',
  "AAAAHHMO" varchar(1) NOT NULL default ' ',
  "AAAAHHMP" varchar(1) NOT NULL default ' ',
  "AAAAHHMQ" varchar(1) NOT NULL default ' ',
  "AAAAHHMR" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHHMW" varchar(1) NOT NULL default '0',
  "AAAAHHMX" varchar(1) NOT NULL default ' ',
  "AAAAHHMY" varchar(1) NOT NULL default ' ',
  "AAAAHHMZ" varchar(1) NOT NULL default '0',
  "AAAAHHNA" varchar(1) NOT NULL default ' ',
  "AAAAVZCL" varchar(4) NOT NULL default ' ',
  "AAABFULA" varchar(10) NOT NULL default ' ',
  "AAAAHHND" varchar(10) NOT NULL default ' ',
  "AAAAHHNE" varchar(10) NOT NULL default ' ',
  "AAAAHHNF" varchar(10) NOT NULL default ' ',
  "AAAAHHNG" smallint(6) NOT NULL default '0',
  "AAAAHHNH" decimal(6,2) NOT NULL default '0.00',
  "AAAAHHNI" decimal(5,3) NOT NULL default '0.000',
  "AAAAHHNJ" varchar(2) NOT NULL default ' ',
  "AAAAHHNK" varchar(2) NOT NULL default ' ',
  "AAAAHHNL" varchar(2) NOT NULL default ' ',
  "AAAAHHNM" varchar(3) NOT NULL default ' ',
  "AAAAHHNN" varchar(3) NOT NULL default ' ',
  "AAABFJZE" varchar(1) NOT NULL default ' ',
  "AAAAHHNP" tinyint(3) unsigned NOT NULL default '0',
  "AAAAHHNQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHNR" varchar(1) NOT NULL default ' ',
  "AAAAHHNS" varchar(1) NOT NULL default ' ',
  "AAAAHHNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAMSIK" varchar(1) NOT NULL default '0',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAMSIK","AAABKOTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHHPP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHHPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHHPP" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAHHPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHHPP" varchar(4) NOT NULL default ' ',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  "AAAAHHPF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHHPP","AAAAHIBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABGIMG" varchar(10) NOT NULL default ' ',
  "AAABGIMH" varchar(10) NOT NULL default ' ',
  "AAAAHHPP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABGIMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  "AAABKZAW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAHIBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHHQK" varchar(1) NOT NULL default ' ',
  "AAAAHHQL" decimal(6,2) NOT NULL default '0.00',
  "AAAAHHQM" varchar(1) NOT NULL default ' ',
  "AAABKGMN" varchar(10) NOT NULL default ' ',
  "AAAAHHQO" varchar(10) NOT NULL default ' ',
  "AAAAHHQP" varchar(10) NOT NULL default ' ',
  "AAAAHHQQ" varchar(10) NOT NULL default ' ',
  "AAAAHHQR" varchar(10) NOT NULL default ' ',
  "AAAAHHQS" varchar(12) NOT NULL default ' ',
  "AAAAHHQT" varchar(6) NOT NULL default ' ',
  "AAAAHHQU" varchar(10) NOT NULL default ' ',
  "AAAAHHQV" varchar(1) NOT NULL default ' ',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAHHTP" varchar(4) NOT NULL default ' ',
  "AAABGIMG" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABGIMH" varchar(10) NOT NULL default ' ',
  "AAABIWAM" decimal(6,2) NOT NULL default '0.00',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  "AAAAHHRK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX","AAAAHHTP","AAABGIMG","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHHRZ" varchar(4) NOT NULL default ' ',
  "AAAAHHRV" varchar(4) NOT NULL default ' ',
  "AAAAHHRW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMCRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMCRK" varchar(22) NOT NULL default ' ',
  "AAAAHHST" varchar(10) NOT NULL default ' ',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMCRK","AAAAHHST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAHHTP" varchar(4) NOT NULL default ' ',
  "AAABLUZL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHHTP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHHTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHHTP" varchar(4) NOT NULL default ' ',
  "AAABAZVX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAHHTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHTQ" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABBPOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAAAHHUA" varchar(10) NOT NULL default ' ',
  "AAABIWAM" decimal(6,2) NOT NULL default '0.00',
  "AAABMCSO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREX","AAABMOAH","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAAAHHUN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABMREL","AAAAHIAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAABKMWI" int(11) NOT NULL default '0',
  "AAAAHHYE" varchar(10) NOT NULL default ' ',
  "AAAAHHZI" varchar(4) NOT NULL default ' ',
  "AAAAHHZJ" varchar(4) NOT NULL default ' ',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAIUPG" varchar(22) NOT NULL default ' ',
  "AAAAHHXN" varchar(10) NOT NULL default ' ',
  "AAAAHHZG" varchar(4) NOT NULL default ' ',
  "AAAAHHZH" varchar(4) NOT NULL default ' ',
  "AAABIKEM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHIAY","AAABKMWI"),
  KEY "AAAAHHVL" ("AAAAHHYE","AAAAHHXN","AAABLTOJ"),
  KEY "AAAAHHVP" ("AAAAHHYE","AAAAIUPG","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAABKMWI" int(11) NOT NULL default '0',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAHHXN" varchar(10) NOT NULL default ' ',
  "AAAAHHZG" varchar(4) NOT NULL default ' ',
  "AAAAHHZH" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAAAVTXW" varchar(22) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHIAY","AAABKMWI"),
  KEY "AAAAHHWP" ("AAAAHHXN","AAAAIUPD","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAABKMWI" int(11) NOT NULL default '0',
  "AAAAHHXN" varchar(10) NOT NULL default ' ',
  "AAAAHHZG" varchar(4) NOT NULL default ' ',
  "AAAAHHZH" varchar(4) NOT NULL default ' ',
  "AAAAHHYE" varchar(10) NOT NULL default ' ',
  "AAAAHHZI" varchar(4) NOT NULL default ' ',
  "AAAAHHZJ" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHIAY","AAABKMWI"),
  KEY "AAAAHHXJ" ("AAAAHHYE","AAABLTOJ"),
  KEY "AAAAHHXM" ("AAAAHHXN","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAABKMWI" int(11) NOT NULL default '0',
  "AAAAHHYE" varchar(10) NOT NULL default ' ',
  "AAAAHHZI" varchar(4) NOT NULL default ' ',
  "AAAAHHZJ" varchar(4) NOT NULL default ' ',
  "AAAAIUPG" varchar(22) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAHIAY","AAABKMWI"),
  KEY "AAAAHHYD" ("AAAAHHYE","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLQHQ" int(11) NOT NULL default '0',
  "AAAAHHYS" int(11) NOT NULL default '0',
  "AAAAHHYV" varchar(1) NOT NULL default '0',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAIUPG" varchar(22) NOT NULL default ' ',
  "AAABIKEM" decimal(15,12) NOT NULL default '0.000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLQHQ","AAAAHHYS","AAAAHHYV"),
  KEY "AAAAHHYU" ("AAAAHHYV","AAAAIUPD","AAAAIUPG","AAABLTOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAAAIUPG" varchar(22) NOT NULL default ' ',
  "AAABIKEM" decimal(15,12) NOT NULL default '0.000000000000',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  "AAAAHHZG" varchar(4) NOT NULL default ' ',
  "AAAAHHZH" varchar(4) NOT NULL default ' ',
  "AAAAHHZI" varchar(4) NOT NULL default ' ',
  "AAAAHHZJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAIUPD","AAAAIUPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHHZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAAAIUPD" varchar(22) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAAAHIAD" varchar(4) NOT NULL default ' ',
  "AAAAHIAE" varchar(4) NOT NULL default ' ',
  "AAABIBGA" varchar(22) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAAAIUPD","AAABJGDL","AAABKOTY","AAAAHIAD","AAAAHIAE","AAABIBGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAAAHIAY" varchar(1) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLXTG","AAAAHIAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAAAHIBG" varchar(1) NOT NULL default ' ',
  "AAAAHIBH" varchar(4) NOT NULL default ' ',
  "AAAAHIBI" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLXTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABLTLR" varchar(2) NOT NULL default ' ',
  "AAABCPAQ" varchar(3) NOT NULL default '000',
  "AAABLQVJ" int(11) NOT NULL default '0',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLSIN" varchar(3) NOT NULL default ' ',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ","AAABLTLR","AAABCPAQ","AAABLQVJ","AAABMLNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHICG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTOJ" varchar(4) NOT NULL default '0000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKPXV" varchar(3) NOT NULL default '000',
  "AAABMQHW" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABKIFX" varchar(2) NOT NULL default ' ',
  "AAAAHICP" varchar(2) NOT NULL default ' ',
  "AAAAHICQ" varchar(1) NOT NULL default ' ',
  "AAAAHICR" varchar(3) NOT NULL default '000',
  "AAAAHICS" varchar(1) NOT NULL default ' ',
  "AAAAHICT" varchar(3) NOT NULL default '000',
  "AAAAHICU" varchar(1) NOT NULL default ' ',
  "AAAAHICV" varchar(3) NOT NULL default '000',
  "AAAAHICW" varchar(1) NOT NULL default ' ',
  "AAAAHICX" varchar(3) NOT NULL default '000',
  "AAAAHICY" varchar(1) NOT NULL default ' ',
  "AAAAHICZ" varchar(3) NOT NULL default '000',
  "AAAAHIDA" varchar(1) NOT NULL default ' ',
  "AAAAHIDB" varchar(3) NOT NULL default '000',
  "AAAAHIDC" varchar(3) NOT NULL default '000',
  "AAAAHIDD" varchar(3) NOT NULL default '000',
  "AAAAHIDE" varchar(1) NOT NULL default ' ',
  "AAAAHIDF" varchar(3) NOT NULL default '000',
  "AAAAHIDG" varchar(1) NOT NULL default ' ',
  "AAAAHIDH" varchar(3) NOT NULL default '000',
  "AAAAHIDI" varchar(6) NOT NULL default '000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABGNEW" varchar(10) NOT NULL default ' ',
  "AAAAHIDN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTOJ"),
  KEY "AAAAHIDQ" ("AAABKPXV","AAABMDBB","AAABMQHW","AAABKRXK","AAABMCRJ","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMREX","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAIEON" varchar(3) NOT NULL default '000',
  "AAAAUGNM" varchar(5) NOT NULL default ' ',
  "AAABIYCS" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAAAHLFC" varchar(10) NOT NULL default ' ',
  "AAAAIEOT" varchar(4) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAIEON","AAAAUGNM","AAABIYCS","AAABMREX","AAABMDBB","AAAAHLFC","AAAAIEOT","AAABLHPM","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIFP" varchar(1) NOT NULL default ' ',
  "AAABLTHM" varchar(8) NOT NULL default '00000000',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUFA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIFP","AAABLTHM","AAABLTHN","AAABMDEN","AAABMRFQ","AAABMLES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABMCQU" decimal(13,2) NOT NULL default '0.00',
  "AAABMCQV" decimal(13,2) NOT NULL default '0.00',
  "AAABMCYK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABHFND" varchar(10) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAABJWRQ" varchar(2) NOT NULL default ' ',
  "AAABJRLW" varchar(20) NOT NULL default ' ',
  "AAAAWDVR" varchar(60) NOT NULL default ' ',
  "AAABMAYY" varchar(2) NOT NULL default ' ',
  "AAABJRKN" varchar(16) NOT NULL default ' ',
  "AAAAHIGY" varchar(6) NOT NULL default ' ',
  "AAABKRFR" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAHIHC" varchar(8) NOT NULL default '00000000',
  "AAAAHIHD" varchar(12) NOT NULL default ' ',
  "AAAAHIHE" varchar(8) NOT NULL default '00000000',
  "AAAAHIHF" varchar(12) NOT NULL default ' ',
  "AAAAHIHG" varchar(8) NOT NULL default '00000000',
  "AAAAHIHH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIIJ" varchar(10) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIIJ","AAABMREX","AAABMCTJ","AAABKWDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIIJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIIJ" varchar(10) NOT NULL default ' ',
  "AAAAHIIG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHIIJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWDG" varchar(20) NOT NULL default ' ',
  "AAABKWDH" varchar(50) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  "AAABLCGE" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCTJ","AAABKWDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIIX" (
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABJOWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJA" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  "AAAAHIJD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJOWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJG" (
  "AAAAHIVP" varchar(6) NOT NULL default ' ',
  "AAAAHIJI" varchar(1) NOT NULL default ' ',
  "AAAAHIJJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIVP" varchar(6) NOT NULL default ' ',
  "AAAAHIJO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHIVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJR" (
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJU" (
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIMU","AAABJOWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIJZ" (
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIMU","AAAAHIUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIKE" (
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAAAHITF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIMU","AAAAHITF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIKJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAAAHIKM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHIMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIKP" (
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIKS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  "AAAAHIKV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHIUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHITF" varchar(3) NOT NULL default ' ',
  "AAAAHILB" varchar(1) NOT NULL default '0',
  "AAAAHILC" varchar(30) NOT NULL default ' ',
  "AAAAHILD" varchar(30) NOT NULL default ' ',
  "AAAAHILE" varchar(30) NOT NULL default ' ',
  "AAAAHILF" varchar(30) NOT NULL default ' ',
  "AAAAHILG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHITF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHILJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHITF" varchar(3) NOT NULL default ' ',
  "AAAAHILN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHITF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHILR" (
  "AAAAHIPI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHILU" (
  "AAAAHIPI" varchar(6) NOT NULL default ' ',
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIPI","AAAAHIMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHILZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIPI" varchar(6) NOT NULL default ' ',
  "AAAAHIMC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHIPI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIMF" (
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  "AAAAHIMJ" varchar(1) NOT NULL default ' ',
  "AAAAHIMK" varchar(255) NOT NULL default ' ',
  "AAAAHIML" varchar(255) NOT NULL default ' ',
  "AAAAHIMM" varchar(255) NOT NULL default ' ',
  "AAAAHIMN" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHIMU","AAABJOWZ","AAAAHIUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAAAHIMU" varchar(6) NOT NULL default ' ',
  "AAAAHIVP" varchar(6) NOT NULL default ' ',
  "AAAAHIMW" varchar(4) NOT NULL default ' ',
  "AAAAHIMX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHINA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISK","AAABJOWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHINH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAABLSMW" varchar(6) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  "AAAAHINS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHINV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISK","AAAAHIUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAAAHITJ" varchar(1) NOT NULL default '0',
  "AAAAHITF" varchar(3) NOT NULL default ' ',
  "AAAAHIOH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAHISK","AAAAHITJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  "AAABKWJH" varchar(250) NOT NULL default ' ',
  "AAABKWJI" varchar(250) NOT NULL default ' ',
  "AAABKWJJ" varchar(250) NOT NULL default ' ',
  "AAABKWJK" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAAAHIPB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIVO" varchar(6) NOT NULL default '000000',
  "AAAAHIPI" varchar(6) NOT NULL default ' ',
  "AAAAHIVP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIVO" varchar(6) NOT NULL default '000000',
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIVO","AAAAHISK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHIVO" varchar(6) NOT NULL default '000000',
  "AAAAHIPX" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAHIVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISE" VARBINARY(255) NOT NULL,
  "AAABJOYS" VARBINARY(255),
  "AAABJPBK" varchar(8) NOT NULL default '00000000',
  "AAABJPBL" varchar(1) NOT NULL default ' ',
  "AAABJPBM" varchar(1) NOT NULL default ' ',
  "AAABJPBN" varchar(8) NOT NULL default '00000000',
  "AAABJPBO" varchar(1) NOT NULL default ' ',
  "AAABJPBP" varchar(1) NOT NULL default ' ',
  "AAABJOWX" varchar(1) NOT NULL default '0',
  "AAABJOWY" varchar(1) NOT NULL default '0',
  "AAAAHIUE" varchar(2) NOT NULL default '00',
  "AAABJOWZ" varchar(6) NOT NULL default ' ',
  "AAAAHIQP" varchar(1) NOT NULL default ' ',
  "AAABJOXG" varchar(1) NOT NULL default ' ',
  "AAAAHIQR" varchar(1) NOT NULL default ' ',
  "AAAAHIQS" decimal(17,2) NOT NULL default '0.00',
  "AAAAHIQT" varchar(7) NOT NULL default '0000000',
  "AAAAHIQU" varchar(6) NOT NULL default ' ',
  "AAAAHIQV" varchar(1) NOT NULL default ' ',
  "AAAAHIQW" varchar(2) NOT NULL default '00',
  "AAAAHIQX" varchar(1) NOT NULL default ' ',
  "AAAAHIQY" varchar(1) NOT NULL default ' ',
  "AAAAHIQZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISE"(255)),
  KEY "AAAAHIRC" ("AAABJPBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISE" VARBINARY(255) NOT NULL,
  "AAABJOZT" varchar(4) NOT NULL default '0000',
  "AAABJOZC" decimal(17,2) NOT NULL default '0.00',
  "AAABJOXB" varchar(4) NOT NULL default ' ',
  "AAABJOXC" varchar(2) NOT NULL default '00',
  "AAABJOZD" decimal(17,2) NOT NULL default '0.00',
  "AAABJOZE" decimal(17,2) NOT NULL default '0.00',
  "AAABJOZF" decimal(17,2) NOT NULL default '0.00',
  "AAAAHIRO" decimal(17,2) NOT NULL default '0.00',
  "AAAAHIRP" varchar(4) NOT NULL default '0000',
  "AAAAHIRQ" varchar(7) NOT NULL default '0000000',
  "AAABJOZG" decimal(4,0) NOT NULL default '0',
  "AAABJOZH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISE"(255),"AAABJOZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHISE" VARBINARY(255) NOT NULL,
  "AAABJOZT" varchar(4) NOT NULL default '0000',
  "AAABJOZU" varchar(4) NOT NULL default ' ',
  "AAABJOZI" decimal(10,7) NOT NULL default '0.0000000',
  "AAABJOZK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHISE"(255),"AAABJOZT","AAABJOZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHISH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOYS" VARBINARY(255) NOT NULL,
  "AAAAHISK" varchar(6) NOT NULL default ' ',
  "AAAAHISL" varchar(1) NOT NULL default ' ',
  "AAAAHISM" varchar(1) NOT NULL default ' ',
  "AAABJOUH" varchar(2) NOT NULL default '00',
  "AAAAHISO" varchar(5) NOT NULL default ' ',
  "AAABJOUJ" varchar(8) NOT NULL default '00000000',
  "AAABJPBR" varchar(12) NOT NULL default ' ',
  "AAABJPBS" varchar(8) NOT NULL default '00000000',
  "AAABJPBT" varchar(6) NOT NULL default '000000',
  "AAABJPBU" varchar(12) NOT NULL default ' ',
  "AAABJPBV" varchar(8) NOT NULL default '00000000',
  "AAABJPBW" varchar(6) NOT NULL default '000000',
  "AAABJPBX" varchar(12) NOT NULL default ' ',
  "AAABJPBY" varchar(8) NOT NULL default '00000000',
  "AAABJPBZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABJOYS"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHITB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJOYS" VARBINARY(255) NOT NULL,
  "AAAAHITJ" varchar(1) NOT NULL default '0',
  "AAAAHITF" varchar(3) NOT NULL default ' ',
  "AAAAHITG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJOYS"(255),"AAAAHITJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHITK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOYS" VARBINARY(255) NOT NULL,
  "AAABJOYP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJOYS"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHITS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERJ" VARBINARY(255) NOT NULL,
  "AAABJOYS" VARBINARY(255),
  "AAABJPBK" varchar(8) NOT NULL default '00000000',
  "AAABJPBL" varchar(1) NOT NULL default ' ',
  "AAABJPBM" varchar(1) NOT NULL default ' ',
  "AAABJPBN" varchar(8) NOT NULL default '00000000',
  "AAABJPBO" varchar(1) NOT NULL default ' ',
  "AAABJPBP" varchar(1) NOT NULL default ' ',
  "AAABJOWX" varchar(1) NOT NULL default '0',
  "AAABJOWY" varchar(1) NOT NULL default '0',
  "AAAAHIUE" varchar(2) NOT NULL default '00',
  "AAAAHIUF" varchar(6) NOT NULL default ' ',
  "AAAAHIUG" int(11) NOT NULL default '0',
  "AAAAHIUH" varchar(1) NOT NULL default ' ',
  "AAAAHIUI" varchar(2) NOT NULL default '00',
  "AAAAHIUJ" varchar(8) NOT NULL default '00000000',
  "AAAAHIUK" varchar(1) NOT NULL default ' ',
  "AAAAHIUL" varchar(1) NOT NULL default ' ',
  "AAAAHIUM" varchar(2) NOT NULL default '00',
  "AAAAHIUN" varchar(2) NOT NULL default ' ',
  "AAAAHIUO" varchar(1) NOT NULL default ' ',
  "AAAAHIUP" varchar(1) NOT NULL default ' ',
  "AAAAHIUQ" varchar(6) NOT NULL default ' ',
  "AAAAHIUR" varchar(1) NOT NULL default ' ',
  "AAAAHIUS" varchar(2) NOT NULL default ' ',
  "AAAAHIUT" varchar(2) NOT NULL default ' ',
  "AAAAHIUU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBERJ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBERJ" VARBINARY(255) NOT NULL,
  "AAABJOZT" varchar(4) NOT NULL default '0000',
  "AAAAHIVB" decimal(17,2) NOT NULL default '0.00',
  "AAAAHIVC" varchar(8) NOT NULL default '00000000',
  "AAAAHIVD" varchar(3) NOT NULL default '000',
  "AAAAHIVE" varchar(1) NOT NULL default '0',
  "AAAAHIVF" varchar(1) NOT NULL default '0',
  "AAAAHIVG" varchar(3) NOT NULL default '000',
  "AAAAHIVH" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBERJ"(255),"AAABJOZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIVZ" VARBINARY(255) NOT NULL,
  "AAAAHIVO" varchar(6) NOT NULL default '000000',
  "AAAAHIVP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAHIVZ"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHIVZ" VARBINARY(255) NOT NULL,
  "AAABJOYS" VARBINARY(255) NOT NULL,
  "AAABJPBK" varchar(8) NOT NULL default '00000000',
  "AAABJPBN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAHIVZ"(255),"AAABJOYS"(255))
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABHZGV" varchar(10) NOT NULL default '0000000000',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABIAKJ" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHZJH","AAABHZGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIWK" (
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
CREATE TABLE "AAAAHIWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIXJ" (
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
CREATE TABLE "AAAAHIYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAHIYZ" ("AAABMRIL","AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIZE" (
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
  UNIQUE KEY "AAAAHIZT" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAHIZW" ("AAABMRIL","AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHIZZ" (
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
  UNIQUE KEY "AAAAHJAO" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAHJAR" ("AAABMRIL","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJAU" (
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
CREATE TABLE "AAAAHJBI" (
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
  PRIMARY KEY ("AAABMRIL","AAABLLNN","AAABLLNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJBY" (
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
CREATE TABLE "AAAAHJDA" (
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
  UNIQUE KEY "AAAAHJDQ" ("AAABMRIL","AAABLLNL"),
  KEY "AAAAHJDT" ("AAABMRIL","AAABLLNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAHJEI" ("AAABMRIL","AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJEN" (
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
CREATE TABLE "AAAAHJFC" (
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
CREATE TABLE "AAAAHJFR" (
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
  KEY "AAAAHJGF" ("AAABMRIL","AAABLLNN"),
  KEY "AAAAHJGI" ("AAABMRIL","AAABLLNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAAAHJGX" ("AAABMRIL","AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJHC" (
  "AAAANAFH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAANAFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJHF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAANAFH" varchar(30) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAANAFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJHL" (
  "AAABLLIO" varchar(32) NOT NULL default ' ',
  "AAABLUNJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKNCF" varchar(63) NOT NULL default ' ',
  "AAAAHJHT" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKNCF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLLIO" varchar(32) NOT NULL default ' ',
  "AAABLUNJ" varchar(70) NOT NULL default ' ',
  "AAABMLPO" varchar(5) NOT NULL default '00000',
  "AAAAHJIC" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAABIGLE" varchar(8) NOT NULL default '00000000',
  "AAABIGLF" varchar(6) NOT NULL default '000000',
  "AAAANCMR" varchar(10) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  "AAABLWEW" varchar(72) NOT NULL default ' ',
  "AAAAHJIO" varchar(22) NOT NULL default ' ',
  "AAAAHJIP" varchar(50) NOT NULL default ' ',
  "AAAAHJIQ" varchar(50) NOT NULL default ' ',
  "AAAAHJIR" varchar(100) NOT NULL default ' ',
  "AAABLEBL" varchar(12) NOT NULL default ' ',
  "AAAAHJIT" varchar(70) NOT NULL default ' ',
  "AAAAHJJI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJIX" (
  "AAAAHJJI" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAHJJI","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJJC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAHJJI" varchar(10) NOT NULL default ' ',
  "AAABMRFO" varchar(5) NOT NULL default '00000',
  "AAAAWOHN" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAHJJI","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJJK" (
  "AAABLLIO" varchar(32) NOT NULL default ' ',
  "AAABLUNJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABLUNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABFIGO" varchar(6) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAABEPCO","AAABFIGO","AAABMLDW","AAABMLDY","AAABLXDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJKO" (
  "AAAAHJLW" varchar(30) NOT NULL default ' ',
  "AAAAHJLX" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAAAHJKT" varchar(50) NOT NULL default ' ',
  "AAAARZJL" varchar(30) NOT NULL default ' ',
  "AAAAHJKV" varchar(30) NOT NULL default ' ',
  "AAAAHJKW" varchar(30) NOT NULL default ' ',
  "AAABKGFE" varchar(1) NOT NULL default ' ',
  "AAAAHJKY" varchar(6) NOT NULL default '000000',
  "AAAAVGIZ" varchar(1) NOT NULL default ' ',
  "AAAAHJLA" varchar(1) NOT NULL default ' ',
  "AAABIRTO" varchar(1) NOT NULL default ' ',
  "AAAAHJLC" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(1) NOT NULL default ' ',
  "AAABIJZP" varchar(1) NOT NULL default ' ',
  "AAAAHJLF" varchar(1) NOT NULL default ' ',
  "AAAALGUD" varchar(1) NOT NULL default ' ',
  "AAAAHJLH" varchar(50) NOT NULL default ' ',
  "AAAAHJLI" varchar(30) NOT NULL default ' ',
  "AAAAHJLJ" varchar(1) NOT NULL default ' ',
  "AAAAHJLK" varchar(1) NOT NULL default ' ',
  "AAAAHJLL" varchar(30) NOT NULL default ' ',
  "AAAAHJLM" varchar(1) NOT NULL default ' ',
  "AAAAHJLN" varchar(50) NOT NULL default ' ',
  "AAAAHJLO" varchar(1) NOT NULL default ' ',
  "AAAAHJLP" varchar(4) NOT NULL default '0000',
  "AAAAHJLQ" varchar(30) NOT NULL default ' ',
  "AAAAHJLR" varchar(30) NOT NULL default ' ',
  "AAAAHJLS" varchar(32) NOT NULL default ' ',
  "AAAAHJLT" varchar(30) NOT NULL default ' ',
  "AAAAHJLU" varchar(30) NOT NULL default ' ',
  "AAAAHJLV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAHJLW","AAAAHJLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMDBA" varchar(10) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPWO" varchar(3) NOT NULL default '000',
  "AAABLQGL" varchar(10) NOT NULL default ' ',
  "AAABMCPO" varchar(10) NOT NULL default ' ',
  "AAAAHJMG" varchar(10) NOT NULL default ' ',
  "AAAAHJMH" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMDBA","AAABMDBB","AAABMPWO","AAABLQGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAHJMQ" varchar(1) NOT NULL default ' ',
  "AAAAHJMR" varchar(1) NOT NULL default ' ',
  "AAAAHJMS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABKNGX" varchar(20) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABKNGX","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJNE" (
  "AAABKNGX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJNH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKNGX" varchar(20) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNXT" decimal(15,0) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABEOCZ" varchar(20) NOT NULL default ' ',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAAALNXW" decimal(15,0) NOT NULL default '0',
  "AAABEOCI" varchar(1) NOT NULL default '0',
  "AAABEOCJ" varchar(20) NOT NULL default ' ',
  "AAAAHJOA" decimal(15,0) NOT NULL default '0',
  "AAABEOCK" varchar(20) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABKJPP" varchar(10) NOT NULL default '0000000000',
  "AAAALNWJ" varchar(50) NOT NULL default ' ',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  "AAAALNWL" decimal(9,5) NOT NULL default '0.00000',
  "AAABMKAH" decimal(13,3) NOT NULL default '0.000',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  "AAAALNWO" varchar(4) NOT NULL default ' ',
  "AAAALNWP" VARBINARY(255),
  "AAAALNWQ" varchar(5) NOT NULL default '00000',
  "AAABMKKY" varchar(8) NOT NULL default '00000000',
  "AAAAHJPF" varchar(8) NOT NULL default '00000000',
  "AAAAHJPG" varchar(8) NOT NULL default '00000000',
  "AAAAHKVB" varchar(8) NOT NULL default '00000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMPWN" varchar(8) NOT NULL default '00000000',
  "AAABMCMW" varchar(8) NOT NULL default '00000000',
  "AAABMCRU" varchar(8) NOT NULL default '00000000',
  "AAAAHJPM" varchar(8) NOT NULL default '00000000',
  "AAABDGZI" varchar(8) NOT NULL default '00000000',
  "AAAAHJPO" varchar(8) NOT NULL default '00000000',
  "AAAAHJPP" varchar(8) NOT NULL default '00000000',
  "AAAAHJPQ" varchar(8) NOT NULL default '00000000',
  "AAAAHJPR" varchar(8) NOT NULL default '00000000',
  "AAAAHJPS" varchar(4) NOT NULL default '0000',
  "AAAAHJPT" varchar(2) NOT NULL default '00',
  "AAAAHJPU" varchar(8) NOT NULL default '00000000',
  "AAAAHJPV" varchar(8) NOT NULL default '00000000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCNG" decimal(9,5) NOT NULL default '0.00000',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAABIAWZ" decimal(13,2) NOT NULL default '0.00',
  "AAABFKTJ" decimal(16,13) NOT NULL default '0.0000000000000',
  "AAABMPTM" decimal(13,3) NOT NULL default '0.000',
  "AAABMCVK" varchar(3) NOT NULL default ' ',
  "AAAAHJQF" varchar(1) NOT NULL default ' ',
  "AAAAHJQG" varchar(1) NOT NULL default ' ',
  "AAABMDGC" varchar(18) NOT NULL default ' ',
  "AAAAVTYK" varchar(10) NOT NULL default ' ',
  "AAAALNXT" decimal(15,0) NOT NULL default '0',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABEOEC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  "AAAAHJRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABLUNJ"(255),"AAABMMVO","AAABMREX","AAABMQCD","AAABKJVG","AAABMQCE","AAABMCOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAABMQVT" varchar(12) NOT NULL default ' ',
  "AAABMQVU" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABIENQ" varchar(20) NOT NULL default ' ',
  "AAAAHJSF" decimal(15,0) NOT NULL default '0',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABKJVG" varchar(10) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMCOP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAABMQVT","AAABMQVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAABDNQK" varchar(30) NOT NULL default ' ',
  "AAABLFWZ" varchar(200) NOT NULL default ' ',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAABDNQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAAAHJTO" varchar(20) NOT NULL default ' ',
  "AAAAHJTP" varchar(20) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNYH","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  "AAAAHJUG" varchar(1) NOT NULL default '0',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUNJ" VARBINARY(255) NOT NULL,
  "AAABMMVO" varchar(5) NOT NULL default '00000',
  "AAAALNYH" decimal(15,0) NOT NULL default '0',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABMQCB" varchar(6) NOT NULL default '000000',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABLUNJ"(255),"AAABMMVO","AAAALNYH","AAABMQCA","AAABMQCB","AAABEOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABLDKQ" varchar(22) NOT NULL default ' ',
  "AAAAHJVO" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMMVH","AAABHMRN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJLTX" varchar(10) NOT NULL default ' ',
  "AAABKJPP" varchar(10) NOT NULL default '0000000000',
  "AAAAPIGD" VARBINARY(255) NOT NULL,
  "AAABIMHK" VARBINARY(255) NOT NULL,
  "AAAAHJWA" varchar(1) NOT NULL default ' ',
  "AAAAHJWB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJLTX","AAABKJPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJWG" (
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
  "AAABMQCO" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCWP" varchar(4) NOT NULL default ' ',
  "AAABMCWQ" varchar(3) NOT NULL default '000',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  "AAAAZOJS" varchar(6) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAVULE" varchar(6) NOT NULL default ' ',
  "AAAAVULF" varchar(3) NOT NULL default '000',
  "AAABMPVO" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVP" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVQ" decimal(17,2) NOT NULL default '0.00',
  "AAABMPVR" decimal(15,3) NOT NULL default '0.000',
  "AAABMPVS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABMCMZ" varchar(16) NOT NULL default ' ',
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
  "AAAAVUMA" varchar(10) NOT NULL default ' ',
  "AAAAVUMB" varchar(10) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABMQCE" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHJYP" ("AAABMQCJ","AAABMQCL","AAABMQHV","AAABMQHY","AAABETQU","AAAAVUNB","AAABMQCO","AAABMQHU"),
  KEY "AAAAHJYY" ("AAABETQU","AAABMQHZ","AAABMQHY","AAABMQHX","AAABMQHV"),
  KEY "AAAAHJZE" ("AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAAAVUNB","AAABMQHV","AAABMQHU","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHJZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCWP" varchar(4) NOT NULL default ' ',
  "AAABMCWQ" varchar(3) NOT NULL default '000',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHKAC" ("AAABEIVN","AAABMQBR","AAABEPWC","AAABJKTY","AAABMCWP","AAABMCWQ","AAABMRIL","AAABBPPO","AAABEPWM","AAABEPWN","AAABEPWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTG" varchar(18) NOT NULL default ' ',
  "AAABGPZJ" varchar(8) NOT NULL default '00000000',
  "AAABLGWN" varchar(8) NOT NULL default '00000000',
  "AAABGPZL" varchar(8) NOT NULL default '00000000',
  "AAABLGWT" varchar(8) NOT NULL default '00000000',
  "AAABEZHC" varchar(6) NOT NULL default ' ',
  "AAABGPZS" varchar(10) NOT NULL default ' ',
  "AAAAKOVZ" varchar(4) NOT NULL default ' ',
  "AAAAIPMH" varchar(6) NOT NULL default ' ',
  "AAAAIPKZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTG"),
  UNIQUE KEY "AAAAHKBC" ("AAABEZHC","AAABGPZS","AAAAKOVZ","AAAAIPMH","AAAAIPKZ","AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKBJ" (
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
  "AAABMQCO" varchar(3) NOT NULL default ' ',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCWP" varchar(4) NOT NULL default ' ',
  "AAABMCWQ" varchar(3) NOT NULL default '000',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  "AAAAZOJS" varchar(6) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAVULE" varchar(6) NOT NULL default ' ',
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
  "AAABMQGN" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAAAVULN" varchar(1) NOT NULL default ' ',
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
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQCG"),
  UNIQUE KEY "AAAAHKGQ" ("AAABMQCJ","AAABMQCL","AAABMQHV","AAABMQHY","AAABETQU","AAAAVUNB","AAABMQCO","AAABMQHS","AAABMQHU"),
  KEY "AAAAHKHA" ("AAABMQHZ","AAABMQHW","AAABMQHX","AAABMQHY","AAABETQU","AAABIBNO","AAABBPPK","AAABEIVN","AAABEPWC","AAABJKTY","AAABMQDW","AAABMQHU","AAABMQHV"),
  KEY "AAAAHKHO" ("AAABMQCV","AAABMQCW","AAABMQCX","AAABETQU","AAABMQHV","AAAAVUNB","AAABMQHU","AAABMQDA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKHX" (
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
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABIBNO" varchar(4) NOT NULL default ' ',
  "AAABBPPK" varchar(6) NOT NULL default ' ',
  "AAAAVUNB" varchar(4) NOT NULL default ' ',
  "AAABEIVN" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABMCWP" varchar(4) NOT NULL default ' ',
  "AAABMCWQ" varchar(3) NOT NULL default '000',
  "AAABBPPO" varchar(3) NOT NULL default '000',
  "AAABEPWM" varchar(10) NOT NULL default ' ',
  "AAABEPWN" varchar(10) NOT NULL default ' ',
  "AAABEPWO" varchar(10) NOT NULL default ' ',
  "AAAAZOJS" varchar(6) NOT NULL default ' ',
  "AAABMQDW" varchar(10) NOT NULL default ' ',
  "AAABIBHP" varchar(4) NOT NULL default ' ',
  "AAAAVULE" varchar(6) NOT NULL default ' ',
  "AAAAVULF" varchar(3) NOT NULL default '000',
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
  "AAABMQGN" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGO" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGP" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGQ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGR" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGS" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGT" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGU" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGV" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGW" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGX" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGY" decimal(15,3) NOT NULL default '0.000',
  "AAABMQGZ" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHA" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHB" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHC" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHD" decimal(15,3) NOT NULL default '0.000',
  "AAABMQHE" varchar(3) NOT NULL default ' ',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQHU","AAABMQHV","AAABMQHW","AAABMQHX","AAABMQHY","AAABMQHM","AAABMQHN","AAABMQHO","AAABMQHP","AAABMQHQ","AAABMQHR","AAABMQHS"),
  KEY "AAAAHKNA" ("AAABETQU","AAABMQHZ","AAABMQHY","AAABMQHX","AAABMQHV"),
  KEY "AAAAHKNG" ("AAABETQU","AAABMQHY","AAABMQHX","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLEJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  "AAAARERZ" varchar(60) NOT NULL default ' ',
  "AAAATRUT" varchar(3) NOT NULL default ' ',
  "AAAAHKNY" varchar(2) NOT NULL default ' ',
  "AAAAPJAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAARFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKNU" (
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  "AAAARERZ" varchar(60) NOT NULL default ' ',
  "AAAATRUT" varchar(3) NOT NULL default ' ',
  "AAAAHKNY" varchar(2) NOT NULL default ' ',
  "AAAAPJAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAARFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABAUSZ" (
  "AAAAPJAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPJAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAPJAQ" (
  "AAAAPJAQ" varchar(10) NOT NULL default ' ',
  "AAAAHKOG" tinyint(3) unsigned NOT NULL default '0',
  "AAAARWHC" varchar(1) NOT NULL default ' ',
  "AAAAHKOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAPJAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABJENC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFZK","AAAARFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKOS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAARFLY" varchar(60) NOT NULL default ' ',
  "AAABLEJW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAARFLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABMKWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAAAHKPB" varchar(4) NOT NULL default ' ',
  "AAABKNGX" varchar(12) NOT NULL default ' ',
  "AAABLILS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAAAHKQD" varchar(10) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABIMRZ" varchar(10) NOT NULL default ' ',
  "AAABIAET" varchar(5) NOT NULL default '00000',
  "AAAAHKPP" varchar(3) NOT NULL default ' ',
  "AAAAHKPQ" varchar(2) NOT NULL default ' ',
  "AAAAHKPR" varchar(2) NOT NULL default ' ',
  "AAAAHKPS" varchar(10) NOT NULL default ' ',
  "AAAAHKPT" varchar(3) NOT NULL default ' ',
  "AAAAHKPU" varchar(4) NOT NULL default ' ',
  "AAABJGGK" varchar(3) NOT NULL default ' ',
  "AAABLSOG" varchar(4) NOT NULL default ' ',
  "AAAAQAPW" varchar(1) NOT NULL default ' ',
  "AAAAHKPY" varchar(40) NOT NULL default ' ',
  "AAABBKLX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITJY","AAABMODI","AAAAHKQD","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABITJY" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAHKQJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABITJY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKQN" (
  "AAABGPJN" varchar(10) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKQT" (
  "AAABGPJN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABGPJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAATOXC" varchar(8) NOT NULL default '00000000',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLTVR","AAAATOXC","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABCDOA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAAATOXC" varchar(8) NOT NULL default '00000000',
  "AAABIJRB" varchar(30) NOT NULL default ' ',
  "AAAAHKRX" varchar(4) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAAATOXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTVR" varchar(32) NOT NULL default ' ',
  "AAABLLWR" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTVR","AAABLLWR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAHKSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
