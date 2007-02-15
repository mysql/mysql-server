# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl38.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABFGEP" (
  "AAABMIRX" varchar(12) NOT NULL default ' ',
  "AAABFGEU" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABFGEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGEV" (
  "AAABMIRX" varchar(12) NOT NULL default ' ',
  "AAABFGFA" varchar(15) NOT NULL default ' ',
  "AAABLTIX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRX","AAABFGFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEXH" varchar(4) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEXH" varchar(4) NOT NULL default ' ',
  "AAABFGFL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKEXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEVK" varchar(12) NOT NULL default ' ',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEVK","AAABKFER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKEVK" varchar(12) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKEQN" varchar(3) NOT NULL default '000',
  "AAABKEPY" varchar(2) NOT NULL default '00',
  "AAABKEQR" varchar(1) NOT NULL default '0',
  "AAABKEQA" varchar(3) NOT NULL default '000',
  "AAABKHMU" varchar(2) NOT NULL default '00',
  "AAABKEQC" varchar(3) NOT NULL default '000',
  "AAABKEQD" varchar(1) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAABKEQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKEVK","AAABKHOL","AAABKEQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKEVK" varchar(12) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKEQN" varchar(3) NOT NULL default '000',
  "AAABKEPY" varchar(2) NOT NULL default '00',
  "AAABKEQR" varchar(1) NOT NULL default '0',
  "AAABKEQA" varchar(3) NOT NULL default '000',
  "AAABKHMU" varchar(2) NOT NULL default '00',
  "AAABKEQC" varchar(3) NOT NULL default '000',
  "AAABKEQD" varchar(1) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAABKEQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKEVK","AAABKHOL","AAABKEQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEVK" varchar(12) NOT NULL default ' ',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKEQN" varchar(3) NOT NULL default '000',
  "AAABKEQO" varchar(2) NOT NULL default '00',
  "AAABKEQP" varchar(2) NOT NULL default '00',
  "AAABKEQQ" varchar(4) NOT NULL default '0000',
  "AAABKEQR" varchar(1) NOT NULL default '0',
  "AAABKEQS" varchar(2) NOT NULL default '00',
  "AAABKEQT" varchar(2) NOT NULL default '00',
  "AAABKEQU" varchar(4) NOT NULL default '0000',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEVK","AAABKFER","AAABKHOL","AAABKEQN","AAABKEQO","AAABKEQP","AAABKEQQ","AAABKEQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEVK" varchar(12) NOT NULL default ' ',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKEQN" varchar(3) NOT NULL default '000',
  "AAABKEQO" varchar(2) NOT NULL default '00',
  "AAABKEQP" varchar(2) NOT NULL default '00',
  "AAABKEQQ" varchar(4) NOT NULL default '0000',
  "AAABKEQR" varchar(1) NOT NULL default '0',
  "AAABKEQS" varchar(2) NOT NULL default '00',
  "AAABKEQT" varchar(2) NOT NULL default '00',
  "AAABKEQU" varchar(4) NOT NULL default '0000',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEVK","AAABKFER","AAABKHOL","AAABKEQN","AAABKEQO","AAABKEQP","AAABKEQQ","AAABKEQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGJI" (
  "AAABFGJR" varchar(1) NOT NULL default ' ',
  "AAABFGJS" varchar(20) NOT NULL default ' ',
  "AAABFGJT" varchar(1) NOT NULL default ' ',
  "AAABHMRI" varchar(14) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFGJW" varchar(2) NOT NULL default ' ',
  "AAABFGJP" int(11) NOT NULL default '0',
  "AAABFGJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFGJR","AAABFGJS","AAABFGJT","AAABHMRI","AAABMREF","AAABFGJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCLM" varchar(4) NOT NULL default '0000',
  "AAABKCJP" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKCLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVHI" varchar(35) NOT NULL default ' ',
  "AAABKCHT" decimal(5,2) NOT NULL default '0.00',
  "AAABFGKH" decimal(5,2) NOT NULL default '0.00',
  "AAABKCHZ" decimal(5,2) NOT NULL default '0.00',
  "AAABKCHV" varchar(10) NOT NULL default ' ',
  "AAABKCHU" varchar(60) NOT NULL default ' ',
  "AAABFGKL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLVHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCHY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCHY" varchar(2) NOT NULL default ' ',
  "AAABFGKX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCLM" varchar(4) NOT NULL default '0000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKCIQ" varchar(8) NOT NULL default '00000000',
  "AAABKCIV" varchar(8) NOT NULL default '00000000',
  "AAABFGLH" decimal(5,2) NOT NULL default '0.00',
  "AAABFGLI" decimal(13,2) NOT NULL default '0.00',
  "AAABFGOX" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKCLM","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCIC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCIC" varchar(2) NOT NULL default ' ',
  "AAABFGLW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCHW" varchar(4) NOT NULL default ' ',
  "AAABFGMD" varchar(2) NOT NULL default ' ',
  "AAABKCHX" decimal(5,2) NOT NULL default '0.00',
  "AAABKCIA" decimal(5,2) NOT NULL default '0.00',
  "AAABKCIP" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKCHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCHW" varchar(4) NOT NULL default ' ',
  "AAABFGMN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCKF" varchar(2) NOT NULL default ' ',
  "AAABKCLD" varchar(1) NOT NULL default ' ',
  "AAABFGMV" varchar(1) NOT NULL default ' ',
  "AAABFGMW" varchar(1) NOT NULL default ' ',
  "AAABKCKP" decimal(5,2) NOT NULL default '0.00',
  "AAABKCKS" decimal(5,2) NOT NULL default '0.00',
  "AAABKCKQ" decimal(5,2) NOT NULL default '0.00',
  "AAABKCKR" decimal(5,2) NOT NULL default '0.00',
  "AAABFGNB" varchar(1) NOT NULL default ' ',
  "AAABFGNC" varchar(1) NOT NULL default '0',
  "AAABFGND" varchar(1) NOT NULL default ' ',
  "AAABFGNE" varchar(1) NOT NULL default ' ',
  "AAABFGNF" varchar(4) NOT NULL default '0000',
  "AAABFGNG" varchar(4) NOT NULL default '0000',
  "AAABFGNH" varchar(4) NOT NULL default '0000',
  "AAABFGNI" varchar(4) NOT NULL default '0000',
  "AAABFGNJ" varchar(4) NOT NULL default '0000',
  "AAABKCLC" varchar(4) NOT NULL default ' ',
  "AAABKCLH" varchar(4) NOT NULL default ' ',
  "AAABKCLI" varchar(4) NOT NULL default ' ',
  "AAABKCKX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKCKF","AAABKCLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKCKF" varchar(2) NOT NULL default ' ',
  "AAABKCLD" varchar(1) NOT NULL default ' ',
  "AAABFGNW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKCKF","AAABKCLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCLC" varchar(4) NOT NULL default ' ',
  "AAABFGOE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCLW" varchar(3) NOT NULL default ' ',
  "AAABKCLV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKCLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKCJF" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFGOR" decimal(13,2) NOT NULL default '0.00',
  "AAABFGOS" decimal(13,2) NOT NULL default '0.00',
  "AAABKCKU" decimal(13,2) NOT NULL default '0.00',
  "AAABFGOU" decimal(13,2) NOT NULL default '0.00',
  "AAABKCKX" decimal(5,2) NOT NULL default '0.00',
  "AAABKCKT" decimal(5,2) NOT NULL default '0.00',
  "AAABFGOX" decimal(13,2) NOT NULL default '0.00',
  "AAABFGOY" varchar(3) NOT NULL default '000',
  "AAABFGOZ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKCJF","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABKDLQ" varchar(10) NOT NULL default ' ',
  "AAABIYZY" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIFN","AAABKDLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFER" varchar(5) NOT NULL default ' ',
  "AAABKHNS" varchar(1) NOT NULL default ' ',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKFER","AAABKHNS","AAABKIFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABLCWL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKIFN","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDIN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKDIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDIN" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFGQL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDIN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDIO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKDIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDIO" varchar(2) NOT NULL default '00',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFGQY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDIO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGRC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABKEWA" varchar(4) NOT NULL default ' ',
  "AAABKDIQ" varchar(2) NOT NULL default '00',
  "AAABIFNA" varchar(4) NOT NULL default '0000',
  "AAABFGRI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKIFN","AAABKEWA","AAABKDIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEWA" varchar(4) NOT NULL default ' ',
  "AAABLCKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEWA" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFGRX" varchar(50) NOT NULL default ' ',
  "AAABFGRY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEWA","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGSC" (
  "AAABFGSI" varchar(8) NOT NULL default ' ',
  "AAABFGSE" varchar(30) NOT NULL default ' ',
  "AAABFGSF" varchar(1) NOT NULL default ' ',
  "AAABFGSG" varchar(1) NOT NULL default ' ',
  "AAABMLTT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABFGSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKIFN" varchar(3) NOT NULL default '000',
  "AAABKDLQ" varchar(10) NOT NULL default ' ',
  "AAABFGSO" varchar(30) NOT NULL default ' ',
  "AAABFGSP" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKIFN","AAABKDLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGTC" varchar(5) NOT NULL default ' ',
  "AAABFGTD" varchar(4) NOT NULL default '0000',
  "AAABFGSY" varchar(5) NOT NULL default ' ',
  "AAABFGSZ" varchar(4) NOT NULL default '0000',
  "AAABFGTA" decimal(10,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABFGTC","AAABFGTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHNRX" varchar(2) NOT NULL default ' ',
  "AAABFGTP" varchar(5) NOT NULL default ' ',
  "AAABFGTQ" varchar(10) NOT NULL default ' ',
  "AAABFGTK" varchar(5) NOT NULL default ' ',
  "AAABFGTL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHNRX","AAABFGTP","AAABFGTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGTW" varchar(1) NOT NULL default ' ',
  "AAABFGTU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKABU" decimal(13,2) NOT NULL default '0.00',
  "AAABKABQ" varchar(1) NOT NULL default ' ',
  "AAABJVXY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKHFZ","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKHFZ" varchar(4) NOT NULL default '0000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABKABV" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKHFZ","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGVA" (
  "AAABMOCC" varchar(6) NOT NULL default ' ',
  "AAABFGVC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGVE" (
  "AAABMOCC" varchar(6) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFGVH" varchar(30) NOT NULL default ' ',
  "AAABFGVI" varchar(15) NOT NULL default ' ',
  "AAABFGVJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOCC","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHNM" varchar(6) NOT NULL default ' ',
  "AAABFGVZ" varchar(4) NOT NULL default '0000',
  "AAABFGWA" varchar(3) NOT NULL default '000',
  "AAABMKYE" varchar(4) NOT NULL default ' ',
  "AAABMMRE" varchar(65) NOT NULL default ' ',
  "AAABFGVT" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(2) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKHNM","AAABFGVZ","AAABFGWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEWC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEWC" varchar(4) NOT NULL default ' ',
  "AAABFGWK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKEWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEWD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEWD" varchar(4) NOT NULL default ' ',
  "AAABFGWX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKEWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGXN" varchar(4) NOT NULL default ' ',
  "AAABFGXK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFGXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFGXT" varchar(3) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFGXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABKKAM" varchar(10) NOT NULL default ' ',
  "AAABFGXY" varchar(1) NOT NULL default ' ',
  "AAABFGXZ" varchar(15) NOT NULL default ' ',
  "AAABFGYA" varchar(1) NOT NULL default ' ',
  "AAABFGYB" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJFL","AAABKKAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFHPN" varchar(4) NOT NULL default ' ',
  "AAABFGYI" varchar(1) NOT NULL default ' ',
  "AAABFGYJ" varchar(1) NOT NULL default ' ',
  "AAABFGYK" varchar(1) NOT NULL default ' ',
  "AAABFGYL" varchar(1) NOT NULL default ' ',
  "AAABFGYM" varchar(1) NOT NULL default ' ',
  "AAABFGYN" varchar(1) NOT NULL default ' ',
  "AAABFGYO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFHPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHPN" varchar(4) NOT NULL default ' ',
  "AAABFGYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFHPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGMM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGMM" varchar(15) NOT NULL default ' ',
  "AAABFGZI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGZM" (
  "AAABKINQ" varchar(4) NOT NULL default ' ',
  "AAABFGZO" varchar(4) NOT NULL default ' ',
  "AAABFGZP" varchar(4) NOT NULL default ' ',
  "AAABFGZQ" varchar(4) NOT NULL default ' ',
  "AAABFGZR" varchar(30) NOT NULL default ' ',
  "AAABFGZS" varchar(30) NOT NULL default ' ',
  "AAABFGZT" varchar(30) NOT NULL default ' ',
  "AAABFGZU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKINQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFGZW" (
  "AAABKINQ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFGZZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKINQ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKAM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKAM" varchar(10) NOT NULL default ' ',
  "AAABFHAL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFL" varchar(4) NOT NULL default ' ',
  "AAABFHAS" varchar(4) NOT NULL default ' ',
  "AAABFHAT" int(11) NOT NULL default '0',
  "AAABFHAU" int(11) NOT NULL default '0',
  "AAABFHAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJFL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHAY" (
  "AAABFHBG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHBB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHBG" varchar(4) NOT NULL default ' ',
  "AAABFHBE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFHBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHBH" (
  "AAABFHBP" varchar(4) NOT NULL default ' ',
  "AAABFHBJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHBL" (
  "AAABFHBP" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHBO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHBP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHBR" (
  "AAABFHBY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHBU" (
  "AAABFHBY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHBX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHBY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKAO" varchar(20) NOT NULL default ' ',
  "AAABFHCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKAO" varchar(20) NOT NULL default ' ',
  "AAABFHCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEHK","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABFHDA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLEHK","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKHOL" varchar(4) NOT NULL default ' ',
  "AAABKEWB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKHOL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVD" varchar(1) NOT NULL default ' ',
  "AAABFHFI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVE" varchar(1) NOT NULL default ' ',
  "AAABFHFQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVG" varchar(1) NOT NULL default ' ',
  "AAABFHFY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVH" varchar(1) NOT NULL default ' ',
  "AAABFHGG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVI" varchar(1) NOT NULL default ' ',
  "AAABFHGO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVJ" varchar(1) NOT NULL default ' ',
  "AAABFHGW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVK" varchar(1) NOT NULL default ' ',
  "AAABFHHE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVL" varchar(1) NOT NULL default ' ',
  "AAABFHHM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVM" varchar(1) NOT NULL default ' ',
  "AAABFHHU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKGVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFHIX" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFHIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGVN" varchar(1) NOT NULL default ' ',
  "AAABFHIM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKGVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHIX" varchar(1) NOT NULL default '0',
  "AAABFHIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFHIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJZUL" varchar(2) NOT NULL default ' ',
  "AAABFHJB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJZUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJZUL" varchar(2) NOT NULL default ' ',
  "AAABJZUM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJZUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHJM" (
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGU" varchar(3) NOT NULL default '000',
  "AAABFHJQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHJRW","AAABMPMH","AAABFIGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHJU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGU" varchar(3) NOT NULL default '000',
  "AAABFHJZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHJRW","AAABMPMH","AAABFIGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGU" varchar(3) NOT NULL default '000',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHJRW","AAABMPMH","AAABFIGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIDK" varchar(3) NOT NULL default ' ',
  "AAABFHKU" varchar(1) NOT NULL default ' ',
  "AAABFHKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABFHLC" tinyint(3) unsigned NOT NULL default '0',
  "AAABKIYF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVLB" varchar(10) NOT NULL default ' ',
  "AAABFHLJ" int(11) NOT NULL default '0',
  "AAABFHLK" int(11) NOT NULL default '0',
  "AAABFHLL" int(11) NOT NULL default '0',
  "AAABFHLM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABJWBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKJFK","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHMD" (
  "AAABKIVS" varchar(4) NOT NULL default ' ',
  "AAABFHMI" varchar(4) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIVS","AAABFHMI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHMJ" (
  "AAABFHMS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHMM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHMS" varchar(2) NOT NULL default ' ',
  "AAABFHMP" varchar(3) NOT NULL default ' ',
  "AAABFHMQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFHMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHMT" (
  "AAABMOIA" varchar(10) NOT NULL default ' ',
  "AAABFHMV" varchar(4) NOT NULL default ' ',
  "AAABLILS" varchar(4) NOT NULL default ' ',
  "AAABFHMX" varchar(4) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  "AAABFHMZ" varchar(4) NOT NULL default ' ',
  "AAABFHNA" tinyint(3) unsigned NOT NULL default '0',
  "AAABFHNB" varchar(3) NOT NULL default ' ',
  "AAABFHNC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHNE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(10) NOT NULL default ' ',
  "AAABFHNH" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHNK" (
  "AAABFHNS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHNN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFHNS" varchar(4) NOT NULL default ' ',
  "AAABFHNQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFHNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABKJFK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKIZL","AAABKJFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHOA" (
  "AAABKITV" varchar(1) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default ' ',
  "AAABFHOD" varchar(1) NOT NULL default ' ',
  "AAABFHOE" varchar(1) NOT NULL default ' ',
  "AAABFHOF" varchar(1) NOT NULL default ' ',
  "AAABFHOG" varchar(1) NOT NULL default ' ',
  "AAABFHOH" varchar(1) NOT NULL default ' ',
  "AAABFHOI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKITV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHOK" (
  "AAABKIZK" varchar(3) NOT NULL default ' ',
  "AAABLEJX" varchar(1) NOT NULL default ' ',
  "AAABFHON" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHOP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKIZK" varchar(3) NOT NULL default ' ',
  "AAABFHOS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKADS" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKADS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKADS" varchar(3) NOT NULL default '000',
  "AAABFHPE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKADS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFHPN" varchar(4) NOT NULL default ' ',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFHPN","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFHQG" varchar(1) NOT NULL default ' ',
  "AAABFHQH" varchar(1) NOT NULL default ' ',
  "AAABFHQI" varchar(1) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABFHQJ" varchar(1) NOT NULL default ' ',
  "AAABFHQK" varchar(1) NOT NULL default ' ',
  "AAABFHQL" varchar(15) NOT NULL default ' ',
  "AAABKILI" varchar(4) NOT NULL default ' ',
  "AAABKIYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFHQG" varchar(1) NOT NULL default ' ',
  "AAABFHQH" varchar(1) NOT NULL default ' ',
  "AAABFHQI" varchar(1) NOT NULL default ' ',
  "AAABFHQJ" varchar(1) NOT NULL default ' ',
  "AAABFHQK" varchar(1) NOT NULL default ' ',
  "AAABFHQL" varchar(15) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABKILI" varchar(4) NOT NULL default ' ',
  "AAABKIYK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKIZK" varchar(1) NOT NULL default ' ',
  "AAABKILT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ","AAABKIZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFHRD" varchar(30) NOT NULL default ' ',
  "AAABFHRE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHRI" (
  "AAABKIFH" varchar(2) NOT NULL default ' ',
  "AAABKIFG" varchar(3) NOT NULL default ' ',
  "AAABFHTO" varchar(3) NOT NULL default ' ',
  "AAABKIVS" varchar(4) NOT NULL default ' ',
  "AAABFHRN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIFH","AAABKIFG","AAABFHTO","AAABKIVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHRS" (
  "AAABKIFG" varchar(3) NOT NULL default ' ',
  "AAABFHRU" varchar(1) NOT NULL default ' ',
  "AAABFHRV" varchar(1) NOT NULL default ' ',
  "AAABFHTN" varchar(1) NOT NULL default ' ',
  "AAABFHRX" varchar(1) NOT NULL default ' ',
  "AAABFHRY" varchar(1) NOT NULL default ' ',
  "AAABFHRZ" varchar(1) NOT NULL default ' ',
  "AAABFHSA" varchar(1) NOT NULL default ' ',
  "AAABKJFL" varchar(1) NOT NULL default ' ',
  "AAABFHSC" varchar(1) NOT NULL default ' ',
  "AAABFHSD" varchar(1) NOT NULL default ' ',
  "AAABFHSE" varchar(1) NOT NULL default ' ',
  "AAABFHSF" varchar(1) NOT NULL default ' ',
  "AAABFHSG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKIFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJJG" varchar(4) NOT NULL default ' ',
  "AAABJCRH" varchar(1) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  "AAABFHXK" varchar(3) NOT NULL default ' ',
  "AAABFHSO" varchar(1) NOT NULL default ' ',
  "AAABFHXL" varchar(1) NOT NULL default ' ',
  "AAABFHXM" varchar(1) NOT NULL default ' ',
  "AAABKIFS" varchar(1) NOT NULL default ' ',
  "AAABKIFT" varchar(255) NOT NULL default ' ',
  "AAABKIFU" varchar(2) NOT NULL default ' ',
  "AAABKIFV" varchar(1) NOT NULL default ' ',
  "AAABKIFW" varchar(255) NOT NULL default ' ',
  "AAABFHSW" varchar(1) NOT NULL default ' ',
  "AAABFHSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJJG" varchar(4) NOT NULL default ' ',
  "AAABFHXU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHTI" (
  "AAABFHTO" varchar(3) NOT NULL default ' ',
  "AAABFHTK" varchar(1) NOT NULL default ' ',
  "AAABFHTL" varchar(1) NOT NULL default ' ',
  "AAABFHTM" varchar(1) NOT NULL default ' ',
  "AAABFHTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHTP" (
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABFHUH" tinyint(3) unsigned NOT NULL default '0',
  "AAABFHTS" varchar(1) NOT NULL default ' ',
  "AAABFHTT" varchar(1) NOT NULL default ' ',
  "AAABFHTU" varchar(1) NOT NULL default ' ',
  "AAABFHTV" varchar(1) NOT NULL default ' ',
  "AAABFHTW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHTY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHUE" (
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABJZBY" varchar(4) NOT NULL default ' ',
  "AAABFHUH" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABLTLR","AAABJZBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHUK" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABJZBY" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLTLR","AAABJZBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABKIZK" varchar(3) NOT NULL default ' ',
  "AAABKIZL" varchar(4) NOT NULL default ' ',
  "AAABKIYD" decimal(5,2) NOT NULL default '0.00',
  "AAABKIYF" varchar(10) NOT NULL default ' ',
  "AAABKIYG" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKLYJ","AAABKIZK","AAABKIZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABKIEH" varchar(10) NOT NULL default ' ',
  "AAABFHVI" varchar(10) NOT NULL default ' ',
  "AAABFHVJ" varchar(10) NOT NULL default ' ',
  "AAABFHVK" varchar(10) NOT NULL default ' ',
  "AAABFHVL" varchar(10) NOT NULL default ' ',
  "AAABFHVM" varchar(10) NOT NULL default ' ',
  "AAABFHVN" varchar(10) NOT NULL default ' ',
  "AAABFHVO" varchar(10) NOT NULL default ' ',
  "AAABFHVP" varchar(10) NOT NULL default ' ',
  "AAABFHVQ" varchar(10) NOT NULL default ' ',
  "AAABFHVR" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABKIEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  "AAABKHOF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFUKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHWD" (
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABKJNY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABKKDV","AAABKJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHWI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABKJNY" varchar(4) NOT NULL default ' ',
  "AAABFHWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKKDV","AAABKJNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJIF" varchar(4) NOT NULL default ' ',
  "AAABFHWT" varchar(1) NOT NULL default ' ',
  "AAABFHWU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJIF" varchar(4) NOT NULL default ' ',
  "AAABFHXB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJIF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJJG" varchar(4) NOT NULL default ' ',
  "AAABJCRH" varchar(1) NOT NULL default ' ',
  "AAABLNIW" varchar(1) NOT NULL default ' ',
  "AAABFHXK" varchar(3) NOT NULL default ' ',
  "AAABFHXL" varchar(1) NOT NULL default ' ',
  "AAABFHXM" varchar(1) NOT NULL default ' ',
  "AAABJVHL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJJG" varchar(4) NOT NULL default ' ',
  "AAABFHXU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJNA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJNA" varchar(4) NOT NULL default ' ',
  "AAABFHYH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJLR" varchar(4) NOT NULL default ' ',
  "AAABKJPC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJLR" varchar(4) NOT NULL default ' ',
  "AAABFHYV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHYZ" (
  "AAABFHZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFHZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKLYJ" varchar(4) NOT NULL default ' ',
  "AAABFHZG" varchar(30) NOT NULL default ' ',
  "AAABFHZH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKLYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMPTW" varchar(2) NOT NULL default ' ',
  "AAABFHZP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMEKQ","AAABMPTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFHZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKJAJ" varchar(8) NOT NULL default '00000000',
  "AAABMPWP" varchar(4) NOT NULL default '0000',
  "AAABFHZY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKJAJ","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJVW" varchar(4) NOT NULL default ' ',
  "AAABFIAG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJVW" varchar(4) NOT NULL default ' ',
  "AAABKJYI" varchar(12) NOT NULL default ' ',
  "AAABKJWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJVW","AAABKJYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJVW" varchar(4) NOT NULL default ' ',
  "AAABFIAV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJYI" varchar(12) NOT NULL default ' ',
  "AAABKJWH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJYI" varchar(12) NOT NULL default ' ',
  "AAABKJYJ" varchar(4) NOT NULL default '0000',
  "AAABKJXU" varchar(2) NOT NULL default '00',
  "AAABKJXV" varchar(2) NOT NULL default '00',
  "AAABKJXW" varchar(2) NOT NULL default '00',
  "AAABKJWZ" varchar(4) NOT NULL default '0000',
  "AAABKJXA" varchar(2) NOT NULL default '00',
  "AAABKJXB" varchar(2) NOT NULL default '00',
  "AAABKJXC" varchar(1) NOT NULL default '0',
  "AAABKJXD" varchar(2) NOT NULL default ' ',
  "AAABKJXE" varchar(4) NOT NULL default '0000',
  "AAABKJXF" varchar(2) NOT NULL default '00',
  "AAABKJXG" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKJYI","AAABKJYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKJYI" varchar(12) NOT NULL default ' ',
  "AAABKJYJ" varchar(4) NOT NULL default '0000',
  "AAABKJXU" varchar(2) NOT NULL default '00',
  "AAABKJXV" varchar(2) NOT NULL default '00',
  "AAABKJXW" varchar(2) NOT NULL default '00',
  "AAABKJXX" varchar(2) NOT NULL default ' ',
  "AAABKJXY" varchar(2) NOT NULL default '00',
  "AAABKJXZ" varchar(2) NOT NULL default '00',
  "AAABKJYA" varchar(2) NOT NULL default '00',
  "AAABKJYB" varchar(2) NOT NULL default '00',
  "AAABKJYC" varchar(1) NOT NULL default '0',
  "AAABKJYD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKJYI","AAABKJYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFICO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKJYI" varchar(12) NOT NULL default ' ',
  "AAABKJWJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKJYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFICW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKCK" varchar(4) NOT NULL default ' ',
  "AAABKKCL" varchar(1) NOT NULL default ' ',
  "AAABFIDA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKCK" varchar(4) NOT NULL default ' ',
  "AAABKKDL" varchar(1) NOT NULL default ' ',
  "AAABKKDM" varchar(4) NOT NULL default '0000',
  "AAABKKCX" varchar(2) NOT NULL default '00',
  "AAABKKCY" varchar(2) NOT NULL default '00',
  "AAABKKCZ" varchar(2) NOT NULL default '00',
  "AAABKKDA" varchar(2) NOT NULL default '00',
  "AAABKKDB" varchar(1) NOT NULL default ' ',
  "AAABKKDC" varchar(2) NOT NULL default '00',
  "AAABKKDD" varchar(2) NOT NULL default '00',
  "AAABKKDE" varchar(2) NOT NULL default '00',
  "AAABKKDF" varchar(2) NOT NULL default '00',
  "AAABKKDG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKCK","AAABKKDL","AAABKKDM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKCK" varchar(4) NOT NULL default ' ',
  "AAABFIEA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIEE" (
  "AAABFIEQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIEH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFIEQ" varchar(4) NOT NULL default ' ',
  "AAABFIEK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFIEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIEN" (
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABFIEQ" varchar(4) NOT NULL default ' ',
  "AAABLISJ" varchar(1) NOT NULL default ' ',
  "AAABFIES" varchar(1) NOT NULL default ' ',
  "AAABFIET" varchar(1) NOT NULL default ' ',
  "AAABFIEU" varchar(1) NOT NULL default ' ',
  "AAABFIEV" varchar(1) NOT NULL default ' ',
  "AAABFIEW" varchar(1) NOT NULL default ' ',
  "AAABFIEX" varchar(1) NOT NULL default ' ',
  "AAABFIEY" varchar(1) NOT NULL default ' ',
  "AAABFIEZ" varchar(1) NOT NULL default ' ',
  "AAABFIFA" varchar(30) NOT NULL default ' ',
  "AAABFIFB" varchar(30) NOT NULL default ' ',
  "AAABFIFC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPJD","AAABKKDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIFF" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABFIFJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLPJD","AAABKKDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABKKDW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABKKDV","AAABKKDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLPJD" varchar(2) NOT NULL default ' ',
  "AAABKKDV" varchar(4) NOT NULL default ' ',
  "AAABKKDW" varchar(4) NOT NULL default ' ',
  "AAABFIGC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLPJD","AAABKKDV","AAABKKDW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGU" varchar(3) NOT NULL default '000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABHJRW","AAABMPMH","AAABFIGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKEEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKEEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKEEH" varchar(1) NOT NULL default ' ',
  "AAABFIHE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKEEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKSF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKKSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKQEQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKSG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABKKSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKRR" varchar(3) NOT NULL default ' ',
  "AAABFIIK" decimal(9,3) NOT NULL default '0.000',
  "AAABFIIL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKKRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(1) NOT NULL default '0',
  "AAABKRHT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKSF" varchar(1) NOT NULL default '0',
  "AAABFIJA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKQEQ" varchar(1) NOT NULL default '0',
  "AAABKRHT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKQEQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKSG" varchar(3) NOT NULL default '000',
  "AAABFIJQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIDJL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIDJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(4) NOT NULL default ' ',
  "AAABIDJL" varchar(2) NOT NULL default '00',
  "AAABKKXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIDJL" varchar(2) NOT NULL default '00',
  "AAABFIKK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIDJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPWP" varchar(4) NOT NULL default ' ',
  "AAABFIKS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFILP" varchar(2) NOT NULL default '00',
  "AAABKKUQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFILP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFILC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKVF" varchar(4) NOT NULL default ' ',
  "AAABFILP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKKVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFILI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFILP" varchar(2) NOT NULL default '00',
  "AAABFILM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFILP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFILQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKVF" varchar(4) NOT NULL default ' ',
  "AAABFILU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFILY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIPJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIPJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFIMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIPJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKXW" varchar(8) NOT NULL default '00000000',
  "AAABKKVX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKSF" varchar(1) NOT NULL default '0',
  "AAABKKXW" varchar(8) NOT NULL default '00000000',
  "AAABKKVX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKKSF","AAABKKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIOG" varchar(2) NOT NULL default '00',
  "AAABKKSJ" varchar(1) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFIOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFING" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKXV" varchar(4) NOT NULL default ' ',
  "AAABFIOG" varchar(2) NOT NULL default '00',
  "AAABFINK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFINN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKKXW" varchar(8) NOT NULL default '00000000',
  "AAABKKXV" varchar(4) NOT NULL default ' ',
  "AAABMBIU" decimal(16,6) NOT NULL default '0.000000',
  "AAABKKVS" varchar(3) NOT NULL default ' ',
  "AAABKKVT" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMPWL","AAABKKXW","AAABKKXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFINZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFIOG" varchar(2) NOT NULL default '00',
  "AAABFIOD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFIOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKXV" varchar(4) NOT NULL default ' ',
  "AAABFIOL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKKXC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKKXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKXC" varchar(4) NOT NULL default ' ',
  "AAABFIOY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKKXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIPJ" varchar(1) NOT NULL default ' ',
  "AAABKYNY" varchar(250) NOT NULL default ' ',
  "AAABKYNZ" varchar(250) NOT NULL default ' ',
  "AAABKYOA" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIPJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIPK" (
  "AAABFIQF" varchar(2) NOT NULL default '00',
  "AAABFIQG" varchar(8) NOT NULL default ' ',
  "AAABFIQH" varchar(4) NOT NULL default '0000',
  "AAABFIPO" varchar(70) NOT NULL default ' ',
  "AAABLHAH" varchar(132) NOT NULL default ' ',
  "AAABFIQN" varchar(40) NOT NULL default ' ',
  "AAABFIPR" varchar(4) NOT NULL default ' ',
  "AAABFIPS" varchar(132) NOT NULL default ' ',
  "AAABFIPT" varchar(40) NOT NULL default ' ',
  "AAABFIPU" varchar(1) NOT NULL default ' ',
  "AAABFIPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIQF","AAABFIQG","AAABFIQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIPZ" (
  "AAABFIQF" varchar(2) NOT NULL default '00',
  "AAABFIQG" varchar(8) NOT NULL default ' ',
  "AAABFIQH" varchar(4) NOT NULL default '0000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJQTW" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIQF","AAABFIQG","AAABFIQH","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIQJ" (
  "AAABFIQN" varchar(40) NOT NULL default ' ',
  "AAABFIQL" varchar(32) NOT NULL default ' ',
  "AAABFIQM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABFIQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFIQR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABFIQZ" varchar(1) NOT NULL default ' ',
  "AAABJVQM" varchar(15) NOT NULL default ' ',
  "AAABFIRB" varchar(15) NOT NULL default ' ',
  "AAABFIRC" varchar(40) NOT NULL default ' ',
  "AAABFIRD" varchar(40) NOT NULL default ' ',
  "AAABLLIN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABIAEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIRK" varchar(2) NOT NULL default ' ',
  "AAABFIRL" varchar(2) NOT NULL default ' ',
  "AAABFIRM" varchar(1) NOT NULL default ' ',
  "AAABFIRN" varchar(8) NOT NULL default ' ',
  "AAABFIRO" varchar(4) NOT NULL default ' ',
  "AAABFIRP" varchar(4) NOT NULL default ' ',
  "AAABFIRQ" varchar(1) NOT NULL default ' ',
  "AAABFIRR" varchar(8) NOT NULL default ' ',
  "AAABFIRS" varchar(4) NOT NULL default ' ',
  "AAABFIRT" varchar(4) NOT NULL default ' ',
  "AAABFIRU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVJC" varchar(1) NOT NULL default ' ',
  "AAABKXRE" varchar(2) NOT NULL default ' ',
  "AAABHVJD" varchar(40) NOT NULL default ' ',
  "AAABHVJE" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABHVJG" varchar(20) NOT NULL default ' ',
  "AAABHVJH" varchar(20) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFISG" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVJC","AAABKXRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFISK" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFISS" varchar(5) NOT NULL default '00000',
  "AAABGXFD" varchar(20) NOT NULL default ' ',
  "AAABFISO" varchar(64) NOT NULL default ' ',
  "AAABMKWZ" varchar(32) NOT NULL default ' ',
  "AAABLITM" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABFISS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIST" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFISV" varchar(1) NOT NULL default ' ',
  "AAABFISW" varchar(1) NOT NULL default ' ',
  "AAABFISX" varchar(4) NOT NULL default ' ',
  "AAABFISY" varchar(1) NOT NULL default ' ',
  "AAABFISZ" varchar(1) NOT NULL default ' ',
  "AAABFITA" varchar(4) NOT NULL default ' ',
  "AAABFITB" varchar(1) NOT NULL default ' ',
  "AAABFITC" varchar(1) NOT NULL default ' ',
  "AAABFITD" varchar(1) NOT NULL default ' ',
  "AAABFITE" varchar(2) NOT NULL default ' ',
  "AAABFITF" varchar(2) NOT NULL default ' ',
  "AAABFITG" varchar(1) NOT NULL default ' ',
  "AAABFITH" varchar(1) NOT NULL default ' ',
  "AAABFITI" varchar(1) NOT NULL default ' ',
  "AAABFITJ" varchar(1) NOT NULL default ' ',
  "AAABFITK" varchar(1) NOT NULL default ' ',
  "AAABFITL" varchar(1) NOT NULL default ' ',
  "AAABFITM" varchar(1) NOT NULL default ' ',
  "AAABFITN" varchar(1) NOT NULL default ' ',
  "AAABFITO" varchar(1) NOT NULL default ' ',
  "AAABFITP" varchar(1) NOT NULL default ' ',
  "AAABFITQ" varchar(1) NOT NULL default ' ',
  "AAABFITR" varchar(1) NOT NULL default ' ',
  "AAABFITS" varchar(1) NOT NULL default ' ',
  "AAABFITT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFITV" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABFITZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIUC" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIUG" (
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABJICB" varchar(1) NOT NULL default ' ',
  "AAABFIUJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIUL" (
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHGO" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFHC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIUS" (
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIUW" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABFIUY" varchar(40) NOT NULL default ' ',
  "AAABFIUZ" varchar(4) NOT NULL default '0000',
  "AAABFIVA" varchar(1) NOT NULL default ' ',
  "AAABFIVB" varchar(1) NOT NULL default ' ',
  "AAABFIVC" varchar(3) NOT NULL default '000',
  "AAABFIVD" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFIVF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIVH" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIVN" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIVR" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABFIYU" varchar(1) NOT NULL default ' ',
  "AAABFIYY" varchar(2) NOT NULL default '00',
  "AAABFIYZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLCGT","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIVZ" (
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIWE" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIWK" (
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABLQBG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABJFHC","AAABJGDL","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIZA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIZA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIZA" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIZA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABFIZA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCGT","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABFIXQ" varchar(1) NOT NULL default ' ',
  "AAABFUAT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLKXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABLCGT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIYQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABFJAC" varchar(5) NOT NULL default ' ',
  "AAABFIYU" varchar(1) NOT NULL default ' ',
  "AAABJFUZ" varchar(2) NOT NULL default '00',
  "AAABFIYW" varchar(2) NOT NULL default '00',
  "AAABFIYX" varchar(2) NOT NULL default '00',
  "AAABFIYY" varchar(2) NOT NULL default '00',
  "AAABFIYZ" varchar(2) NOT NULL default '00',
  "AAABFIZA" varchar(8) NOT NULL default ' ',
  "AAABFIZB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABFJAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABFJAC" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKHGO" varchar(4) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  "AAABMOQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABFJAC","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFIZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFJAC" varchar(5) NOT NULL default ' ',
  "AAABLQBG" varchar(1) NOT NULL default '0',
  "AAABFIZX" varchar(1) NOT NULL default ' ',
  "AAABFIZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLKXR","AAABJGDL","AAABFJAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABKHGP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFTXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABMLEL" varchar(4) NOT NULL default '0000',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABJSBE" varchar(5) NOT NULL default ' ',
  "AAABFJAX" varchar(1) NOT NULL default ' ',
  "AAABFJAY" varchar(1) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  "AAABFJBB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTXT","AAABMLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLEAH" varchar(30) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLEAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJBZ" varchar(1) NOT NULL default ' ',
  "AAABFJCA" varchar(10) NOT NULL default ' ',
  "AAABFJCB" varchar(6) NOT NULL default ' ',
  "AAABFJCC" varchar(8) NOT NULL default '00000000',
  "AAABFJCD" smallint(6) NOT NULL default '0',
  "AAABFJCE" decimal(10,7) NOT NULL default '0.0000000',
  "AAABLUTZ" smallint(6) NOT NULL default '0',
  "AAABFJBX" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABFJBZ","AAABFJCA","AAABFJCB","AAABFJCC","AAABFJCD","AAABFJCE","AAABLUTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJCG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCO" varchar(2) NOT NULL default '00',
  "AAABFJCP" varchar(4) NOT NULL default '0000',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  "AAABFJCM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCO","AAABFJCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJCU" varchar(20) NOT NULL default ' ',
  "AAABFJCV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFJDH" varchar(3) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABFJDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJDM" varchar(20) NOT NULL default ' ',
  "AAABFJDN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJOM" varchar(22) NOT NULL default ' ',
  "AAABFJON" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJEP" varchar(1) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJOM" varchar(22) NOT NULL default ' ',
  "AAABFJON" varchar(22) NOT NULL default ' ',
  "AAABFJEW" varchar(8) NOT NULL default '00000000',
  "AAABFJEX" varchar(8) NOT NULL default '00000000',
  "AAABFJEY" varchar(1) NOT NULL default ' ',
  "AAABFJEZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYOZ" varchar(3) NOT NULL default '000',
  "AAABKYPA" varchar(3) NOT NULL default '000',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYOZ","AAABKYPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJHC" varchar(20) NOT NULL default ' ',
  "AAABFJHD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJGB" varchar(12) NOT NULL default '000000000000',
  "AAABFJGC" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJGM" varchar(8) NOT NULL default '00000000',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKYOZ" varchar(3) NOT NULL default '000',
  "AAABKYPA" varchar(3) NOT NULL default '000',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKYOZ","AAABKYPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJHC" varchar(20) NOT NULL default ' ',
  "AAABFJHD" varchar(20) NOT NULL default ' ',
  "AAABFJHE" varchar(8) NOT NULL default '00000000',
  "AAABFJHF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJHW" varchar(4) NOT NULL default ' ',
  "AAABFJHX" varchar(4) NOT NULL default ' ',
  "AAABFJHY" varchar(1) NOT NULL default ' ',
  "AAABFJHZ" varchar(1) NOT NULL default ' ',
  "AAABFJIA" varchar(13) NOT NULL default ' ',
  "AAABFJIB" varchar(13) NOT NULL default ' ',
  "AAABFJOM" varchar(22) NOT NULL default ' ',
  "AAABFJON" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJREU" varchar(1) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLXTG" varchar(22) NOT NULL default ' ',
  "AAABFJIN" varchar(1) NOT NULL default ' ',
  "AAABFJIO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJREU","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJJN" varchar(16) NOT NULL default ' ',
  "AAABFJJO" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJJN" varchar(16) NOT NULL default ' ',
  "AAABFJJO" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJKF" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJKK" varchar(16) NOT NULL default ' ',
  "AAABFJKL" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJKP" (
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
  "AAABFOJA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABHFMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJMR" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJMX" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJNC" (
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABFOMB" varchar(3) NOT NULL default '000',
  "AAABFOLW" varchar(40) NOT NULL default ' ',
  "AAABINFJ" varchar(40) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMPMH","AAABFOMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ","AAABFOEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABFJOM" varchar(22) NOT NULL default ' ',
  "AAABFJON" varchar(22) NOT NULL default ' ',
  "AAABFJOO" varchar(8) NOT NULL default '00000000',
  "AAABFJOP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGZJM" varchar(3) NOT NULL default ' ',
  "AAABFJOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGZJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJPB" (
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
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJRC" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFJRT" varchar(1) NOT NULL default ' ',
  "AAABFJRU" varchar(1) NOT NULL default ' ',
  "AAABFJRV" varchar(1) NOT NULL default ' ',
  "AAABFJRW" varchar(1) NOT NULL default ' ',
  "AAABFJRX" varchar(1) NOT NULL default ' ',
  "AAABFNJL" varchar(1) NOT NULL default ' ',
  "AAABFJRZ" varchar(1) NOT NULL default ' ',
  "AAABFJSA" varchar(1) NOT NULL default ' ',
  "AAABFJSB" varchar(1) NOT NULL default ' ',
  "AAABFJSC" varchar(1) NOT NULL default ' ',
  "AAABFJSD" varchar(1) NOT NULL default ' ',
  "AAABFJSE" varchar(1) NOT NULL default ' ',
  "AAABFJSF" varchar(1) NOT NULL default ' ',
  "AAABFJSG" varchar(1) NOT NULL default ' ',
  "AAABFJSH" varchar(1) NOT NULL default ' ',
  "AAABFJSI" varchar(1) NOT NULL default ' ',
  "AAABFJSJ" varchar(1) NOT NULL default ' ',
  "AAABFJSK" varchar(1) NOT NULL default ' ',
  "AAABFJSL" varchar(1) NOT NULL default ' ',
  "AAABFJSM" varchar(1) NOT NULL default ' ',
  "AAABFJSN" varchar(1) NOT NULL default ' ',
  "AAABFJSO" varchar(1) NOT NULL default ' ',
  "AAABFJSP" varchar(1) NOT NULL default ' ',
  "AAABFJTO" varchar(1) NOT NULL default ' ',
  "AAABFJSR" varchar(1) NOT NULL default ' ',
  "AAABFJSS" varchar(1) NOT NULL default ' ',
  "AAABFJST" varchar(1) NOT NULL default ' ',
  "AAABFJSU" varchar(1) NOT NULL default ' ',
  "AAABFJSV" varchar(1) NOT NULL default ' ',
  "AAABFJSW" varchar(15) NOT NULL default ' ',
  "AAABFJSX" varchar(15) NOT NULL default ' ',
  "AAABFJSY" varchar(15) NOT NULL default ' ',
  "AAABFJSZ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFJTO" varchar(1) NOT NULL default ' ',
  "AAABFJXP" varchar(1) NOT NULL default ' ',
  "AAABFJXQ" varchar(1) NOT NULL default ' ',
  "AAABFJTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBB","AAABFJTO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAHQ" varchar(25) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABFJTV" varchar(2) NOT NULL default ' ',
  "AAABFJTW" varchar(1) NOT NULL default ' ',
  "AAABKRIM" varchar(10) NOT NULL default ' ',
  "AAABFJTY" varchar(2) NOT NULL default ' ',
  "AAABFJTZ" varchar(10) NOT NULL default ' ',
  "AAABFJUA" varchar(12) NOT NULL default ' ',
  "AAABFJUB" varchar(1) NOT NULL default ' ',
  "AAABFJUC" varchar(1) NOT NULL default ' ',
  "AAABFJUD" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFJUF" varchar(10) NOT NULL default ' ',
  "AAABFJUG" varchar(12) NOT NULL default ' ',
  "AAABFJUH" varchar(2) NOT NULL default ' ',
  "AAABFJUI" varchar(1) NOT NULL default ' ',
  "AAABFJUJ" varchar(1) NOT NULL default ' ',
  "AAABFJUK" varchar(12) NOT NULL default ' ',
  "AAABFJUL" varchar(1) NOT NULL default ' ',
  "AAABFJUM" varchar(10) NOT NULL default ' ',
  "AAABFJUN" varchar(10) NOT NULL default ' ',
  "AAABFJUO" varchar(5) NOT NULL default ' ',
  "AAABFJUP" varchar(2) NOT NULL default ' ',
  "AAABFJUQ" varchar(1) NOT NULL default ' ',
  "AAABFJUR" varchar(2) NOT NULL default ' ',
  "AAABFJUS" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMCNU" varchar(4) NOT NULL default ' ',
  "AAABFJUV" varchar(1) NOT NULL default ' ',
  "AAABFJUW" varchar(2) NOT NULL default ' ',
  "AAABFJUX" varchar(1) NOT NULL default '0',
  "AAABFJUY" varchar(4) NOT NULL default ' ',
  "AAABFJUZ" varchar(1) NOT NULL default ' ',
  "AAABMCWV" varchar(12) NOT NULL default ' ',
  "AAABFJVB" varchar(1) NOT NULL default ' ',
  "AAABFJVC" varchar(1) NOT NULL default '0',
  "AAABFJVD" varchar(10) NOT NULL default ' ',
  "AAABFJVE" varchar(1) NOT NULL default ' ',
  "AAABFJVF" varchar(1) NOT NULL default ' ',
  "AAABFJVG" varchar(1) NOT NULL default ' ',
  "AAABFJVH" varchar(1) NOT NULL default ' ',
  "AAABFJVI" varchar(1) NOT NULL default ' ',
  "AAABFJVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABFJVZ" varchar(1) NOT NULL default ' ',
  "AAABFJWA" varchar(1) NOT NULL default ' ',
  "AAABMLCA" varchar(30) NOT NULL default ' ',
  "AAABLYMH" varchar(30) NOT NULL default ' ',
  "AAABFJWD" varchar(1) NOT NULL default ' ',
  "AAABFJWE" varchar(1) NOT NULL default ' ',
  "AAABFJWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFUKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFULA" varchar(1) NOT NULL default ' ',
  "AAABFJWM" varchar(1) NOT NULL default ' ',
  "AAABFJWN" varchar(1) NOT NULL default ' ',
  "AAABFJWO" varchar(1) NOT NULL default ' ',
  "AAABFJWP" varchar(1) NOT NULL default ' ',
  "AAABFJWQ" varchar(1) NOT NULL default ' ',
  "AAABFJWR" varchar(1) NOT NULL default ' ',
  "AAABFJWS" varchar(1) NOT NULL default ' ',
  "AAABFJWT" varchar(2) NOT NULL default ' ',
  "AAABFSXI" varchar(1) NOT NULL default ' ',
  "AAABKRIP" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFULA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJWY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(1) NOT NULL default ' ',
  "AAABLSAK" varchar(4) NOT NULL default ' ',
  "AAABFJXC" varchar(1) NOT NULL default ' ',
  "AAABFJXD" varchar(1) NOT NULL default ' ',
  "AAABFJXE" varchar(1) NOT NULL default ' ',
  "AAABFJXF" varchar(1) NOT NULL default ' ',
  "AAABFJXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLUER","AAABLSAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFJXP" varchar(1) NOT NULL default ' ',
  "AAABFJXQ" varchar(1) NOT NULL default ' ',
  "AAABFJXR" varchar(1) NOT NULL default ' ',
  "AAABFJXS" varchar(1) NOT NULL default ' ',
  "AAABKVPA" varchar(1) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABHUQZ" varchar(8) NOT NULL default '00000000',
  "AAABFJXW" varchar(1) NOT NULL default ' ',
  "AAABFJXX" varchar(1) NOT NULL default ' ',
  "AAABFJXY" varchar(1) NOT NULL default ' ',
  "AAABFJXZ" varchar(1) NOT NULL default ' ',
  "AAABFJYA" varchar(3) NOT NULL default ' ',
  "AAABFJYB" varchar(1) NOT NULL default '0',
  "AAABFJYC" varchar(1) NOT NULL default '0',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFJYE" varchar(2) NOT NULL default '00',
  "AAABFJYF" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABLEST" varchar(3) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKRXK","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABFJYZ" varchar(3) NOT NULL default '000',
  "AAABFJYT" varchar(3) NOT NULL default '000',
  "AAABKFBP" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMDBB","AAABKOTY","AAABFJYZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABFJZE" varchar(1) NOT NULL default ' ',
  "AAABFJZF" varchar(1) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABFJZH" varchar(3) NOT NULL default ' ',
  "AAABFUVW" varchar(2) NOT NULL default '00',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  "AAABFUVZ" varchar(1) NOT NULL default ' ',
  "AAABFUWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKRXK","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFJZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFULS" varchar(3) NOT NULL default ' ',
  "AAABFKAB" varchar(1) NOT NULL default ' ',
  "AAABFKAC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFULS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFUMC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMB","AAABFUMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFUMC" varchar(3) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  "AAABFKAR" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFUMB","AAABFUMC","AAABFSFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFKAZ" varchar(1) NOT NULL default ' ',
  "AAABFKBA" varchar(1) NOT NULL default ' ',
  "AAABFKBB" varchar(1) NOT NULL default ' ',
  "AAABFKBC" varchar(1) NOT NULL default ' ',
  "AAABFKBD" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV","AAABKOTY","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMQHF" varchar(4) NOT NULL default ' ',
  "AAABMQHG" varchar(4) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABFKDB" varchar(1) NOT NULL default ' ',
  "AAABHTTZ" varchar(1) NOT NULL default ' ',
  "AAABHTUA" varchar(1) NOT NULL default ' ',
  "AAABFKDE" varchar(1) NOT NULL default ' ',
  "AAABICFV" varchar(40) NOT NULL default ' ',
  "AAABHTUC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABFKDU" varchar(5) NOT NULL default ' ',
  "AAABFKDV" varchar(8) NOT NULL default '00000000',
  "AAABFKDW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABMKXW" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(24) NOT NULL default ' ',
  "AAABMNNA" varchar(24) NOT NULL default ' ',
  "AAABFKFQ" varchar(1) NOT NULL default ' ',
  "AAABHTVH" varchar(1) NOT NULL default ' ',
  "AAABFKER" varchar(10) NOT NULL default ' ',
  "AAABFKES" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP","AAABMKXW","AAABMNML","AAABLGYD","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABHTVP" varchar(10) NOT NULL default ' ',
  "AAABMNMZ" varchar(32) NOT NULL default ' ',
  "AAABMNNA" varchar(32) NOT NULL default ' ',
  "AAABFKFQ" varchar(1) NOT NULL default ' ',
  "AAABHTVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP","AAABMKXW","AAABHTVO","AAABHTVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABFKGP" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIIVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABIHQW","AAABMREL","AAABIHLK","AAABFKGP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQCD","AAABLTXG","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKHC" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKHH" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKJJ" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGYQM","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABFKNJ" varchar(1) NOT NULL default ' ',
  "AAABFKJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABGYQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABFKNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABIHEH","AAABGYQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKKJ" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKKR" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFKNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKLH" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABHFNE" varchar(3) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  "AAABHFNH" varchar(1) NOT NULL default ' ',
  "AAABHFNI" varchar(1) NOT NULL default ' ',
  "AAABHFNM" varchar(30) NOT NULL default ' ',
  "AAABHFNN" varchar(30) NOT NULL default ' ',
  "AAABHFNP" varchar(1) NOT NULL default ' ',
  "AAABHFNQ" varchar(5) NOT NULL default ' ',
  "AAABHFNR" varchar(3) NOT NULL default ' ',
  "AAABHFNS" varchar(30) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKMD" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABHFME" varchar(1) NOT NULL default ' ',
  "AAABHFMF" varchar(30) NOT NULL default ' ',
  "AAABHFMG" varchar(30) NOT NULL default ' ',
  "AAABHFMH" varchar(1) NOT NULL default ' ',
  "AAABHFMI" varchar(18) NOT NULL default ' ',
  "AAABHFMK" varchar(30) NOT NULL default ' ',
  "AAABHFML" varchar(30) NOT NULL default ' ',
  "AAABHFMM" varchar(30) NOT NULL default ' ',
  "AAABHFMN" varchar(6) NOT NULL default '000000',
  "AAABHFMP" varchar(20) NOT NULL default ' ',
  "AAABHFMQ" varchar(20) NOT NULL default ' ',
  "AAABHFMR" varchar(30) NOT NULL default ' ',
  "AAABHFMX" varchar(1) NOT NULL default ' ',
  "AAABHFMY" varchar(1) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABHFND" varchar(1) NOT NULL default ' ',
  "AAABHFNV" varchar(31) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFKNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXIY","AAABIHEH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKOB" varchar(4) NOT NULL default ' ',
  "AAABFKOC" varchar(4) NOT NULL default ' ',
  "AAABMKFG" varchar(1) NOT NULL default ' ',
  "AAABFKOP" varchar(30) NOT NULL default ' ',
  "AAABFKNU" varchar(30) NOT NULL default ' ',
  "AAABFKNV" varchar(30) NOT NULL default ' ',
  "AAABLSDL" varchar(3) NOT NULL default ' ',
  "AAABFKNX" varchar(20) NOT NULL default ' ',
  "AAABFKNY" varchar(30) NOT NULL default ' ',
  "AAABFKNZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKOB","AAABFKOC","AAABMKFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKOP" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLNC" varchar(1) NOT NULL default ' ',
  "AAABLAUO" varchar(1) NOT NULL default ' ',
  "AAABMLNA" varchar(3) NOT NULL default '000',
  "AAABLTXU" varchar(10) NOT NULL default ' ',
  "AAABLAUE" varchar(1) NOT NULL default ' ',
  "AAABKGYH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKOP","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKOW" varchar(30) NOT NULL default ' ',
  "AAABFKOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKOW","AAABFKOX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKPL" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKRD" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFKRI" varchar(3) NOT NULL default '000',
  "AAABFKQZ" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFKRD","AAABMCRJ","AAABKRXK","AAABMDBB","AAABFKRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKRD" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFKRI" varchar(3) NOT NULL default '000',
  "AAABFKQZ" decimal(11,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCVU" decimal(5,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFKRD","AAABMCRK","AAABMCRJ","AAABKRXK","AAABMDBB","AAABFKRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKRJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNJ" varchar(1) NOT NULL default ' ',
  "AAABMMJW" varchar(4) NOT NULL default ' ',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUNJ","AAABMMJW","AAABIYCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKRS" (
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFKRW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABJGDL","AAABFKRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKRX" (
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  "AAABFKRZ" varchar(1) NOT NULL default ' ',
  "AAABFKSA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKSC" (
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  "AAABFKSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHQRM","AAABFKSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKSH" (
  "AAABFKUX" varchar(30) NOT NULL default ' ',
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABFKUX","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKSM" (
  "AAABFKUX" varchar(30) NOT NULL default ' ',
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  "AAABFKSU" varchar(30) NOT NULL default ' ',
  "AAABFKSQ" varchar(30) NOT NULL default ' ',
  "AAABFKSR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFKUX","AAABHQRM","AAABFKSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKUD" varchar(6) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKUD" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABMALF" varchar(2) NOT NULL default '00',
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  "AAABFKTH" varchar(3) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  "AAABFKTJ" decimal(5,2) NOT NULL default '0.00',
  "AAABFKVI" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFKUD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFM" varchar(4) NOT NULL default ' ',
  "AAABFKUD" varchar(6) NOT NULL default ' ',
  "AAABFKTS" varchar(1) NOT NULL default '0',
  "AAABMORC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKUD" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKUE" (
  "AAABFKUX" varchar(30) NOT NULL default ' ',
  "AAABMORC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABFKUX","AAABMORC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKUJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQRM" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABHQRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKUP" (
  "AAABFKUX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFKUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKUS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKUX" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFKUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAABKPOJ" varchar(1) NOT NULL default ' ',
  "AAABFKVC" varchar(1) NOT NULL default ' ',
  "AAABFKVD" varchar(1) NOT NULL default ' ',
  "AAABFKVE" varchar(3) NOT NULL default '000',
  "AAABFKVF" varchar(1) NOT NULL default ' ',
  "AAABMPTM" varchar(1) NOT NULL default ' ',
  "AAABFKVH" varchar(1) NOT NULL default ' ',
  "AAABFKVI" varchar(1) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKVL" varchar(2) NOT NULL default ' ',
  "AAABFKVM" varchar(1) NOT NULL default ' ',
  "AAABFKVN" varchar(1) NOT NULL default ' ',
  "AAABFKVO" varchar(1) NOT NULL default ' ',
  "AAABFKVP" varchar(1) NOT NULL default ' ',
  "AAABFKVQ" varchar(1) NOT NULL default ' ',
  "AAABLXHB" varchar(1) NOT NULL default ' ',
  "AAABFKVS" varchar(1) NOT NULL default ' ',
  "AAABFKVT" varchar(1) NOT NULL default ' ',
  "AAABFKVU" varchar(1) NOT NULL default ' ',
  "AAABFKVV" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABFKVX" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFKVZ" varchar(1) NOT NULL default ' ',
  "AAABFKWA" varchar(1) NOT NULL default ' ',
  "AAABFKWB" varchar(2) NOT NULL default ' ',
  "AAABFKWC" varchar(2) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABFKWE" varchar(1) NOT NULL default ' ',
  "AAABGCXN" varchar(1) NOT NULL default ' ',
  "AAABFKWG" varchar(1) NOT NULL default ' ',
  "AAABFKWH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHDLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHDLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKXE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKXE" varchar(3) NOT NULL default ' ',
  "AAABJFBB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKYS","AAABFKYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  "AAABHANX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKYS","AAABFKYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKYS" varchar(2) NOT NULL default ' ',
  "AAABFKYT" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKYS","AAABFKYT","AAABMCRJ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFLBP" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFKZG" varchar(10) NOT NULL default ' ',
  "AAABFKZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTVQ","AAABFLBP","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFTVQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFKZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFLBP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTVQ","AAABFLBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFLBP" varchar(3) NOT NULL default ' ',
  "AAABHANX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFTVQ","AAABFLBP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFLBP" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(15) NOT NULL default ' ',
  "AAABFLAT" varchar(10) NOT NULL default ' ',
  "AAABFLAU" varchar(10) NOT NULL default ' ',
  "AAABFLAV" varchar(2) NOT NULL default ' ',
  "AAABFLAW" varchar(2) NOT NULL default ' ',
  "AAABFLAX" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTVQ","AAABFLBP","AAABMCRJ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABFLBP" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFLBR" varchar(2) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABFLBK" varchar(1) NOT NULL default ' ',
  "AAABMCWW" varchar(2) NOT NULL default ' ',
  "AAABFLBM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTVQ","AAABFLBP","AAABMCRJ","AAABFLBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKAFO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFLER" varchar(3) NOT NULL default ' ',
  "AAABFLCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSQO","AAABFLER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFLER" varchar(3) NOT NULL default ' ',
  "AAABHANX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSQO","AAABFLER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFLER" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(10) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSQO","AAABFLER","AAABMCRJ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFLER" varchar(3) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFRJP" varchar(1) NOT NULL default ' ',
  "AAABFOLG" varchar(1) NOT NULL default ' ',
  "AAABFLDW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSQO","AAABFLER","AAABLXDN","AAABFRJP","AAABFOLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABFLER" varchar(3) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABFLEJ" varchar(15) NOT NULL default ' ',
  "AAABHAPO" varchar(24) NOT NULL default ' ',
  "AAABHAPP" varchar(24) NOT NULL default ' ',
  "AAABKRXL" varchar(2) NOT NULL default '00',
  "AAABKSTP" varchar(2) NOT NULL default ' ',
  "AAABFLEO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSQO","AAABFLER","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABIEWH" varchar(2) NOT NULL default ' ',
  "AAABGRYF" varchar(4) NOT NULL default ' ',
  "AAABHKHZ" varchar(4) NOT NULL default ' ',
  "AAABHKIM" varchar(4) NOT NULL default ' ',
  "AAABGRZJ" varchar(4) NOT NULL default ' ',
  "AAABFLFC" varchar(1) NOT NULL default ' ',
  "AAABFLGD" varchar(1) NOT NULL default ' ',
  "AAABFSXD" varchar(3) NOT NULL default '000',
  "AAABIKIG" varchar(30) NOT NULL default ' ',
  "AAABFLFG" varchar(1) NOT NULL default ' ',
  "AAABFZXA" varchar(4) NOT NULL default ' ',
  "AAABFLFI" varchar(1) NOT NULL default ' ',
  "AAABFLFJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABFLFS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLODQ","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABLYCI" varchar(2) NOT NULL default ' ',
  "AAABFSXD" varchar(3) NOT NULL default '000',
  "AAABFLGD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMCMO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLGR" (
  "AAABFLRX" varchar(3) NOT NULL default '000',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABFLGU" varchar(30) NOT NULL default ' ',
  "AAABFLGV" varchar(30) NOT NULL default ' ',
  "AAABFLGW" varchar(1) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFLGY" varchar(3) NOT NULL default ' ',
  "AAABFLGZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFLRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLHB" (
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABGYQP" varchar(30) NOT NULL default ' ',
  "AAABHFMF" varchar(30) NOT NULL default ' ',
  "AAABHFMG" varchar(30) NOT NULL default ' ',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABLUEO" varchar(32) NOT NULL default ' ',
  "AAABMJWN" varchar(32) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABFLHK" varchar(30) NOT NULL default ' ',
  "AAABIVIM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIIMJ","AAABGYQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFLHW" varchar(3) NOT NULL default '000',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABFMBN" varchar(1) NOT NULL default ' ',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  "AAABLMLB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFLHW","AAABLTXI","AAABFMBN","AAABFRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLIA" (
  "AAABFLIE" varchar(3) NOT NULL default '000',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFLIE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLIF" (
  "AAABFLII" varchar(30) NOT NULL default ' ',
  "AAABFLRX" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABFLII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLIJ" (
  "AAABFLJO" varchar(1) NOT NULL default '0',
  "AAABFLJM" decimal(6,4) NOT NULL default '0.0000',
  "AAABFLJN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFLJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(12) NOT NULL default ' ',
  "AAABFNWN" varchar(3) NOT NULL default '000',
  "AAABFNWO" varchar(3) NOT NULL default '000',
  "AAABFNWP" varchar(3) NOT NULL default '000',
  "AAABHJAV" decimal(6,4) NOT NULL default '0.0000',
  "AAABFNWR" decimal(6,4) NOT NULL default '0.0000',
  "AAABFNWS" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABFNWU" varchar(2) NOT NULL default ' ',
  "AAABFNWV" varchar(6) NOT NULL default '000000',
  "AAABFNWW" varchar(1) NOT NULL default ' ',
  "AAABFNWX" varchar(1) NOT NULL default ' ',
  "AAABFNWY" varchar(1) NOT NULL default ' ',
  "AAABFNWZ" varchar(6) NOT NULL default '000000',
  "AAABFNXA" varchar(100) NOT NULL default ' ',
  "AAABFNXB" varchar(100) NOT NULL default ' ',
  "AAABFNXC" varchar(2) NOT NULL default '00',
  "AAABFNXD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLJK" (
  "AAABFLJO" varchar(1) NOT NULL default '0',
  "AAABFLJM" decimal(6,4) NOT NULL default '0.0000',
  "AAABFLJN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFLJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(12) NOT NULL default ' ',
  "AAABFNWM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLJX" (
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
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLLX" (
  "AAABFLRX" varchar(3) NOT NULL default '000',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFLRX","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLMC" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLMI" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGPQK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGPQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLMN" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGPQK" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGPQK","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLMU" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGPQK" varchar(30) NOT NULL default ' ',
  "AAABFQZS" varchar(20) NOT NULL default ' ',
  "AAABFLMZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHEH","AAABGPQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHAE" varchar(2) NOT NULL default ' ',
  "AAABLUSU" varchar(32) NOT NULL default ' ',
  "AAABFLQL" varchar(32) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABFLNM" varchar(3) NOT NULL default '000',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABIHAE","AAABLUSU","AAABFLQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHAE" varchar(2) NOT NULL default ' ',
  "AAABLUSU" varchar(32) NOT NULL default ' ',
  "AAABFLQL" varchar(32) NOT NULL default ' ',
  "AAABFLOA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHEH","AAABIHAE","AAABLUSU","AAABFLQL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHAE" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABIHSD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABIHAE","AAABMKXW","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHAE" varchar(2) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABIHSD" varchar(50) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHEH","AAABIHAE","AAABMKXW","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFLPT" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFLPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFLPT" varchar(3) NOT NULL default '000',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFLPT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABIHAE" varchar(2) NOT NULL default ' ',
  "AAABLUSU" varchar(32) NOT NULL default ' ',
  "AAABFLQL" varchar(32) NOT NULL default ' ',
  "AAABFLQM" varchar(8) NOT NULL default '00000000',
  "AAABFLQB" varchar(8) NOT NULL default '00000000',
  "AAABFLQC" varchar(8) NOT NULL default '00000000',
  "AAABJLHO" varchar(50) NOT NULL default ' ',
  "AAABFLQE" varchar(50) NOT NULL default ' ',
  "AAABFLQF" varchar(1) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHEH","AAABIHAE","AAABLUSU","AAABFLQL","AAABFLQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLQN" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABFLQU" varchar(30) NOT NULL default ' ',
  "AAABFLQR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGYQM","AAABFLQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLQV" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABHFNE" varchar(3) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  "AAABFLRD" varchar(1) NOT NULL default ' ',
  "AAABHFNH" varchar(1) NOT NULL default ' ',
  "AAABHFNI" varchar(1) NOT NULL default ' ',
  "AAABJLRA" varchar(3) NOT NULL default '000',
  "AAABJFAA" varchar(5) NOT NULL default ' ',
  "AAABJFLR" varchar(12) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAABHFNY" varchar(6) NOT NULL default '000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABFLRO" varchar(1) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABFRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLRS" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABFLRX" varchar(3) NOT NULL default '000',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABFLRX","AAABFRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLRZ" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABFLSK" varchar(34) NOT NULL default ' ',
  "AAABFLSC" varchar(3) NOT NULL default ' ',
  "AAABKOVP" varchar(1) NOT NULL default ' ',
  "AAABFLSE" varchar(34) NOT NULL default ' ',
  "AAABFLSF" varchar(34) NOT NULL default ' ',
  "AAABFLSG" varchar(3) NOT NULL default ' ',
  "AAABHFNF" varchar(3) NOT NULL default ' ',
  "AAABHFNG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABFLSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLSL" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGYQM","AAABFRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLSS" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABLXJT" varchar(3) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABLXJT","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  "AAABLZDI" varchar(12) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMBYX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLTS" (
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABFLTU" varchar(1) NOT NULL default ' ',
  "AAABFLTV" varchar(4) NOT NULL default ' ',
  "AAABFLTW" varchar(4) NOT NULL default ' ',
  "AAABFLTX" varchar(4) NOT NULL default ' ',
  "AAABFLTY" varchar(4) NOT NULL default ' ',
  "AAABFLTZ" varchar(1) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABFLUB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLUD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLCGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLUJ" (
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABFLVC" varchar(4) NOT NULL default ' ',
  "AAABHXLS" varchar(30) NOT NULL default ' ',
  "AAABFLVS" varchar(3) NOT NULL default '000',
  "AAABLQRT" varchar(1) NOT NULL default ' ',
  "AAABFLUP" varchar(1) NOT NULL default ' ',
  "AAABFLUQ" tinyint(3) unsigned NOT NULL default '0',
  "AAABFLUR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT","AAABFLVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLUU" (
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABFLVC" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(2) NOT NULL default '00',
  "AAABFLUY" varchar(30) NOT NULL default ' ',
  "AAABFLUZ" varchar(4) NOT NULL default ' ',
  "AAABFLVA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT","AAABFLVC","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLVE" (
  "AAABKUQR" varchar(8) NOT NULL default '00000000',
  "AAABLWWO" varchar(6) NOT NULL default '000000',
  "AAABFLVM" varchar(4) NOT NULL default '0000',
  "AAABFLVN" varchar(2) NOT NULL default '00',
  "AAABIHSD" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABKUQR","AAABLWWO","AAABFLVM","AAABFLVN","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLVP" (
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABHXLS" varchar(30) NOT NULL default ' ',
  "AAABFLVS" varchar(3) NOT NULL default '000',
  "AAABFLVT" varchar(10) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABFLVY" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCGT","AAABHXLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLWC" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLWH" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMKFH" varchar(10) NOT NULL default ' ',
  "AAABKNGM" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMPMH","AAABMKFH","AAABKNGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLWP" (
  "AAABLCGT" varchar(4) NOT NULL default ' ',
  "AAABIHLK" varchar(2) NOT NULL default '00',
  "AAABILPS" varchar(30) NOT NULL default ' ',
  "AAABFLWT" varchar(6) NOT NULL default '000000',
  "AAABFLWU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLCGT","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLWX" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLXL" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABFLXS" varchar(4) NOT NULL default '0000',
  "AAABFLXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABFLXW","AAABFLXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLXT" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFLYH" (
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
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMAH" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMAN" (
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHFMY" varchar(1) NOT NULL default ' ',
  "AAABHFMZ" varchar(1) NOT NULL default ' ',
  "AAABHFNA" varchar(1) NOT NULL default ' ',
  "AAABHFNB" varchar(1) NOT NULL default ' ',
  "AAABHFNC" varchar(1) NOT NULL default ' ',
  "AAABHFND" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHEH","AAABGYQM","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMBA" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  "AAABFQZS" varchar(20) NOT NULL default ' ',
  "AAABFQZT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIHEH","AAABFRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMLX" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(1) NOT NULL default ' ',
  "AAABFMBN" varchar(1) NOT NULL default ' ',
  "AAABFPTL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMLX","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFPTL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFPTL" varchar(3) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABFMCW" varchar(1) NOT NULL default ' ',
  "AAABFMCX" varchar(1) NOT NULL default ' ',
  "AAABFMCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLEAH" varchar(30) NOT NULL default ' ',
  "AAABFMDG" varchar(1) NOT NULL default ' ',
  "AAABLEBN" varchar(12) NOT NULL default ' ',
  "AAABKNCM" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLEAH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFMED" varchar(10) NOT NULL default ' ',
  "AAABFMDY" varchar(30) NOT NULL default ' ',
  "AAABMPTM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMCRJ","AAABFMED")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMEE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABGOOI" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(2) NOT NULL default '00',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHAGF" smallint(6) NOT NULL default '0',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(30) NOT NULL default ' ',
  "AAABLTYG" varchar(30) NOT NULL default ' ',
  "AAABIABP" varchar(30) NOT NULL default ' ',
  "AAABIABQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABGOOI","AAABMLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMFT" varchar(3) NOT NULL default ' ',
  "AAABMGOZ" varchar(3) NOT NULL default '000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFMFC" varchar(4) NOT NULL default ' ',
  "AAABFMFD" varchar(4) NOT NULL default ' ',
  "AAABFMFE" varchar(4) NOT NULL default ' ',
  "AAABFMFF" varchar(4) NOT NULL default '0000',
  "AAABHFPL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMFT" varchar(3) NOT NULL default ' ',
  "AAABFMFP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABFMFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMFU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFQYW" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFQWX" varchar(1) NOT NULL default ' ',
  "AAABFOPK" varchar(2) NOT NULL default ' ',
  "AAABFOPJ" varchar(1) NOT NULL default ' ',
  "AAABFMJY" varchar(1) NOT NULL default ' ',
  "AAABFMJZ" varchar(1) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABFMKB" varchar(1) NOT NULL default ' ',
  "AAABFMGJ" varchar(1) NOT NULL default ' ',
  "AAABHFJM" varchar(4) NOT NULL default ' ',
  "AAABHFJN" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  "AAABGWNX" varchar(1) NOT NULL default ' ',
  "AAABFMGX" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABFREP" varchar(8) NOT NULL default '00000000',
  "AAABFREQ" varchar(12) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABFMHE" varchar(8) NOT NULL default '00000000',
  "AAABFMHF" varchar(8) NOT NULL default '00000000',
  "AAABJHFW" varchar(8) NOT NULL default ' ',
  "AAABFMHH" varchar(1) NOT NULL default ' ',
  "AAABFMHI" varchar(8) NOT NULL default '00000000',
  "AAABFMHJ" varchar(6) NOT NULL default '000000',
  "AAABFRWJ" varchar(2) NOT NULL default '00',
  "AAABFMHL" varchar(1) NOT NULL default ' ',
  "AAABFMHM" varchar(2) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABHDEU" varchar(40) NOT NULL default ' ',
  "AAABFMHP" varchar(1) NOT NULL default ' ',
  "AAABFMHQ" varchar(1) NOT NULL default ' ',
  "AAABFMHR" varchar(1) NOT NULL default ' ',
  "AAABFMHS" int(11) NOT NULL default '0',
  "AAABFRTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABKMGT" varchar(10) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFQYW" varchar(3) NOT NULL default '000',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMIY" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPNM" varchar(1) NOT NULL default ' ',
  "AAABFMMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIVVH" varchar(2) NOT NULL default ' ',
  "AAABJUQY" varchar(8) NOT NULL default '00000000',
  "AAABJTGS" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIVVH","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFQWX" varchar(1) NOT NULL default ' ',
  "AAABFOPK" varchar(2) NOT NULL default ' ',
  "AAABFOPJ" varchar(1) NOT NULL default ' ',
  "AAABFMJY" varchar(1) NOT NULL default ' ',
  "AAABFMJZ" varchar(1) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABFMKB" varchar(1) NOT NULL default ' ',
  "AAABFMKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMKG" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABJTCX" varchar(4) NOT NULL default ' ',
  "AAABFMKK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABJTCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMKO" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABJTCX" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default ' ',
  "AAABFMKT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABJTCX","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMKY" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABJTCX" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  "AAABFPKJ" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  "AAABFMLI" varchar(30) NOT NULL default ' ',
  "AAABFMLJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABJTCX","AAABMNYT","AAABLWWP","AAABFPKJ","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFMLX" varchar(2) NOT NULL default '00',
  "AAABFMLY" varchar(6) NOT NULL default ' ',
  "AAABFMLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFMMJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABMIRY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABHTVP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABHTVO","AAABJECQ","AAABHTVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABFNHN" varchar(10) NOT NULL default '0000000000',
  "AAABKMGT" varchar(10) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABFNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMOK" (
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFMOQ" varchar(10) NOT NULL default ' ',
  "AAABMGOZ" varchar(2) NOT NULL default '00',
  "AAABFMOO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABJECQ","AAABFMOQ","AAABMGOZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMOS" (
  "AAABFMOY" varchar(30) NOT NULL default ' ',
  "AAABFMOZ" varchar(30) NOT NULL default ' ',
  "AAABFMPA" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMOY","AAABFMOZ","AAABFMPA","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFMPH" varchar(2) NOT NULL default ' ',
  "AAABMCMG" varchar(4) NOT NULL default '0000',
  "AAABFMPJ" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMPO" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFRWH" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABFMPV" varchar(30) NOT NULL default ' ',
  "AAABFMPW" varchar(1) NOT NULL default ' ',
  "AAABFMPX" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(1) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFMVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMQC" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFMQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMQI" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMRT" varchar(30) NOT NULL default ' ',
  "AAABFMRU" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABFMQV" varchar(8) NOT NULL default '00000000',
  "AAABFMQW" varchar(6) NOT NULL default '000000',
  "AAABFMQX" varchar(8) NOT NULL default '00000000',
  "AAABFMQY" varchar(6) NOT NULL default '000000',
  "AAABFMQZ" decimal(16,0) NOT NULL default '0',
  "AAABFMRA" int(11) NOT NULL default '0',
  "AAABFMRB" int(11) NOT NULL default '0',
  "AAABFMRC" int(11) NOT NULL default '0',
  "AAABFMRD" varchar(8) NOT NULL default '00000000',
  "AAABFMRE" varchar(6) NOT NULL default '000000',
  "AAABFMRF" varchar(8) NOT NULL default '00000000',
  "AAABFMRG" varchar(6) NOT NULL default '000000',
  "AAABFMRH" decimal(16,0) NOT NULL default '0',
  "AAABFMRI" int(11) NOT NULL default '0',
  "AAABFMRJ" int(11) NOT NULL default '0',
  "AAABFMRK" decimal(16,0) NOT NULL default '0',
  "AAABFPGK" varchar(6) NOT NULL default '000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMRQ" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABFMRT" varchar(30) NOT NULL default ' ',
  "AAABFMRU" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABFNER" varchar(1) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAABFNES" varchar(30) NOT NULL default ' ',
  "AAABFNET" varchar(6) NOT NULL default '000000',
  "AAABFNEF" decimal(16,0) NOT NULL default '0',
  "AAABFNEG" decimal(16,0) NOT NULL default '0',
  "AAABFNEH" int(11) NOT NULL default '0',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABFNEM" int(11) NOT NULL default '0',
  "AAABFNEN" int(11) NOT NULL default '0',
  "AAABFPGK" varchar(6) NOT NULL default '000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABFNEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMSV" (
  "AAABFMTA" varchar(30) NOT NULL default ' ',
  "AAABFMTB" varchar(30) NOT NULL default ' ',
  "AAABFMTC" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMTA","AAABFMTB","AAABFMTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMTD" (
  "AAABFMYS" varchar(30) NOT NULL default ' ',
  "AAABFMTF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMTH" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(10) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABMRFQ","AAABMLES","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMTX" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(10) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABFNEP","AAABMRFQ","AAABMLES","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMUP" (
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFNBV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMWU" varchar(7) NOT NULL default '0000000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFMVB" varchar(72) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(3) NOT NULL default ' ',
  "AAABFMVG" varchar(7) NOT NULL default '0000000',
  "AAABFPZB" varchar(3) NOT NULL default ' ',
  "AAABFPIA" varchar(1) NOT NULL default ' ',
  "AAABFMVJ" varchar(12) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABFNBV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMWU" varchar(7) NOT NULL default '0000000',
  "AAABMRCU" varchar(6) NOT NULL default '000000',
  "AAABFMVT" varchar(10) NOT NULL default ' ',
  "AAABFMVU" varchar(10) NOT NULL default ' ',
  "AAABFMVV" varchar(10) NOT NULL default ' ',
  "AAABFMVW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMWU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMWA" (
  "AAABKGBL" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(10) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABFMWF" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAABFMWH" varchar(30) NOT NULL default ' ',
  "AAABFMWI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL","AAABILHK","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMWU" varchar(7) NOT NULL default '0000000',
  "AAABFTNB" varchar(30) NOT NULL default ' ',
  "AAABFMWQ" varchar(60) NOT NULL default ' ',
  "AAABFMWR" varchar(64) NOT NULL default ' ',
  "AAABFMWS" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMWU","AAABFTNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMWW" (
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABFMWY" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILRH" varchar(16) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILRH","AAABLXDN","AAABHCXM","AAABFNBJ","AAABIHKQ","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABLGXW" varchar(4) NOT NULL default '0000',
  "AAABFMYR" varchar(30) NOT NULL default ' ',
  "AAABFMYS" varchar(30) NOT NULL default ' ',
  "AAABFRJK" varchar(2) NOT NULL default '00',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFNBJ","AAABFZAB","AAABLXDN","AAABHCXM","AAABIHKQ","AAABFMZT","AAABLGXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABFMYR" varchar(30) NOT NULL default ' ',
  "AAABFMYS" varchar(30) NOT NULL default ' ',
  "AAABFMYT" varchar(30) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  "AAABFRJK" varchar(2) NOT NULL default '00',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFZAB","AAABLXDN","AAABHCXM","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABHCXM","AAABFMZT","AAABFNBJ","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMZQ" (
  "AAABFMZT" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFMZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFMZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILRH" varchar(16) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNBV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILRH","AAABLXDN","AAABHCXM","AAABFNBJ","AAABIHKQ","AAABFZAB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNAM" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB","AAABJECQ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNAX" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(20) NOT NULL default ' ',
  "AAABIWTO" varchar(40) NOT NULL default ' ',
  "AAABIWTP" varchar(60) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABFNBJ" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABHCXM","AAABFZAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default ' ',
  "AAABFZAB" varchar(72) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFNBV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABHCXM","AAABFZAB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNCB" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNCK" varchar(8) NOT NULL default '00000000',
  "AAABFNCL" varchar(6) NOT NULL default '000000',
  "AAABKPMT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNCH" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABFNCK" varchar(8) NOT NULL default '00000000',
  "AAABFNCL" varchar(6) NOT NULL default '000000',
  "AAABKPMT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABIANB","AAABFNEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNCP" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNCR" varchar(1) NOT NULL default ' ',
  "AAABFNCS" varchar(40) NOT NULL default ' ',
  "AAABFNCT" varchar(30) NOT NULL default ' ',
  "AAABFNCU" varchar(30) NOT NULL default ' ',
  "AAABFNCV" varchar(1) NOT NULL default ' ',
  "AAABFNCW" varchar(40) NOT NULL default ' ',
  "AAABFNCX" varchar(30) NOT NULL default ' ',
  "AAABFNCY" varchar(30) NOT NULL default ' ',
  "AAABFNCZ" varchar(1) NOT NULL default ' ',
  "AAABFNDA" varchar(40) NOT NULL default ' ',
  "AAABFNDB" varchar(30) NOT NULL default ' ',
  "AAABFNDC" varchar(30) NOT NULL default ' ',
  "AAABFNDD" varchar(1) NOT NULL default ' ',
  "AAABFNDE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIANB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNES" varchar(30) NOT NULL default ' ',
  "AAABFNET" varchar(6) NOT NULL default '000000',
  "AAABFNEF" decimal(16,0) NOT NULL default '0',
  "AAABFNEG" decimal(16,0) NOT NULL default '0',
  "AAABFNEH" int(11) NOT NULL default '0',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABFNEM" int(11) NOT NULL default '0',
  "AAABFNEN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIANB","AAABFNES","AAABFNET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNDY" (
  "AAABIANB" varchar(30) NOT NULL default ' ',
  "AAABFNEP" varchar(10) NOT NULL default ' ',
  "AAABFNEQ" varchar(10) NOT NULL default ' ',
  "AAABFNER" varchar(1) NOT NULL default ' ',
  "AAABFNES" varchar(30) NOT NULL default ' ',
  "AAABFNET" varchar(6) NOT NULL default '000000',
  "AAABFNEF" decimal(16,0) NOT NULL default '0',
  "AAABFNEG" decimal(16,0) NOT NULL default '0',
  "AAABFNEH" int(11) NOT NULL default '0',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABFNEM" int(11) NOT NULL default '0',
  "AAABFNEN" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABIANB","AAABFNEP","AAABFNEQ","AAABFNER","AAABFNES","AAABFNET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABFNHN" varchar(10) NOT NULL default '0000000000',
  "AAABLTQJ" varchar(2) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABFNFG" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABFNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABFNHN" varchar(10) NOT NULL default '0000000000',
  "AAABJLKW" varchar(10) NOT NULL default '0000000000',
  "AAABFNFV" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFNFX" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABFNGB" int(11) NOT NULL default '0',
  "AAABFNGC" int(11) NOT NULL default '0',
  "AAABFNGD" varchar(10) NOT NULL default ' ',
  "AAABFNGE" varchar(50) NOT NULL default ' ',
  "AAABFNGF" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(4) NOT NULL default ' ',
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABFNHN","AAABJLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABFNHN" varchar(10) NOT NULL default '0000000000',
  "AAABFNGX" varchar(1) NOT NULL default ' ',
  "AAABFNGY" decimal(17,3) NOT NULL default '0.000',
  "AAABFNGZ" varchar(2) NOT NULL default ' ',
  "AAABFNHA" varchar(1) NOT NULL default ' ',
  "AAABFNHB" decimal(17,3) NOT NULL default '0.000',
  "AAABFNHC" varchar(2) NOT NULL default ' ',
  "AAABMKFH" varchar(1) NOT NULL default ' ',
  "AAABFNHE" varchar(1) NOT NULL default ' ',
  "AAABIVPQ" varchar(5) NOT NULL default ' ',
  "AAABFNHG" varchar(2) NOT NULL default ' ',
  "AAABFNHH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABFNHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABFNHW" int(11) NOT NULL default '0',
  "AAABLFTX" int(11) NOT NULL default '0',
  "AAABFNHY" int(11) NOT NULL default '0',
  "AAABFNHZ" int(11) NOT NULL default '0',
  "AAABFNIA" int(11) NOT NULL default '0',
  "AAABLFTW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABMDEN","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHSD" varchar(50) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHHB","AAABIHSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFNJL" varchar(1) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNJR" (
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTY" varchar(30) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFNTW","AAABFNTY","AAABFNTX","AAABFNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFNLF" varchar(30) NOT NULL default ' ',
  "AAABFNLG" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABIXHB" varchar(8) NOT NULL default '00000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFNLF","AAABFNLG","AAABMLPO","AAABIXHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default ' ',
  "AAABFNLO" varchar(1) NOT NULL default ' ',
  "AAABFNLP" varchar(60) NOT NULL default ' ',
  "AAABFNLQ" varchar(60) NOT NULL default ' ',
  "AAABFNLR" varchar(60) NOT NULL default ' ',
  "AAABFNLS" int(11) NOT NULL default '0',
  "AAABFNLT" int(11) NOT NULL default '0',
  "AAABFNLU" varchar(1) NOT NULL default ' ',
  "AAABFNLV" varchar(1) NOT NULL default ' ',
  "AAABFNLW" varchar(8) NOT NULL default '00000000',
  "AAABFNLX" varchar(8) NOT NULL default '00000000',
  "AAABFNLY" varchar(1) NOT NULL default ' ',
  "AAABFNLZ" varchar(1) NOT NULL default ' ',
  "AAABGAJY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABFNRB" varchar(1) NOT NULL default ' ',
  "AAABFNNM" varchar(1) NOT NULL default ' ',
  "AAABFNNN" varchar(1) NOT NULL default ' ',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAABHDLR" varchar(1) NOT NULL default ' ',
  "AAABFNNQ" varchar(1) NOT NULL default ' ',
  "AAABFNNR" varchar(1) NOT NULL default ' ',
  "AAABFNNS" varchar(1) NOT NULL default ' ',
  "AAABFNNT" varchar(1) NOT NULL default ' ',
  "AAABFNNU" varchar(1) NOT NULL default ' ',
  "AAABFNRI" varchar(1) NOT NULL default ' ',
  "AAABFNNW" varchar(10) NOT NULL default '0000000000',
  "AAABFNQG" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTX","AAABFNRW","AAABLLIX","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABFNRB" varchar(1) NOT NULL default ' ',
  "AAABFNNM" varchar(1) NOT NULL default ' ',
  "AAABFNNN" varchar(1) NOT NULL default ' ',
  "AAABGASY" varchar(1) NOT NULL default ' ',
  "AAABHDLR" varchar(1) NOT NULL default ' ',
  "AAABFNNQ" varchar(1) NOT NULL default ' ',
  "AAABFNNR" varchar(1) NOT NULL default ' ',
  "AAABFNNS" varchar(1) NOT NULL default ' ',
  "AAABFNNT" varchar(1) NOT NULL default ' ',
  "AAABFNNU" varchar(1) NOT NULL default ' ',
  "AAABFNRI" varchar(1) NOT NULL default ' ',
  "AAABFNNW" varchar(10) NOT NULL default '0000000000',
  "AAABFNQG" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTX","AAABFNRW","AAABLLIX","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNRO" varchar(4) NOT NULL default '0000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNRS" varchar(30) NOT NULL default ' ',
  "AAABFNRT" varchar(30) NOT NULL default ' ',
  "AAABFNRU" varchar(30) NOT NULL default ' ',
  "AAABFNRV" varchar(30) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABFNRB" varchar(1) NOT NULL default ' ',
  "AAABFNRC" varchar(1) NOT NULL default ' ',
  "AAABFNRD" varchar(1) NOT NULL default ' ',
  "AAABFNRE" varchar(1) NOT NULL default ' ',
  "AAABFNOU" varchar(1) NOT NULL default ' ',
  "AAABFNRF" varchar(1) NOT NULL default ' ',
  "AAABFNRG" varchar(1) NOT NULL default ' ',
  "AAABFNRH" varchar(1) NOT NULL default ' ',
  "AAABFNRI" varchar(1) NOT NULL default ' ',
  "AAABFNOZ" varchar(22) NOT NULL default ' ',
  "AAABFNPA" varchar(22) NOT NULL default ' ',
  "AAABFNPB" varchar(22) NOT NULL default ' ',
  "AAABFNPC" varchar(22) NOT NULL default ' ',
  "AAABFNPD" varchar(22) NOT NULL default ' ',
  "AAABFNPE" varchar(22) NOT NULL default ' ',
  "AAABFNPF" varchar(22) NOT NULL default ' ',
  "AAABFNPG" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNRO","AAABFNTW","AAABFNTX","AAABFNRS","AAABFNRT","AAABFNRU","AAABFNRV","AAABFNRW","AAABLLIX","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNRP" varchar(4) NOT NULL default ' ',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABFNRB" varchar(1) NOT NULL default ' ',
  "AAABFNRD" varchar(1) NOT NULL default ' ',
  "AAABLENL" varchar(1) NOT NULL default ' ',
  "AAABFNRG" varchar(1) NOT NULL default ' ',
  "AAABFNRE" varchar(1) NOT NULL default ' ',
  "AAABFNRI" varchar(1) NOT NULL default ' ',
  "AAABFNQG" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABFNRP","AAABFNTW","AAABFNTX","AAABFNRW","AAABLLIX","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNRO" varchar(4) NOT NULL default '0000',
  "AAABFNRP" varchar(4) NOT NULL default ' ',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNRS" varchar(30) NOT NULL default ' ',
  "AAABFNRT" varchar(30) NOT NULL default ' ',
  "AAABFNRU" varchar(30) NOT NULL default ' ',
  "AAABFNRV" varchar(30) NOT NULL default ' ',
  "AAABFNRW" varchar(12) NOT NULL default ' ',
  "AAABLLIX" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABFNRB" varchar(1) NOT NULL default ' ',
  "AAABFNRC" varchar(1) NOT NULL default ' ',
  "AAABFNRD" varchar(1) NOT NULL default ' ',
  "AAABFNRE" varchar(1) NOT NULL default ' ',
  "AAABFNRF" varchar(1) NOT NULL default ' ',
  "AAABFNRG" varchar(1) NOT NULL default ' ',
  "AAABFNRH" varchar(1) NOT NULL default ' ',
  "AAABFNRI" varchar(1) NOT NULL default ' ',
  "AAABFNRJ" varchar(22) NOT NULL default ' ',
  "AAABFNRK" varchar(22) NOT NULL default ' ',
  "AAABFNRL" varchar(22) NOT NULL default ' ',
  "AAABFNRM" varchar(22) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNRO","AAABFNRP","AAABFNTW","AAABFNTX","AAABFNRS","AAABFNRT","AAABFNRU","AAABFNRV","AAABFNRW","AAABLLIX","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABJLFF" varchar(5) NOT NULL default '00000',
  "AAABFNSE" varchar(1) NOT NULL default ' ',
  "AAABFNSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTX","AAABJLFF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTY" varchar(30) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNSU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTY","AAABFNTX","AAABFNSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKDLP" varchar(26) NOT NULL default ' ',
  "AAABJLFF" varchar(5) NOT NULL default '00000',
  "AAABMRCU" varchar(10) NOT NULL default '0000000000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKDLP","AAABJLFF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNTW" varchar(4) NOT NULL default ' ',
  "AAABFNTX" varchar(4) NOT NULL default ' ',
  "AAABFNTY" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABFNTR" varchar(1) NOT NULL default ' ',
  "AAABFNTS" varchar(2) NOT NULL default ' ',
  "AAABFNTT" varchar(50) NOT NULL default ' ',
  "AAABFNTU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNTW","AAABFNTX","AAABFNTY","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMIRY" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMIRY","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNUR" varchar(32) NOT NULL default ' ',
  "AAABFNUS" varchar(1) NOT NULL default ' ',
  "AAABIXHB" varchar(8) NOT NULL default '00000000',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABMMRE" varchar(72) NOT NULL default ' ',
  "AAABLUER" varchar(5) NOT NULL default ' ',
  "AAABFNUP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNUR","AAABFNUS","AAABIXHB","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJVQM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNVO" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNVO" varchar(16) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNVO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNVQ" (
  "AAABLEJI" varchar(30) NOT NULL default ' ',
  "AAABFNVZ" varchar(30) NOT NULL default ' ',
  "AAABILHK" varchar(10) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(30) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEJI","AAABFNVZ","AAABILHK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNWB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABLUEO" varchar(12) NOT NULL default ' ',
  "AAABFNXT" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABILGV" varchar(8) NOT NULL default '00000000',
  "AAABILGW" varchar(6) NOT NULL default '000000',
  "AAABFNWM" varchar(20) NOT NULL default ' ',
  "AAABFNWN" varchar(3) NOT NULL default '000',
  "AAABFNWO" varchar(3) NOT NULL default '000',
  "AAABFNWP" varchar(3) NOT NULL default '000',
  "AAABHJAV" decimal(6,4) NOT NULL default '0.0000',
  "AAABFNWR" decimal(6,4) NOT NULL default '0.0000',
  "AAABFNWS" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default ' ',
  "AAABFNWU" varchar(2) NOT NULL default ' ',
  "AAABFNWV" varchar(6) NOT NULL default '000000',
  "AAABFNWW" varchar(1) NOT NULL default ' ',
  "AAABFNWX" varchar(1) NOT NULL default ' ',
  "AAABFNWY" varchar(1) NOT NULL default ' ',
  "AAABFNWZ" varchar(6) NOT NULL default '000000',
  "AAABFNXA" varchar(100) NOT NULL default ' ',
  "AAABFNXB" varchar(100) NOT NULL default ' ',
  "AAABFNXC" varchar(2) NOT NULL default '00',
  "AAABFNXD" varchar(2) NOT NULL default '00',
  "AAABFNXE" varchar(10) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFNXG" varchar(1) NOT NULL default ' ',
  "AAABFNXH" varchar(1) NOT NULL default ' ',
  "AAABFNYF" varchar(6) NOT NULL default '000000',
  "AAABFNZA" varchar(6) NOT NULL default '000000',
  "AAABFNXK" varchar(8) NOT NULL default '00000000',
  "AAABFNXL" varchar(6) NOT NULL default '000000',
  "AAABFNXM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABLUEO","AAABFNXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNYF" varchar(6) NOT NULL default '000000',
  "AAABFNZB" varchar(6) NOT NULL default '000000',
  "AAABKVPX" varchar(6) NOT NULL default '000000',
  "AAABMLZG" varchar(6) NOT NULL default '000000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFNYB" varchar(50) NOT NULL default ' ',
  "AAABFNYC" varchar(6) NOT NULL default '000000',
  "AAABGZSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFNYF","AAABFNZB","AAABKVPX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFNZA" varchar(6) NOT NULL default '000000',
  "AAABFNZB" varchar(6) NOT NULL default '000000',
  "AAABFNZC" varchar(2) NOT NULL default '00',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFNYO" varchar(4) NOT NULL default ' ',
  "AAABMBIU" double NOT NULL default '0',
  "AAABFRJR" varchar(3) NOT NULL default ' ',
  "AAABFRJP" varchar(1) NOT NULL default ' ',
  "AAABFNYS" varchar(1) NOT NULL default '0',
  "AAABFNYT" double NOT NULL default '0',
  "AAABLZAE" varchar(5) NOT NULL default ' ',
  "AAABFNYV" int(11) NOT NULL default '0',
  "AAABFNYW" int(11) NOT NULL default '0',
  "AAABFYON" varchar(4) NOT NULL default '0000',
  "AAABFYOO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFNZA","AAABFNZB","AAABFNZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNZD" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABIJSC" varchar(10) NOT NULL default ' ',
  "AAABFNZI" varchar(40) NOT NULL default ' ',
  "AAABFNZJ" varchar(40) NOT NULL default ' ',
  "AAABFNZK" varchar(30) NOT NULL default ' ',
  "AAABFNZL" varchar(30) NOT NULL default ' ',
  "AAABFNZM" varchar(40) NOT NULL default ' ',
  "AAABFNZN" varchar(20) NOT NULL default ' ',
  "AAABFNZO" varchar(20) NOT NULL default ' ',
  "AAABFNZP" varchar(1) NOT NULL default ' ',
  "AAABFNZQ" varchar(1) NOT NULL default ' ',
  "AAABFNZR" varchar(1) NOT NULL default ' ',
  "AAABFNZS" varchar(1) NOT NULL default ' ',
  "AAABFNZT" varchar(1) NOT NULL default ' ',
  "AAABFNZU" varchar(1) NOT NULL default ' ',
  "AAABFNZV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABFOEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFNZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABLTXI" varchar(24) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABFOEJ","AAABFODY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOAN" (
  "AAABFOAQ" varchar(10) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABFOAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOAR" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFOBI" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAABGOAH" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABLZKF","AAABFOBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABLUEO" varchar(8) NOT NULL default ' ',
  "AAABFOBG" varchar(32) NOT NULL default ' ',
  "AAABFOBH" varchar(32) NOT NULL default ' ',
  "AAABFOBI" varchar(12) NOT NULL default ' ',
  "AAABFOBJ" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(8) NOT NULL default ' ',
  "AAABFOBL" varchar(8) NOT NULL default ' ',
  "AAABFOBM" varchar(1) NOT NULL default ' ',
  "AAABFOBN" varchar(1) NOT NULL default ' ',
  "AAABFOBO" varchar(1) NOT NULL default ' ',
  "AAABFOBP" varchar(1) NOT NULL default ' ',
  "AAABFOBQ" varchar(1) NOT NULL default ' ',
  "AAABFOBR" varchar(1) NOT NULL default ' ',
  "AAABFOBS" varchar(1) NOT NULL default ' ',
  "AAABFOBT" varchar(1) NOT NULL default ' ',
  "AAABFOBU" varchar(12) NOT NULL default ' ',
  "AAABFOBV" varchar(8) NOT NULL default '00000000',
  "AAABFOBW" varchar(32) NOT NULL default ' ',
  "AAABFOBX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABFOEJ","AAABFODY","AAABFODZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABFODJ" varchar(1) NOT NULL default ' ',
  "AAABFODK" varchar(4) NOT NULL default '0000',
  "AAABKNCF" varchar(32) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFOCQ" varchar(1) NOT NULL default ' ',
  "AAABFOCR" varchar(1) NOT NULL default ' ',
  "AAABFOCS" varchar(1) NOT NULL default ' ',
  "AAABFOCT" varchar(4) NOT NULL default '0000',
  "AAABFOCU" varchar(4) NOT NULL default '0000',
  "AAABFOCV" varchar(1) NOT NULL default ' ',
  "AAABJETU" varchar(32) NOT NULL default ' ',
  "AAABFOCX" varchar(1) NOT NULL default ' ',
  "AAABFOCY" varchar(1) NOT NULL default ' ',
  "AAABFOCZ" varchar(1) NOT NULL default ' ',
  "AAABFODA" varchar(1) NOT NULL default ' ',
  "AAABFODB" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABFOEJ","AAABFODY","AAABFODZ","AAABFODJ","AAABFODK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFODL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABFODZ" varchar(4) NOT NULL default '0000',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABFOEJ","AAABFODY","AAABFODZ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOEB" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABFOEJ","AAABMNJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABJELB" varchar(8) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFOEU" varchar(8) NOT NULL default ' ',
  "AAABFOEV" varchar(6) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFOEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMOHM","AAABMLPV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOFD" (
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
CREATE TABLE "AAABFOFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIXKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIXKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABIZPQ" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABIXKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOGR" (
  "AAABFOGY" varchar(16) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFOGY","AAABLECD","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOHB" (
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
  "AAABFOJA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABHFMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOJD" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOJJ" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOJO" (
  "AAABHFMT" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABHFMS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABHFMT","AAABHFMU","AAABIIMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(20) NOT NULL default ' ',
  "AAABKNGM" varchar(20) NOT NULL default ' ',
  "AAABFOKP" varchar(10) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLEML" varchar(20) NOT NULL default ' ',
  "AAABFQYW" varchar(3) NOT NULL default '000',
  "AAABKGBL" varchar(100) NOT NULL default ' ',
  "AAABLENH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLFYC","AAABMKFH","AAABKNGM","AAABFOKP","AAABMRFQ","AAABMLES","AAABJSAN","AAABLEML","AAABFQYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  "AAABFOLG" varchar(1) NOT NULL default ' ',
  "AAABFSGZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFUMB","AAABFSFW","AAABFOLG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABHCXM" varchar(1) NOT NULL default '0',
  "AAABFPGJ" varchar(5) NOT NULL default '00000',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABHCXM","AAABFPGJ","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOLS" (
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABFOMB" varchar(3) NOT NULL default '000',
  "AAABFOLW" varchar(40) NOT NULL default ' ',
  "AAABINFJ" varchar(40) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMPMH","AAABFOMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL","AAABFOOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFOOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL","AAABFOOU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRRL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL","AAABFOOU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFONJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABLXDN","AAABJGDL","AAABFOOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFONU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFOOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABLXDN","AAABJGDL","AAABFOOU","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFOOU" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLRRL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZKF","AAABLXDN","AAABJGDL","AAABFOOU","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAABGAJY" varchar(2) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFQWX" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFOPJ" varchar(1) NOT NULL default ' ',
  "AAABFOPK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFOPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFOPV" varchar(2) NOT NULL default ' ',
  "AAABFOPR" varchar(30) NOT NULL default ' ',
  "AAABFOPS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFOPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOPW" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMDCC" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMDCC" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABJDQT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  "AAABLXIS" varchar(50) NOT NULL default ' ',
  "AAABFOQT" int(11) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLRPY" varchar(8) NOT NULL default '00000000',
  "AAABLERE" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABFOQZ" varchar(32) NOT NULL default ' ',
  "AAABFORA" varchar(8) NOT NULL default '00000000',
  "AAABLESZ" varchar(6) NOT NULL default '000000',
  "AAABFOSW" varchar(1) NOT NULL default ' ',
  "AAABFOSX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFORH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFOSN" varchar(10) NOT NULL default '0000000000',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  "AAABJLKW" varchar(4) NOT NULL default '0000',
  "AAABLXIS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFOSN"),
  KEY "AAABFORP" ("AAABMRIL","AAABFOTU","AAABJLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFORT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFOSN" varchar(10) NOT NULL default '0000000000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABJLKW" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFOSN","AAABJECQ","AAABJLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFOSN" varchar(10) NOT NULL default '0000000000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFOSK" varchar(1) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFOSN","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABIYCS" varchar(4) NOT NULL default ' ',
  "AAABFOSV" varchar(1) NOT NULL default ' ',
  "AAABFOSW" varchar(1) NOT NULL default ' ',
  "AAABFOSX" varchar(1) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABLEQO" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABFOTE" int(11) NOT NULL default '0',
  "AAABFOTF" int(11) NOT NULL default '0',
  "AAABFOTG" int(11) NOT NULL default '0',
  "AAABFOTH" int(11) NOT NULL default '0',
  "AAABFOTI" int(11) NOT NULL default '0',
  "AAABLETI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOTU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOTU" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOVU" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOVU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOVU" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABFOUK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABFOVU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOVU" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFOUW" varchar(50) NOT NULL default ' ',
  "AAABFOUX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOVU","AAABMRCU","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOVU" varchar(4) NOT NULL default ' ',
  "AAABIXHB" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFOVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFOVU" varchar(4) NOT NULL default ' ',
  "AAABFOVQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABFOVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOVV" (
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
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOXL" (
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABGLSO" varchar(10) NOT NULL default ' ',
  "AAABIFSI" varchar(50) NOT NULL default ' ',
  "AAABIAET" varchar(8) NOT NULL default ' ',
  "AAABFOXT" varchar(20) NOT NULL default ' ',
  "AAABFOXU" varchar(1) NOT NULL default ' ',
  "AAABFOXV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLGEY","AAABJAAI","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOYA" (
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
  PRIMARY KEY ("AAABMRHK","AAABHZRB","AAABJCXS","AAABHZQN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOYW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFOZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABJLKW" varchar(2) NOT NULL default '00',
  "AAABFYON" varchar(4) NOT NULL default '0000',
  "AAABFYOO" varchar(4) NOT NULL default '0000',
  "AAABFOZT" varchar(1) NOT NULL default ' ',
  "AAABFOZU" varchar(2) NOT NULL default '00',
  "AAABFOZV" varchar(4) NOT NULL default '0000',
  "AAABFOZW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABJLKW","AAABFYON","AAABFYOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABJLKW" varchar(2) NOT NULL default '00',
  "AAABFYON" varchar(4) NOT NULL default '0000',
  "AAABFYOO" varchar(4) NOT NULL default '0000',
  "AAABHHOB" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMKAI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABJLKW","AAABFYON","AAABFYOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNOW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABHNOW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNOW" varchar(4) NOT NULL default ' ',
  "AAABFSJW" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABJHFC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABHNOW","AAABFSJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPCA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNOW" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABHNOW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPCO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFPZB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFPZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPCZ" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  "AAABHUVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABFPEC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWVJ"),
  KEY "AAABFPDL" ("AAABMRIL","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWVJ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWVJ" varchar(10) NOT NULL default ' ',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABFPEC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWVJ","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFPZB" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIWAM" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFPZB","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFPZB" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFPZB","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSUJ" varchar(1) NOT NULL default ' ',
  "AAABFPGJ" varchar(5) NOT NULL default '00000',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSUJ","AAABFPGJ","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSUJ" varchar(1) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAABFPGK" varchar(6) NOT NULL default '000000',
  "AAABLYMH" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABLUWW" varchar(1) NOT NULL default ' ',
  "AAABJTEQ" varchar(6) NOT NULL default '000000',
  "AAABMNMZ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSUJ","AAABFPGJ","AAABFPGK","AAABLYMH","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPGN" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQBN" varchar(3) NOT NULL default ' ',
  "AAABFQBO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQBN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQBN" varchar(3) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQBN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFPIA" varchar(1) NOT NULL default ' ',
  "AAABFPIB" varchar(1) NOT NULL default ' ',
  "AAABFRMF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUMK","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPIG" (
  "AAABFPII" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABFPII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPIQ" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABLTIX" varchar(20) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABLTIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPIZ" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMLPO" varchar(2) NOT NULL default '00',
  "AAABLMUZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPJI" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABMNLW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPJS" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABHLTE" varchar(4) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLWWP" varchar(4) NOT NULL default ' ',
  "AAABFPKJ" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFPKA" varchar(1) NOT NULL default ' ',
  "AAABFPKB" varchar(1) NOT NULL default ' ',
  "AAABFPKC" varchar(1) NOT NULL default ' ',
  "AAABFPKD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABHLTE","AAABMNYT","AAABLWWP","AAABFPKJ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPKL" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABLTIX" varchar(30) NOT NULL default ' ',
  "AAABFPKP" varchar(30) NOT NULL default ' ',
  "AAABFPKQ" varchar(30) NOT NULL default ' ',
  "AAABFPKR" varchar(30) NOT NULL default ' ',
  "AAABFPKS" varchar(30) NOT NULL default ' ',
  "AAABFPKT" varchar(30) NOT NULL default ' ',
  "AAABFPKU" varchar(30) NOT NULL default ' ',
  "AAABFPKV" varchar(30) NOT NULL default ' ',
  "AAABFPKW" varchar(30) NOT NULL default ' ',
  "AAABFPKX" varchar(30) NOT NULL default ' ',
  "AAABFPKY" varchar(30) NOT NULL default ' ',
  "AAABFPKZ" varchar(30) NOT NULL default ' ',
  "AAABFPLA" varchar(30) NOT NULL default ' ',
  "AAABFPLB" varchar(30) NOT NULL default ' ',
  "AAABFPLC" varchar(30) NOT NULL default ' ',
  "AAABFPLD" varchar(30) NOT NULL default ' ',
  "AAABFPLE" varchar(30) NOT NULL default ' ',
  "AAABFPLF" varchar(30) NOT NULL default ' ',
  "AAABFPLG" varchar(30) NOT NULL default ' ',
  "AAABFPLH" varchar(30) NOT NULL default ' ',
  "AAABFPLI" varchar(30) NOT NULL default ' ',
  "AAABFPLJ" varchar(30) NOT NULL default ' ',
  "AAABJARY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABJTCX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABJTCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABJTCX" varchar(20) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABJTCX","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABJLKW" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIWUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABJECQ","AAABJLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPOV" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFPOY" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPPB" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMLNA" int(11) NOT NULL default '0',
  "AAABFPPX" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  "AAABMLMY" varchar(6) NOT NULL default '000000',
  "AAABMLNB" varchar(1) NOT NULL default ' ',
  "AAABHVTT" varchar(40) NOT NULL default ' ',
  "AAABFPPK" varchar(1) NOT NULL default ' ',
  "AAABFPPL" varchar(1) NOT NULL default ' ',
  "AAABFPPM" tinyint(3) unsigned NOT NULL default '0',
  "AAABFPPN" varchar(1) NOT NULL default ' ',
  "AAABFPPO" varchar(1) NOT NULL default ' ',
  "AAABFPPP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABMLNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPPT" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFPQD" varchar(2) NOT NULL default '00',
  "AAABFPPX" varchar(30) NOT NULL default ' ',
  "AAABLUEL" varchar(30) NOT NULL default ' ',
  "AAABKRAD" varchar(1) NOT NULL default ' ',
  "AAABKRAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABFPQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFPRE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABJTCX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABJTCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFPTH" decimal(2,0) NOT NULL default '0',
  "AAABFPTI" varchar(7) NOT NULL default '0000000',
  "AAABFPTJ" varchar(7) NOT NULL default '0000000',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFPTL" varchar(3) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFPTN" varchar(1) NOT NULL default ' ',
  "AAABFPTO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFPXO" varchar(1) NOT NULL default ' ',
  "AAABFSRB" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABHBAZ" varchar(1) NOT NULL default ' ',
  "AAABFPVV" varchar(4) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABHBAZ" varchar(1) NOT NULL default ' ',
  "AAABFQYI" varchar(10) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABHBAZ" varchar(1) NOT NULL default ' ',
  "AAABFPXM" varchar(4) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABFPXO" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFRBZ" varchar(4) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABHBAZ" varchar(1) NOT NULL default ' ',
  "AAABFPZB" varchar(3) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFPZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFPZW" varchar(4) NOT NULL default ' ',
  "AAABFPZX" varchar(8) NOT NULL default ' ',
  "AAABFPZY" varchar(3) NOT NULL default '000',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFRKZ" varchar(1) NOT NULL default ' ',
  "AAABFQAS" varchar(1) NOT NULL default ' ',
  "AAABFSRB" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABFQAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFQBN" varchar(3) NOT NULL default ' ',
  "AAABFQBO" varchar(1) NOT NULL default ' ',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLQSF" varchar(1) NOT NULL default ' ',
  "AAABFQCI" varchar(1) NOT NULL default ' ',
  "AAABFQCJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABFQFU" varchar(50) NOT NULL default ' ',
  "AAABFQFV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABLUSV","AAABMJWN","AAABKAFO","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABIJTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABLUSV","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABFQMT" varchar(8) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABFQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABFQMT" varchar(8) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABJLKW" varchar(4) NOT NULL default '0000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  "AAABMNMP" varchar(4) NOT NULL default '0000',
  "AAABIZDJ" varchar(1) NOT NULL default ' ',
  "AAABIWUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABFQMT","AAABJECQ","AAABJLKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABFQMT" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQMS","AAABFQMT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABGKTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABFQMS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABFQMS","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABLUSV" varchar(4) NOT NULL default ' ',
  "AAABMJWN" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABFQMS","AAABLUSV","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABFQMS" varchar(8) NOT NULL default ' ',
  "AAABFQMT" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABFQMS","AAABFQMT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFQPE" varchar(8) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFQNN" varchar(1) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABFQPE","AAABIHQV","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQNW" (
  "AAABFQOM" varchar(10) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFQOA" varchar(2) NOT NULL default ' ',
  "AAABFQOB" varchar(2) NOT NULL default ' ',
  "AAABFQOC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABFQOM","AAABIHQV","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFQPE" varchar(8) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABIVVF" varchar(12) NOT NULL default ' ',
  "AAABFQOM" varchar(10) NOT NULL default ' ',
  "AAABLECU" varchar(12) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFQPE","AAABIHQV","AAABIHQW","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFQPE" varchar(8) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABIVVF" varchar(12) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFQPC" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFQPE","AAABIHQV","AAABIHQW","AAABIVVF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLUVS" varchar(8) NOT NULL default ' ',
  "AAABLEUZ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLUVS","AAABLEUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQPW" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMMRM" varchar(4) NOT NULL default ' ',
  "AAABFQQB" varchar(40) NOT NULL default ' ',
  "AAABFQQC" varchar(32) NOT NULL default ' ',
  "AAABFQQD" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQQH" (
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFQQK" varchar(40) NOT NULL default ' ',
  "AAABFQQL" varchar(40) NOT NULL default ' ',
  "AAABFQQM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHQV","AAABIHQW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFQRS" varchar(10) NOT NULL default ' ',
  "AAABFQQW" varchar(50) NOT NULL default ' ',
  "AAABJEVQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABJECQ","AAABFQRS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABFQRS" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTUP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABJECQ","AAABFQRS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFRRT" varchar(30) NOT NULL default ' ',
  "AAABJQDC" varchar(30) NOT NULL default ' ',
  "AAABFQRZ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFRRT","AAABJQDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQUF" varchar(3) NOT NULL default ' ',
  "AAABIJNF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFQUF" varchar(3) NOT NULL default ' ',
  "AAABFQTA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABFQUF","AAABFQTA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQTB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQUF" varchar(3) NOT NULL default ' ',
  "AAABLUSV" varchar(10) NOT NULL default ' ',
  "AAABLFYJ" varchar(10) NOT NULL default ' ',
  "AAABFQTJ" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQUF","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQUF" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABIHQW","AAABMREL","AAABFQUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABHZQZ" varchar(10) NOT NULL default ' ',
  "AAABGPVW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFUWK","AAABHZQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQVU","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQVU" varchar(3) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQVU","AAABMOHM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQYI" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQYI","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQYI" varchar(10) NOT NULL default ' ',
  "AAABFQWQ" varchar(10) NOT NULL default ' ',
  "AAABJFLP" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABFQWT" varchar(12) NOT NULL default ' ',
  "AAABFQWU" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFQWX" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFQWZ" varchar(7) NOT NULL default '0000000',
  "AAABFQXA" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQYI" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMLPO" varchar(3) NOT NULL default '000',
  "AAABMMJT" decimal(15,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQYI","AAABMREF","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFQYI" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFQYI","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQYK" (
  "AAABFQYU" varchar(2) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFQYW" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABFQYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQYP" (
  "AAABFQYU" varchar(2) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABFQYW" varchar(3) NOT NULL default '000',
  "AAABMMBE" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABFQYU","AAABJSAN","AAABFQYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLPGZ" varchar(5) NOT NULL default ' ',
  "AAABFQZD" varchar(30) NOT NULL default ' ',
  "AAABFQZE" varchar(30) NOT NULL default ' ',
  "AAABFQZF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLPGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFQZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLPGZ" varchar(5) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFQZS" varchar(20) NOT NULL default ' ',
  "AAABFQZT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABLPGZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRBZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRBZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRBZ" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRBZ","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRBZ" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABLPGZ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRBZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRBZ" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRBZ","AAABMRCU","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRCC" (
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
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFREJ" varchar(1) NOT NULL default ' ',
  "AAABFREK" varchar(1) NOT NULL default ' ',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABFREM" varchar(1) NOT NULL default ' ',
  "AAABFREN" varchar(2) NOT NULL default '00',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABFREP" varchar(8) NOT NULL default '00000000',
  "AAABFREQ" varchar(12) NOT NULL default ' ',
  "AAABLPYC" varchar(8) NOT NULL default '00000000',
  "AAABLPYD" varchar(6) NOT NULL default '000000',
  "AAABFRWJ" varchar(8) NOT NULL default '00000000',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  "AAABIIVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABKMGT" varchar(10) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABFRFY" varchar(20) NOT NULL default ' ',
  "AAABFRFZ" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(12) NOT NULL default ' ',
  "AAABFRJI" varchar(1) NOT NULL default ' ',
  "AAABFRJJ" varchar(1) NOT NULL default ' ',
  "AAABFRJK" varchar(4) NOT NULL default '0000',
  "AAABFRJL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFRHR" varchar(4) NOT NULL default '0000',
  "AAABFRGS" varchar(4) NOT NULL default '0000',
  "AAABFRGT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABFRHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFRHR" varchar(4) NOT NULL default '0000',
  "AAABKMGT" varchar(10) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABFRHR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRHS" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMNYU" varchar(12) NOT NULL default ' ',
  "AAABFRIN" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default '0000',
  "AAABIHPX" varchar(60) NOT NULL default ' ',
  "AAABFRIH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMNYU","AAABFRIN","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFRIY" varchar(1) NOT NULL default ' ',
  "AAABGWNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL","AAABFRIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABHTVP" varchar(10) NOT NULL default ' ',
  "AAABFRJF" varchar(1) NOT NULL default ' ',
  "AAABFRJG" varchar(1) NOT NULL default ' ',
  "AAABFRJH" varchar(30) NOT NULL default ' ',
  "AAABFRJI" varchar(1) NOT NULL default ' ',
  "AAABFRJJ" varchar(1) NOT NULL default ' ',
  "AAABFRJK" varchar(4) NOT NULL default '0000',
  "AAABFRJL" varchar(4) NOT NULL default '0000',
  "AAABLTQM" varchar(4) NOT NULL default '0000',
  "AAABIHSD" varchar(32) NOT NULL default ' ',
  "AAABFRJO" varchar(1) NOT NULL default ' ',
  "AAABFRJP" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFRJR" varchar(3) NOT NULL default ' ',
  "AAABFRJS" varchar(1) NOT NULL default ' ',
  "AAABFRJT" varchar(1) NOT NULL default ' ',
  "AAABFRJU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABHTVO","AAABJECQ","AAABHTVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABHTVO" varchar(1) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABHTVP" varchar(10) NOT NULL default ' ',
  "AAABKMGT" varchar(10) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  "AAABFRKK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABIHQV","AAABHTVO","AAABJECQ","AAABHTVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRMF" varchar(4) NOT NULL default ' ',
  "AAABFRKX" varchar(1) NOT NULL default ' ',
  "AAABFRKY" varchar(3) NOT NULL default ' ',
  "AAABFRKZ" varchar(1) NOT NULL default ' ',
  "AAABFRNS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRMF" varchar(4) NOT NULL default ' ',
  "AAABFRLS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRMF","AAABFRLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRMF" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRMF","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRNS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRNS" varchar(4) NOT NULL default ' ',
  "AAABFRNF" varchar(30) NOT NULL default ' ',
  "AAABFRMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRNS","AAABFRNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRNS" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRNS","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRNU" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABIYCS" varchar(20) NOT NULL default ' ',
  "AAABHZQN" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABMDXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFROC" (
  "AAABMDXP" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDXP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFROI" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABFROM" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFROP" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABMMJT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLXDN","AAABIHHB","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFROX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRPC" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABFRPE" varchar(2) NOT NULL default ' ',
  "AAABFRPF" varchar(2) NOT NULL default ' ',
  "AAABFRPG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(60) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABLQGL" varchar(1) NOT NULL default ' ',
  "AAABKQHD" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFRQF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(4) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default '0',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(4) NOT NULL default ' ',
  "AAABFRRT" varchar(30) NOT NULL default ' ',
  "AAABFRRU" varchar(6) NOT NULL default '000000',
  "AAABLQRH" varchar(30) NOT NULL default ' ',
  "AAABFRTL" varchar(1) NOT NULL default '0',
  "AAABJSAN" varchar(4) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABFRRZ" decimal(15,0) NOT NULL default '0',
  "AAABFRSA" decimal(15,0) NOT NULL default '0',
  "AAABFRSB" decimal(15,0) NOT NULL default '0',
  "AAABFRSC" decimal(15,0) NOT NULL default '0',
  "AAABFRSX" decimal(15,2) NOT NULL default '0.00',
  "AAABFRSE" decimal(15,2) NOT NULL default '0.00',
  "AAABFRSF" decimal(15,0) NOT NULL default '0',
  "AAABFRSG" decimal(15,2) NOT NULL default '0.00',
  "AAABFRSH" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABFRSX" decimal(15,2) NOT NULL default '0.00',
  "AAABFRSY" decimal(15,0) NOT NULL default '0',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRTG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(4) NOT NULL default ' ',
  "AAABFRTL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMREL","AAABIIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABIIUE" varchar(3) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(4) NOT NULL default ' ',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABFRUM" varchar(30) NOT NULL default ' ',
  "AAABFRUN" varchar(30) NOT NULL default ' ',
  "AAABFRUO" varchar(30) NOT NULL default ' ',
  "AAABFRUP" varchar(30) NOT NULL default ' ',
  "AAABFRUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFRWI","AAABIIUE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRUX" (
  "AAABLOSZ" varchar(10) NOT NULL default ' ',
  "AAABFRVC" varchar(20) NOT NULL default ' ',
  "AAABFRVA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOSZ","AAABFRVC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABJQCM" varchar(12) NOT NULL default ' ',
  "AAABJQCK" varchar(8) NOT NULL default '00000000',
  "AAABJQCL" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKFU" varchar(14) NOT NULL default '00000000000000',
  "AAABKZAZ" varchar(6) NOT NULL default '000000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFRVX" varchar(2) NOT NULL default '00',
  "AAABFRVY" varchar(12) NOT NULL default ' ',
  "AAABFRVZ" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABFRWB" decimal(15,0) NOT NULL default '0',
  "AAABFRWC" decimal(15,0) NOT NULL default '0',
  "AAABLTIZ" decimal(15,2) NOT NULL default '0.00',
  "AAABFRWE" decimal(15,0) NOT NULL default '0',
  "AAABFRWF" decimal(15,0) NOT NULL default '0',
  "AAABFRWG" decimal(15,2) NOT NULL default '0.00',
  "AAABFRWH" varchar(2) NOT NULL default '00',
  "AAABFRWI" varchar(6) NOT NULL default '000000',
  "AAABFRWJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMKFU","AAABKZAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABFRXF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABJECQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQRH" varchar(30) NOT NULL default ' ',
  "AAABJECQ" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(4) NOT NULL default '0000',
  "AAABMMJT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQRH","AAABJECQ","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABMREL","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABMREL","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSAB" varchar(14) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFSAB","AAABMMVH","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSAB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABMREL","AAABFSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFRZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSAB" varchar(14) NOT NULL default ' ',
  "AAABFRZV" varchar(20) NOT NULL default ' ',
  "AAABIWTP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABMREL","AAABFSAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABIJNY" int(11) NOT NULL default '0',
  "AAABFSAL" int(11) NOT NULL default '0',
  "AAABFSAM" int(11) NOT NULL default '0',
  "AAABFSAN" int(11) NOT NULL default '0',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABLEST","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMMVH" varchar(12) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABHFNU" varchar(46) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIHQV","AAABIHQW","AAABMREL","AAABMMVH","AAABMOIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSFU" varchar(2) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSFU","AAABHNOA","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  "AAABMLTT" varchar(2) NOT NULL default '00',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABMPTM" varchar(30) NOT NULL default ' ',
  "AAABFSCQ" varchar(1) NOT NULL default ' ',
  "AAABFSCR" varchar(1) NOT NULL default ' ',
  "AAABFSCS" varchar(3) NOT NULL default ' ',
  "AAABFSCT" varchar(3) NOT NULL default ' ',
  "AAABFSCU" varchar(30) NOT NULL default ' ',
  "AAABFSCV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSRE","AAABMLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSHV" varchar(3) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  "AAABFSDE" varchar(2) NOT NULL default '00',
  "AAABFSDF" varchar(1) NOT NULL default ' ',
  "AAABFSDG" varchar(8) NOT NULL default '00000000',
  "AAABFSDH" varchar(7) NOT NULL default '0000000',
  "AAABFSDI" varchar(1) NOT NULL default ' ',
  "AAABFSDJ" varchar(1) NOT NULL default ' ',
  "AAABLIQO" varchar(1) NOT NULL default ' ',
  "AAABFSDL" varchar(1) NOT NULL default ' ',
  "AAABMCVW" varchar(1) NOT NULL default ' ',
  "AAABLRBV" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABFSDP" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(1) NOT NULL default ' ',
  "AAABFSDR" varchar(1) NOT NULL default '0',
  "AAABFSDS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFSHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSFU" varchar(2) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  "AAABLUJZ" varchar(8) NOT NULL default '00000000',
  "AAABFSEV" varchar(3) NOT NULL default ' ',
  "AAABFSEW" varchar(3) NOT NULL default ' ',
  "AAABFSEX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSFU","AAABHNOA","AAABFUWK","AAABLRMN","AAABLUJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSFU" varchar(2) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABMLCT" varchar(4) NOT NULL default ' ',
  "AAABLUJZ" varchar(8) NOT NULL default '00000000',
  "AAABFSEV" varchar(3) NOT NULL default ' ',
  "AAABFSEW" varchar(3) NOT NULL default ' ',
  "AAABFSEX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSFU","AAABHNOA","AAABFUWK","AAABMLCT","AAABLUJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSFU" varchar(2) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFSFW" varchar(3) NOT NULL default '000',
  "AAABFSFX" varchar(1) NOT NULL default ' ',
  "AAABFSFM" varchar(30) NOT NULL default ' ',
  "AAABFSFN" varchar(30) NOT NULL default ' ',
  "AAABFSFO" varchar(30) NOT NULL default ' ',
  "AAABFSFP" varchar(30) NOT NULL default ' ',
  "AAABFSFQ" varchar(30) NOT NULL default ' ',
  "AAABFSFR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSFU","AAABFUMB","AAABFSFW","AAABFSFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSHV" varchar(3) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSGZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSHV","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSGO" varchar(30) NOT NULL default ' ',
  "AAABFSGP" varchar(30) NOT NULL default ' ',
  "AAABFSGL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSGO","AAABFSGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMAMV" varchar(4) NOT NULL default ' ',
  "AAABFSGZ" varchar(30) NOT NULL default ' ',
  "AAABFSGV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABMAMV","AAABFSGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABFSUJ" varchar(1) NOT NULL default ' ',
  "AAABFSHL" varchar(6) NOT NULL default '000000',
  "AAABFSHM" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKYZX" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABFSUJ","AAABFSHL","AAABFSHM","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSHV" varchar(3) NOT NULL default ' ',
  "AAABFSHS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  "AAABFSIF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSKY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFSKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSKY" varchar(4) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFSKY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFSKY" varchar(4) NOT NULL default ' ',
  "AAABFSJW" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABFSKY","AAABFSJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  "AAABHNOW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABHNOW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHNOA" varchar(1) NOT NULL default ' ',
  "AAABFSKY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIHQV","AAABIHQW","AAABMREL","AAABHNOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFSLJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABKRXK","AAABMDBB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSMB" varchar(4) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  "AAABFSMD" varchar(4) NOT NULL default ' ',
  "AAABFSLT" varchar(1) NOT NULL default ' ',
  "AAABFSLU" varchar(30) NOT NULL default ' ',
  "AAABFSLV" varchar(30) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABFSLX" varchar(1) NOT NULL default ' ',
  "AAABMCXN" varchar(10) NOT NULL default '0000000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSMB","AAABFSUK","AAABFSMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSME" (
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLTYA" varchar(30) NOT NULL default ' ',
  "AAABFSMH" varchar(2) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXDN","AAABLTYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSML" (
  "AAABLOZE" varchar(6) NOT NULL default ' ',
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSMQ" (
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSMU" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  "AAABFSMX" varchar(1) NOT NULL default ' ',
  "AAABFSMY" varchar(1) NOT NULL default ' ',
  "AAABFSMZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSNC" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABHEGP","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSNK" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJTAQ" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABJTAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABGOOI" varchar(30) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHAGF" smallint(6) NOT NULL default '0',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAABMLCO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABGOOI","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLUSC" varchar(10) NOT NULL default '0000000000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFSOJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLUSC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABKQYU" varchar(12) NOT NULL default ' ',
  "AAABLUSG" decimal(16,0) NOT NULL default '0',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFSPB" varchar(16) NOT NULL default '0000000000000000',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLILW","AAABKQYU","AAABLUSG","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABFSPQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLILW","AAABFSPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLILW","AAABMQDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLILW","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABJGDL" varchar(4) NOT NULL default ' ',
  "AAABFSQO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABJGDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABIZMX" varchar(1) NOT NULL default ' ',
  "AAABFSUJ" varchar(1) NOT NULL default ' ',
  "AAABIIAV" varchar(1) NOT NULL default ' ',
  "AAABFSRB" varchar(1) NOT NULL default ' ',
  "AAABFSRC" varchar(1) NOT NULL default ' ',
  "AAABFSRD" varchar(1) NOT NULL default ' ',
  "AAABFSRE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSRN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFSTG" varchar(4) NOT NULL default '0000',
  "AAABFSSM" varchar(1) NOT NULL default ' ',
  "AAABFSWG" varchar(2) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABFSSB" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ","AAABFSTG","AAABFSSM","AAABFSWG","AAABKAFO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFSTG" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABFSSZ" decimal(5,2) NOT NULL default '0.00',
  "AAABFSTA" decimal(10,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ","AAABFSTG","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSWG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSWG" varchar(2) NOT NULL default ' ',
  "AAABFSTR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABKPXV" varchar(7) NOT NULL default '0000000',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMQCJ" varchar(10) NOT NULL default ' ',
  "AAABFSUD" varchar(12) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFSUF" varchar(1) NOT NULL default ' ',
  "AAABFSUG" varchar(10) NOT NULL default ' ',
  "AAABFSUH" varchar(12) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABFSUJ" varchar(1) NOT NULL default ' ',
  "AAABFSUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABKPXV","AAABMRCU","AAABMQCJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(50) NOT NULL default ' ',
  "AAABMNNA" varchar(50) NOT NULL default ' ',
  "AAABFSVE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ","AAABKAFO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFSWG" varchar(2) NOT NULL default ' ',
  "AAABFSWH" varchar(30) NOT NULL default ' ',
  "AAABFSVU" varchar(30) NOT NULL default ' ',
  "AAABFSVV" varchar(1) NOT NULL default ' ',
  "AAABFSVW" varchar(30) NOT NULL default ' ',
  "AAABFSVX" varchar(1) NOT NULL default ' ',
  "AAABFSVY" decimal(5,2) NOT NULL default '0.00',
  "AAABFSVZ" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ","AAABFSWG","AAABFSWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABFSWV" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAABFSWP" varchar(30) NOT NULL default ' ',
  "AAABFSWQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN","AAABLECD","AAABFSWV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  "AAABFSXD" varchar(3) NOT NULL default '000',
  "AAABFSXE" varchar(2) NOT NULL default ' ',
  "AAABFSXF" varchar(1) NOT NULL default ' ',
  "AAABFSXG" varchar(10) NOT NULL default ' ',
  "AAABFSXH" varchar(10) NOT NULL default ' ',
  "AAABFSXI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMCRJ","AAABMCMO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLODQ","AAABMCRJ","AAABMCMO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFSZB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFSZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFSZB" varchar(6) NOT NULL default ' ',
  "AAABFTPD" varchar(10) NOT NULL default ' ',
  "AAABFTPO" varchar(7) NOT NULL default '0000000',
  "AAABFSYK" varchar(1) NOT NULL default ' ',
  "AAABFSYL" varchar(10) NOT NULL default ' ',
  "AAABFSYM" varchar(1) NOT NULL default ' ',
  "AAABKQHR" varchar(1) NOT NULL default ' ',
  "AAABFSYO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFSZB","AAABFTPD","AAABFTPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFSZB" varchar(6) NOT NULL default ' ',
  "AAABIIVG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFSZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABFSZF" varchar(1) NOT NULL default ' ',
  "AAABFSZG" varchar(1) NOT NULL default ' ',
  "AAABFSZH" varchar(1) NOT NULL default ' ',
  "AAABFSZI" varchar(1) NOT NULL default ' ',
  "AAABFTMG" varchar(1) NOT NULL default ' ',
  "AAABFTMH" varchar(1) NOT NULL default ' ',
  "AAABFTMI" varchar(1) NOT NULL default ' ',
  "AAABFTMJ" varchar(1) NOT NULL default ' ',
  "AAABFTMK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFSZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABKRXL" varchar(2) NOT NULL default '00',
  "AAABFTAM" varchar(1) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABFTAO" varchar(1) NOT NULL default '0',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABFTAC" varchar(10) NOT NULL default ' ',
  "AAABFTAD" varchar(10) NOT NULL default ' ',
  "AAABFTAE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMREX","AAABKRXE","AAABJGHG","AAABKRGH","AAABKRXL","AAABFTAM","AAABKOTY","AAABFTAO","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABIIVG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTAY" (
  "AAABFTNK" varchar(2) NOT NULL default ' ',
  "AAABMLTT" varchar(1) NOT NULL default '0',
  "AAABFTBB" varchar(1) NOT NULL default ' ',
  "AAABFTBC" varchar(1) NOT NULL default ' ',
  "AAABFTBD" varchar(1) NOT NULL default ' ',
  "AAABFTBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTBG" (
  "AAABFTNK" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGGHA" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTNK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABIIVG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABICIU","AAABJGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABFTPD" varchar(10) NOT NULL default ' ',
  "AAABFTPE" varchar(4) NOT NULL default ' ',
  "AAABFTPF" varchar(10) NOT NULL default ' ',
  "AAABFTPG" varchar(6) NOT NULL default ' ',
  "AAABFTPI" varchar(1) NOT NULL default ' ',
  "AAABFTPJ" varchar(1) NOT NULL default ' ',
  "AAABFTPO" varchar(7) NOT NULL default '0000000',
  "AAABFTCI" decimal(5,2) NOT NULL default '0.00',
  "AAABFTOX" decimal(5,2) NOT NULL default '0.00',
  "AAABFTOY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJGHG","AAABKRGH","AAABFTPD","AAABFTPE","AAABFTPF","AAABFTPG","AAABFTPI","AAABFTPJ","AAABFTPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABICIU","AAABJGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABFTDH" varchar(7) NOT NULL default '0000000',
  "AAABFTDI" varchar(7) NOT NULL default '0000000',
  "AAABFTDJ" varchar(1) NOT NULL default '0',
  "AAABFTDK" varchar(1) NOT NULL default ' ',
  "AAABFTDL" varchar(1) NOT NULL default ' ',
  "AAABFTDM" varchar(10) NOT NULL default ' ',
  "AAABFTDN" varchar(10) NOT NULL default ' ',
  "AAABFTDO" varchar(10) NOT NULL default ' ',
  "AAABFTDP" varchar(10) NOT NULL default ' ',
  "AAABFTDQ" varchar(10) NOT NULL default ' ',
  "AAABFTDR" varchar(10) NOT NULL default ' ',
  "AAABFTDS" varchar(10) NOT NULL default ' ',
  "AAABFTDT" varchar(10) NOT NULL default ' ',
  "AAABFTDU" varchar(10) NOT NULL default ' ',
  "AAABFTDV" varchar(10) NOT NULL default ' ',
  "AAABFTDW" varchar(10) NOT NULL default ' ',
  "AAABFTDX" varchar(10) NOT NULL default ' ',
  "AAABFTDY" varchar(10) NOT NULL default ' ',
  "AAABFTDZ" varchar(10) NOT NULL default ' ',
  "AAABFTEA" varchar(10) NOT NULL default ' ',
  "AAABFTMG" varchar(1) NOT NULL default ' ',
  "AAABFTMH" varchar(1) NOT NULL default ' ',
  "AAABFTMI" varchar(1) NOT NULL default ' ',
  "AAABFTMJ" varchar(1) NOT NULL default ' ',
  "AAABFTMK" varchar(1) NOT NULL default ' ',
  "AAABFTEG" varchar(10) NOT NULL default ' ',
  "AAABFTEH" varchar(10) NOT NULL default ' ',
  "AAABFTEI" varchar(1) NOT NULL default ' ',
  "AAABFTML" varchar(1) NOT NULL default ' ',
  "AAABFTMR" varchar(1) NOT NULL default ' ',
  "AAABFTMS" varchar(1) NOT NULL default ' ',
  "AAABFTEM" varchar(1) NOT NULL default ' ',
  "AAABFTEN" varchar(1) NOT NULL default ' ',
  "AAABFTMT" varchar(1) NOT NULL default ' ',
  "AAABFTNC" varchar(1) NOT NULL default ' ',
  "AAABFTND" varchar(1) NOT NULL default ' ',
  "AAABFTNE" varchar(1) NOT NULL default ' ',
  "AAABFTES" varchar(10) NOT NULL default ' ',
  "AAABFTET" varchar(3) NOT NULL default ' ',
  "AAABIEIX" varchar(1) NOT NULL default '0',
  "AAABFTEV" varchar(1) NOT NULL default ' ',
  "AAABFTEW" varchar(1) NOT NULL default ' ',
  "AAABFTLG" varchar(3) NOT NULL default ' ',
  "AAABFTEY" varchar(1) NOT NULL default ' ',
  "AAABFTEZ" varchar(1) NOT NULL default ' ',
  "AAABFTFA" varchar(1) NOT NULL default ' ',
  "AAABFTFB" varchar(1) NOT NULL default ' ',
  "AAABFTFC" varchar(1) NOT NULL default ' ',
  "AAABFTFD" varchar(10) NOT NULL default ' ',
  "AAABFTFE" varchar(10) NOT NULL default ' ',
  "AAABFTFF" varchar(10) NOT NULL default ' ',
  "AAABFTFG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABFTOC" varchar(3) NOT NULL default ' ',
  "AAABFTFQ" varchar(1) NOT NULL default ' ',
  "AAABFTFR" varchar(10) NOT NULL default ' ',
  "AAABFTFS" varchar(10) NOT NULL default ' ',
  "AAABFTFT" varchar(10) NOT NULL default ' ',
  "AAABFTFU" varchar(10) NOT NULL default ' ',
  "AAABFTFV" varchar(10) NOT NULL default ' ',
  "AAABFTFW" decimal(5,2) NOT NULL default '0.00',
  "AAABFTFX" decimal(5,2) NOT NULL default '0.00',
  "AAABKPVP" varchar(10) NOT NULL default ' ',
  "AAABFTFZ" varchar(3) NOT NULL default ' ',
  "AAABFTGA" varchar(3) NOT NULL default ' ',
  "AAABFTGB" varchar(3) NOT NULL default ' ',
  "AAABFTMG" varchar(1) NOT NULL default ' ',
  "AAABFTMH" varchar(1) NOT NULL default ' ',
  "AAABFTMI" varchar(1) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG","AAABKRGH","AAABFTOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABLKXR" varchar(8) NOT NULL default ' ',
  "AAABJFHC" varchar(5) NOT NULL default ' ',
  "AAABFTKW" varchar(2) NOT NULL default '00',
  "AAABFTKX" varchar(1) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABFTKZ" varchar(1) NOT NULL default ' ',
  "AAABFTLA" varchar(1) NOT NULL default ' ',
  "AAABFTLB" varchar(1) NOT NULL default ' ',
  "AAABFTLC" varchar(1) NOT NULL default ' ',
  "AAABFTLD" varchar(1) NOT NULL default ' ',
  "AAABFTLE" varchar(1) NOT NULL default ' ',
  "AAABFTLF" varchar(1) NOT NULL default ' ',
  "AAABFTLG" varchar(3) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFTLI" varchar(3) NOT NULL default ' ',
  "AAABFTLJ" varchar(1) NOT NULL default ' ',
  "AAABFTLK" varchar(1) NOT NULL default ' ',
  "AAABFTLL" varchar(1) NOT NULL default ' ',
  "AAABFTLM" varchar(1) NOT NULL default ' ',
  "AAABFTLN" varchar(1) NOT NULL default ' ',
  "AAABFTLO" varchar(1) NOT NULL default ' ',
  "AAABFTLP" varchar(1) NOT NULL default ' ',
  "AAABFTLQ" varchar(1) NOT NULL default ' ',
  "AAABFTLR" varchar(1) NOT NULL default ' ',
  "AAABFTLS" varchar(1) NOT NULL default ' ',
  "AAABFTLT" varchar(1) NOT NULL default ' ',
  "AAABFTLU" varchar(1) NOT NULL default ' ',
  "AAABFTLV" varchar(1) NOT NULL default ' ',
  "AAABFTLW" varchar(1) NOT NULL default ' ',
  "AAABFTLX" varchar(1) NOT NULL default ' ',
  "AAABFTLY" varchar(1) NOT NULL default ' ',
  "AAABFTLZ" varchar(1) NOT NULL default ' ',
  "AAABFTMA" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMB" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMC" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMD" decimal(15,2) NOT NULL default '0.00',
  "AAABFTME" varchar(1) NOT NULL default ' ',
  "AAABFTMF" varchar(1) NOT NULL default ' ',
  "AAABFTMG" varchar(1) NOT NULL default ' ',
  "AAABFTMH" varchar(1) NOT NULL default ' ',
  "AAABFTMI" varchar(1) NOT NULL default ' ',
  "AAABFTMJ" varchar(1) NOT NULL default ' ',
  "AAABFTMK" varchar(1) NOT NULL default ' ',
  "AAABFTML" varchar(1) NOT NULL default ' ',
  "AAABFTMM" decimal(5,2) NOT NULL default '0.00',
  "AAABFTOC" varchar(3) NOT NULL default ' ',
  "AAABFTMO" varchar(1) NOT NULL default ' ',
  "AAABFTMP" varchar(1) NOT NULL default ' ',
  "AAABFTMQ" varchar(3) NOT NULL default ' ',
  "AAABFTMR" varchar(1) NOT NULL default ' ',
  "AAABFTMS" varchar(1) NOT NULL default ' ',
  "AAABFTMT" varchar(1) NOT NULL default ' ',
  "AAABFTMU" varchar(3) NOT NULL default ' ',
  "AAABFTMV" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMW" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMX" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMY" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMZ" decimal(5,2) NOT NULL default '0.00',
  "AAABFTNA" varchar(1) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABFTNC" varchar(1) NOT NULL default ' ',
  "AAABFTND" varchar(1) NOT NULL default ' ',
  "AAABFTNE" varchar(1) NOT NULL default ' ',
  "AAABFTNF" varchar(1) NOT NULL default ' ',
  "AAABFTNG" varchar(1) NOT NULL default ' ',
  "AAABFTNH" varchar(1) NOT NULL default ' ',
  "AAABKMOG" varchar(8) NOT NULL default ' ',
  "AAABMPTM" varchar(1) NOT NULL default ' ',
  "AAABFTJG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG","AAABKRGH","AAABLKXR","AAABJFHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABFTJS" varchar(1) NOT NULL default ' ',
  "AAABLRVC" varchar(5) NOT NULL default ' ',
  "AAABLRVD" varchar(5) NOT NULL default ' ',
  "AAABFTNK" varchar(2) NOT NULL default ' ',
  "AAABFTLV" varchar(1) NOT NULL default ' ',
  "AAABFTLA" varchar(1) NOT NULL default ' ',
  "AAABFTMA" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMB" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMC" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMD" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMK" varchar(1) NOT NULL default ' ',
  "AAABKMOG" varchar(8) NOT NULL default ' ',
  "AAABFTLI" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG","AAABKRGH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABIIVG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABJGHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTKT" (
  "AAABFTNK" varchar(2) NOT NULL default ' ',
  "AAABFTNL" varchar(1) NOT NULL default ' ',
  "AAABFTKW" varchar(2) NOT NULL default '00',
  "AAABFTKX" varchar(1) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABFTKZ" varchar(1) NOT NULL default ' ',
  "AAABFTLA" varchar(1) NOT NULL default ' ',
  "AAABFTLB" varchar(1) NOT NULL default ' ',
  "AAABFTLC" varchar(1) NOT NULL default ' ',
  "AAABFTLD" varchar(1) NOT NULL default ' ',
  "AAABFTLE" varchar(1) NOT NULL default ' ',
  "AAABFTLF" varchar(1) NOT NULL default ' ',
  "AAABFTLG" varchar(3) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFTLI" varchar(3) NOT NULL default ' ',
  "AAABFTLJ" varchar(1) NOT NULL default ' ',
  "AAABFTLK" varchar(1) NOT NULL default ' ',
  "AAABFTLL" varchar(1) NOT NULL default ' ',
  "AAABFTLM" varchar(1) NOT NULL default ' ',
  "AAABFTLN" varchar(1) NOT NULL default ' ',
  "AAABFTLO" varchar(1) NOT NULL default ' ',
  "AAABFTLP" varchar(1) NOT NULL default ' ',
  "AAABFTLQ" varchar(1) NOT NULL default ' ',
  "AAABFTLR" varchar(1) NOT NULL default ' ',
  "AAABFTLS" varchar(1) NOT NULL default ' ',
  "AAABFTLT" varchar(1) NOT NULL default ' ',
  "AAABFTLU" varchar(1) NOT NULL default ' ',
  "AAABFTLV" varchar(1) NOT NULL default ' ',
  "AAABFTLW" varchar(1) NOT NULL default ' ',
  "AAABFTLX" varchar(1) NOT NULL default ' ',
  "AAABFTLY" varchar(1) NOT NULL default ' ',
  "AAABFTLZ" varchar(1) NOT NULL default ' ',
  "AAABFTMA" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMB" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMC" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMD" decimal(15,2) NOT NULL default '0.00',
  "AAABFTME" varchar(1) NOT NULL default ' ',
  "AAABFTMF" varchar(1) NOT NULL default ' ',
  "AAABFTMG" varchar(1) NOT NULL default ' ',
  "AAABFTMH" varchar(1) NOT NULL default ' ',
  "AAABFTMI" varchar(1) NOT NULL default ' ',
  "AAABFTMJ" varchar(1) NOT NULL default ' ',
  "AAABFTMK" varchar(1) NOT NULL default ' ',
  "AAABFTML" varchar(1) NOT NULL default ' ',
  "AAABFTMM" decimal(5,2) NOT NULL default '0.00',
  "AAABFTOC" varchar(3) NOT NULL default ' ',
  "AAABFTMO" varchar(1) NOT NULL default ' ',
  "AAABFTMP" varchar(1) NOT NULL default ' ',
  "AAABFTMQ" varchar(3) NOT NULL default ' ',
  "AAABFTMR" varchar(1) NOT NULL default ' ',
  "AAABFTMS" varchar(1) NOT NULL default ' ',
  "AAABFTMT" varchar(1) NOT NULL default ' ',
  "AAABFTMU" varchar(3) NOT NULL default ' ',
  "AAABFTMV" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMW" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMX" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMY" decimal(15,2) NOT NULL default '0.00',
  "AAABFTMZ" decimal(5,2) NOT NULL default '0.00',
  "AAABFTNA" varchar(1) NOT NULL default ' ',
  "AAABFTNB" varchar(3) NOT NULL default ' ',
  "AAABFTNC" varchar(1) NOT NULL default ' ',
  "AAABFTND" varchar(1) NOT NULL default ' ',
  "AAABFTNE" varchar(1) NOT NULL default ' ',
  "AAABFTNF" varchar(1) NOT NULL default ' ',
  "AAABFTNG" varchar(1) NOT NULL default ' ',
  "AAABFTNH" varchar(1) NOT NULL default ' ',
  "AAABKMOG" varchar(8) NOT NULL default ' ',
  "AAABMPTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTNK","AAABFTNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFTOC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFTOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFTOC" varchar(3) NOT NULL default ' ',
  "AAABFTNY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABFTOC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABJGHG" varchar(3) NOT NULL default ' ',
  "AAABKRGH" varchar(6) NOT NULL default ' ',
  "AAABFTPD" varchar(10) NOT NULL default ' ',
  "AAABFTPE" varchar(4) NOT NULL default ' ',
  "AAABFTPF" varchar(10) NOT NULL default ' ',
  "AAABFTPG" varchar(6) NOT NULL default ' ',
  "AAABFTPH" varchar(12) NOT NULL default ' ',
  "AAABFTPI" varchar(1) NOT NULL default ' ',
  "AAABFTPJ" varchar(1) NOT NULL default ' ',
  "AAABFTPK" varchar(2) NOT NULL default ' ',
  "AAABFTPL" varchar(1) NOT NULL default ' ',
  "AAABFTPM" varchar(1) NOT NULL default ' ',
  "AAABFTPN" varchar(2) NOT NULL default ' ',
  "AAABFTPO" varchar(7) NOT NULL default '0000000',
  "AAABFTOU" varchar(3) NOT NULL default ' ',
  "AAABFTOV" varchar(3) NOT NULL default ' ',
  "AAABFTOW" varchar(3) NOT NULL default ' ',
  "AAABFTOX" decimal(5,2) NOT NULL default '0.00',
  "AAABFTOY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABJGHG","AAABKRGH","AAABFTPD","AAABFTPE","AAABFTPF","AAABFTPG","AAABFTPH","AAABFTPI","AAABFTPJ","AAABFTPK","AAABFTPL","AAABFTPM","AAABFTPN","AAABFTPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTUI" varchar(1) NOT NULL default ' ',
  "AAABFTTQ" varchar(4) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABFTPU" varchar(1) NOT NULL default ' ',
  "AAABFTTR" varchar(1) NOT NULL default ' ',
  "AAABGOBC" varchar(1) NOT NULL default ' ',
  "AAABFTTV" varchar(1) NOT NULL default ' ',
  "AAABJDAN" varchar(4) NOT NULL default ' ',
  "AAABFTPZ" varchar(1) NOT NULL default ' ',
  "AAABFTQZ" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(1) NOT NULL default ' ',
  "AAABLENA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTUI","AAABFTTQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTUI" varchar(1) NOT NULL default ' ',
  "AAABFTTV" varchar(1) NOT NULL default ' ',
  "AAABFTQO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTUI","AAABFTTV","AAABFTQO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTUI" varchar(1) NOT NULL default ' ',
  "AAABFTUE" varchar(40) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  "AAABFTQU" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(1) NOT NULL default ' ',
  "AAABLENA" varchar(1) NOT NULL default ' ',
  "AAABGOBC" varchar(1) NOT NULL default ' ',
  "AAABFTTV" varchar(1) NOT NULL default ' ',
  "AAABFTQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTUI","AAABFTUE","AAABMMVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTWK" varchar(3) NOT NULL default ' ',
  "AAABFTSA" varchar(2) NOT NULL default ' ',
  "AAABFTRI" int(11) NOT NULL default '0',
  "AAABFTRJ" int(11) NOT NULL default '0',
  "AAABFTRK" decimal(13,2) NOT NULL default '0.00',
  "AAABFTRL" decimal(13,2) NOT NULL default '0.00',
  "AAABLPQY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTRP" (
  "AAABFTRT" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFTSA" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTRT","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTRV" (
  "AAABFTSA" varchar(2) NOT NULL default ' ',
  "AAABLLJG" varchar(1) NOT NULL default ' ',
  "AAABFTRY" varchar(1) NOT NULL default ' ',
  "AAABFTRZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFTSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLSTE" varchar(4) NOT NULL default ' ',
  "AAABFTSL" varchar(4) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABMMCV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOIA","AAABLSTE","AAABFTSL","AAABMMCU","AAABMMCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTST" varchar(30) NOT NULL default ' ',
  "AAABFTSR" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGYTS" varchar(4) NOT NULL default ' ',
  "AAABFTTF" varchar(4) NOT NULL default ' ',
  "AAABKRHZ" varchar(1) NOT NULL default ' ',
  "AAABFTUH" varchar(12) NOT NULL default ' ',
  "AAABFTTB" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABGYTS","AAABFTTF","AAABKRHZ","AAABFTUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTUH" varchar(12) NOT NULL default ' ',
  "AAABFTUI" varchar(1) NOT NULL default ' ',
  "AAABFTTV" varchar(1) NOT NULL default ' ',
  "AAABLENA" varchar(1) NOT NULL default ' ',
  "AAABIJSB" varchar(1) NOT NULL default ' ',
  "AAABFTUJ" varchar(3) NOT NULL default '000',
  "AAABFTTQ" varchar(4) NOT NULL default ' ',
  "AAABFTTR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTUH","AAABFTUI","AAABFTTV","AAABLENA","AAABIJSB","AAABFTUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTUH" varchar(12) NOT NULL default ' ',
  "AAABFTUI" varchar(1) NOT NULL default ' ',
  "AAABFTUJ" varchar(3) NOT NULL default '000',
  "AAABFTUE" varchar(40) NOT NULL default ' ',
  "AAABMMVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTUH","AAABFTUI","AAABFTUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFTVD" varchar(2) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFTVD","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFTVD" varchar(2) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABFTUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABFTVD","AAABMCXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTWA" varchar(6) NOT NULL default ' ',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABFTVK" varchar(4) NOT NULL default ' ',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMEKQ" varchar(6) NOT NULL default ' ',
  "AAABHDLK" varchar(6) NOT NULL default ' ',
  "AAABFTVQ" varchar(2) NOT NULL default ' ',
  "AAABLXUA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTWA" varchar(6) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFTVY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTWA","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(8) NOT NULL default ' ',
  "AAABFTWG" varchar(30) NOT NULL default ' ',
  "AAABMLFD" varchar(12) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABFTWK" varchar(3) NOT NULL default ' ',
  "AAABJDMI" varchar(1) NOT NULL default ' ',
  "AAABFTWM" varchar(40) NOT NULL default ' ',
  "AAABFTWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTYR","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(8) NOT NULL default ' ',
  "AAABFTXC" varchar(30) NOT NULL default ' ',
  "AAABFTWW" varchar(1) NOT NULL default ' ',
  "AAABFTWX" varchar(1) NOT NULL default ' ',
  "AAABFTWY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTYR","AAABMLXR","AAABFTXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(8) NOT NULL default ' ',
  "AAABFTXI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFTYR","AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABMLXR" varchar(8) NOT NULL default ' ',
  "AAABFUAB" varchar(4) NOT NULL default ' ',
  "AAABFTXS" varchar(14) NOT NULL default ' ',
  "AAABFTXT" varchar(7) NOT NULL default ' ',
  "AAABFTXU" varchar(7) NOT NULL default ' ',
  "AAABFTXV" varchar(7) NOT NULL default ' ',
  "AAABFTXW" varchar(1) NOT NULL default ' ',
  "AAABJJLP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFTYR","AAABMLXR","AAABFUAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABLTXG" varchar(2) NOT NULL default ' ',
  "AAABIVOD" varchar(30) NOT NULL default ' ',
  "AAABFTYH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFTYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFTYR" varchar(2) NOT NULL default ' ',
  "AAABFTYO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFTYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUAB" varchar(4) NOT NULL default ' ',
  "AAABFTYV" varchar(30) NOT NULL default ' ',
  "AAABFTYW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFUAB" varchar(4) NOT NULL default ' ',
  "AAABFTZD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFUAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABFTZK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUAB" varchar(4) NOT NULL default ' ',
  "AAABIKFN" varchar(30) NOT NULL default ' ',
  "AAABFTZR" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUAB","AAABIKFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFTZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUAB" varchar(4) NOT NULL default ' ',
  "AAABFUAC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUAB","AAABFUAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUAM" varchar(8) NOT NULL default ' ',
  "AAABKMJV" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABHAGF" varchar(2) NOT NULL default '00',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFUAM","AAABKMJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUAO" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABJDAN" varchar(4) NOT NULL default ' ',
  "AAABKRFP" varchar(2) NOT NULL default '00',
  "AAABFUAS" varchar(2) NOT NULL default '00',
  "AAABFUAT" varchar(2) NOT NULL default '00',
  "AAABFUAU" varchar(1) NOT NULL default ' ',
  "AAABKOTS" varchar(1) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  "AAABKOTQ" varchar(4) NOT NULL default ' ',
  "AAABKOTF" varchar(1) NOT NULL default ' ',
  "AAABKOTG" varchar(1) NOT NULL default ' ',
  "AAABKOTH" varchar(1) NOT NULL default ' ',
  "AAABKOTI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABJDAN","AAABKRFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUBF" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJDAN" varchar(4) NOT NULL default ' ',
  "AAABKRFP" varchar(2) NOT NULL default '00',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABJDAN","AAABKRFP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUBP" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default '00',
  "AAABFUBT" varchar(1) NOT NULL default ' ',
  "AAABFUBU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMCRJ","AAABKOTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUBY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default '00',
  "AAABFUCD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABMCRJ","AAABKOTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUCI" (
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABFUCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUCL" (
  "AAABFUCP" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABFUCP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUCR" (
  "AAABFUDE" varchar(2) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFUDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUDE" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABKRHS" varchar(2) NOT NULL default ' ',
  "AAABMQCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUDE","AAABMCRJ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABFVBF" varchar(3) NOT NULL default '000',
  "AAABFVBG" varchar(8) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLXZC" decimal(5,0) NOT NULL default '0',
  "AAABLXZD" decimal(5,0) NOT NULL default '0',
  "AAABLWYK" varchar(1) NOT NULL default ' ',
  "AAABLWYL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABILWI","AAABMAMY","AAABMQIJ","AAABLVBH","AAABFVBF","AAABFVBG","AAABLYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHODN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHODN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABHPUE" varchar(1) NOT NULL default ' ',
  "AAABHODN" varchar(2) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW","AAABHPUE","AAABHODN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHODN" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHODN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKGBL" varchar(15) NOT NULL default ' ',
  "AAABFUGV" varchar(1) NOT NULL default ' ',
  "AAABFUGW" varchar(1) NOT NULL default ' ',
  "AAABFUGX" varchar(1) NOT NULL default ' ',
  "AAABFUGY" varchar(1) NOT NULL default ' ',
  "AAABFUFZ" varchar(1) NOT NULL default ' ',
  "AAABFUGA" varchar(1) NOT NULL default ' ',
  "AAABFUGB" varchar(1) NOT NULL default '0',
  "AAABFUGC" varchar(1) NOT NULL default '0',
  "AAABFUGD" varchar(4) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABGGGH" varchar(12) NOT NULL default ' ',
  "AAABLWWN" varchar(8) NOT NULL default '00000000',
  "AAABFUGI" varchar(12) NOT NULL default ' ',
  "AAABFUGJ" varchar(8) NOT NULL default '00000000',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABJANL" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABKRXE" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKGBL" varchar(12) NOT NULL default ' ',
  "AAABFUGV" varchar(1) NOT NULL default ' ',
  "AAABFUGW" varchar(1) NOT NULL default ' ',
  "AAABFUGX" varchar(1) NOT NULL default ' ',
  "AAABFUGY" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABFUHA" varchar(3) NOT NULL default '000',
  "AAABFUHB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABJANL","AAABFUJO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABJANL" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABJANL","AAABFUJO","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABKRXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABJANL" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABKRXF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABJANL","AAABFUJO","AAABKRXF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFUJH" varchar(1) NOT NULL default ' ',
  "AAABFUJI" varchar(1) NOT NULL default ' ',
  "AAABFUJJ" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABJANL" varchar(8) NOT NULL default '00000000',
  "AAABFUJO" varchar(6) NOT NULL default '000000',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFUJH" varchar(1) NOT NULL default ' ',
  "AAABFUJI" varchar(1) NOT NULL default ' ',
  "AAABFUJJ" varchar(1) NOT NULL default ' ',
  "AAABGHKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABJANL","AAABFUJO","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABFUJZ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABFUJV" varchar(3) NOT NULL default ' ',
  "AAABFUJW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUKI","AAABFUJZ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFUKI" varchar(8) NOT NULL default ' ',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUKJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCRJ","AAABFUKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFULA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFULA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFULB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKRXK" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMCRJ","AAABKRXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFULL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFULS" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFULS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFULT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFUMB" varchar(2) NOT NULL default ' ',
  "AAABFUMC" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFUMB","AAABFUMC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFUMK" varchar(8) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFUMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUML" (
  "AAABGYQM" varchar(30) NOT NULL default ' ',
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFUMO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGYQM","AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUNM" varchar(1) NOT NULL default ' ',
  "AAABFUNN" varchar(1) NOT NULL default ' ',
  "AAABFUNO" varchar(1) NOT NULL default ' ',
  "AAABFUNP" varchar(1) NOT NULL default ' ',
  "AAABFUNQ" varchar(1) NOT NULL default ' ',
  "AAABFUNR" varchar(1) NOT NULL default ' ',
  "AAABFUNS" varchar(1) NOT NULL default ' ',
  "AAABFUNT" varchar(1) NOT NULL default ' ',
  "AAABFUNU" varchar(1) NOT NULL default ' ',
  "AAABFUNV" varchar(1) NOT NULL default ' ',
  "AAABFUNW" varchar(1) NOT NULL default ' ',
  "AAABFUNX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUNM" varchar(1) NOT NULL default ' ',
  "AAABFUNN" varchar(1) NOT NULL default ' ',
  "AAABFUNO" varchar(1) NOT NULL default ' ',
  "AAABFUNP" varchar(1) NOT NULL default ' ',
  "AAABFUNQ" varchar(1) NOT NULL default ' ',
  "AAABFUNR" varchar(1) NOT NULL default ' ',
  "AAABFUNS" varchar(1) NOT NULL default ' ',
  "AAABFUNT" varchar(1) NOT NULL default ' ',
  "AAABFUNU" varchar(1) NOT NULL default ' ',
  "AAABFUNV" varchar(1) NOT NULL default ' ',
  "AAABFUNW" varchar(1) NOT NULL default ' ',
  "AAABFUNX" varchar(1) NOT NULL default ' ',
  "AAABFUNY" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABFUQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUQN" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFUQD" varchar(60) NOT NULL default ' ',
  "AAABFUOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUQM","AAABFUQN","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUQN" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFUQD" varchar(60) NOT NULL default ' ',
  "AAABFUOX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABFUQM","AAABFUQN","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUQN" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFUQD" varchar(60) NOT NULL default ' ',
  "AAABFUQF" varchar(1) NOT NULL default ' ',
  "AAABFUQG" varchar(1) NOT NULL default ' ',
  "AAABFUQH" varchar(1) NOT NULL default '0',
  "AAABFUQI" varchar(1) NOT NULL default ' ',
  "AAABFUQJ" varchar(1) NOT NULL default ' ',
  "AAABFUQE" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFUQM","AAABFUQN","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFUQM" varchar(1) NOT NULL default ' ',
  "AAABFUQN" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFUQD" varchar(60) NOT NULL default ' ',
  "AAABFUQE" int(11) NOT NULL default '0',
  "AAABFUQF" varchar(1) NOT NULL default ' ',
  "AAABFUQG" varchar(1) NOT NULL default ' ',
  "AAABFUQH" varchar(1) NOT NULL default '0',
  "AAABFUQI" varchar(1) NOT NULL default ' ',
  "AAABFUQJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABFUQM","AAABFUQN","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQNF" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMQNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFURD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFURJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABMJXV" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(2) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABMJXV","AAABMJXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFURT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABKOUS" varchar(6) NOT NULL default ' ',
  "AAABFURX" varchar(1) NOT NULL default ' ',
  "AAABFURY" varchar(1) NOT NULL default ' ',
  "AAABFURZ" varchar(1) NOT NULL default ' ',
  "AAABFUSA" varchar(1) NOT NULL default ' ',
  "AAABFUSB" varchar(1) NOT NULL default ' ',
  "AAABFUSC" varchar(1) NOT NULL default ' ',
  "AAABFUSD" varchar(1) NOT NULL default ' ',
  "AAABFUSE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUVG","AAABKOUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUVI" varchar(3) NOT NULL default ' ',
  "AAABFUUI" varchar(2) NOT NULL default ' ',
  "AAABFUUJ" varchar(3) NOT NULL default ' ',
  "AAABFUUD" varchar(3) NOT NULL default ' ',
  "AAABFUTP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUVG","AAABMCRJ","AAABFUVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABFUTP" varchar(3) NOT NULL default ' ',
  "AAABFUSZ" varchar(2) NOT NULL default '00',
  "AAABFUTA" varchar(2) NOT NULL default '00',
  "AAABFUTB" varchar(2) NOT NULL default '00',
  "AAABFUTC" varchar(2) NOT NULL default '00',
  "AAABFUTD" varchar(2) NOT NULL default '00',
  "AAABFUTE" varchar(2) NOT NULL default '00',
  "AAABFUTF" varchar(2) NOT NULL default '00',
  "AAABFUTG" varchar(2) NOT NULL default '00',
  "AAABFUTH" varchar(2) NOT NULL default '00',
  "AAABFUTI" varchar(2) NOT NULL default '00',
  "AAABFUTJ" varchar(2) NOT NULL default '00',
  "AAABFUTK" varchar(2) NOT NULL default '00',
  "AAABFUTL" varchar(2) NOT NULL default '00',
  "AAABFUTM" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFUVG","AAABFUTP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUUD" varchar(3) NOT NULL default ' ',
  "AAABFUUQ" varchar(2) NOT NULL default ' ',
  "AAABFUUR" varchar(3) NOT NULL default ' ',
  "AAABFUUS" varchar(1) NOT NULL default ' ',
  "AAABFUUT" varchar(4) NOT NULL default ' ',
  "AAABFUTZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUVG","AAABMCRJ","AAABFUUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUVI" varchar(3) NOT NULL default ' ',
  "AAABFUUI" varchar(2) NOT NULL default ' ',
  "AAABFUUJ" varchar(3) NOT NULL default ' ',
  "AAABFUUK" varchar(3) NOT NULL default ' ',
  "AAABFUUL" varchar(3) NOT NULL default ' ',
  "AAABFUUM" varchar(3) NOT NULL default ' ',
  "AAABFUUN" varchar(1) NOT NULL default ' ',
  "AAABFUUO" varchar(4) NOT NULL default ' ',
  "AAABFUUP" varchar(2) NOT NULL default '00',
  "AAABFUUQ" varchar(2) NOT NULL default ' ',
  "AAABFUUR" varchar(3) NOT NULL default ' ',
  "AAABFUUS" varchar(1) NOT NULL default ' ',
  "AAABFUUT" varchar(4) NOT NULL default ' ',
  "AAABFUUU" varchar(2) NOT NULL default '00',
  "AAABFUVS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABFUVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVG" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABFUVI" varchar(3) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUVG","AAABMCRJ","AAABFUVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUVS" varchar(3) NOT NULL default ' ',
  "AAABFUVM" varchar(1) NOT NULL default '0',
  "AAABFUVN" varchar(1) NOT NULL default '0',
  "AAABFUVO" varchar(1) NOT NULL default '0',
  "AAABFUVP" varchar(4) NOT NULL default ' ',
  "AAABFUVQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFUVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABFUVW" varchar(2) NOT NULL default '00',
  "AAABFUVX" varchar(1) NOT NULL default ' ',
  "AAABFUVY" varchar(1) NOT NULL default ' ',
  "AAABFUVZ" varchar(1) NOT NULL default ' ',
  "AAABFUWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFUWK" varchar(3) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFUWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUWQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKRGG" varchar(6) NOT NULL default ' ',
  "AAABHANX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKRGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUWY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMDZH" varchar(4) NOT NULL default ' ',
  "AAABFUYD" varchar(3) NOT NULL default '000',
  "AAABFUXN" varchar(10) NOT NULL default ' ',
  "AAABFUXF" varchar(10) NOT NULL default ' ',
  "AAABGIMJ" varchar(15) NOT NULL default ' ',
  "AAABFUZI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMCRJ","AAABMAMY","AAABMDZH","AAABFUYD","AAABFUXN"),
  KEY "AAABFUXO" ("AAABLUXQ","AAABGIMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUXR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMDZH" varchar(4) NOT NULL default ' ',
  "AAABFUYD" varchar(3) NOT NULL default '000',
  "AAABFUYE" varchar(4) NOT NULL default ' ',
  "AAABFUXY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMCRJ","AAABMAMY","AAABMDZH","AAABFUYD","AAABFUYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFVCA" varchar(3) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABFVBU" varchar(5) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMAMY","AAABFVCA","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUYZ" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMDZH" varchar(4) NOT NULL default ' ',
  "AAABFUZN" varchar(10) NOT NULL default ' ',
  "AAABFUZO" varchar(6) NOT NULL default ' ',
  "AAABFUZG" varchar(10) NOT NULL default ' ',
  "AAABFUZH" varchar(6) NOT NULL default ' ',
  "AAABFUZI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMCRJ","AAABMAMY","AAABMDZH","AAABFUZN","AAABFUZO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFUZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABKOTY","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABLVBH" varchar(3) NOT NULL default '000',
  "AAABFVBF" varchar(3) NOT NULL default '000',
  "AAABFVBG" varchar(8) NOT NULL default ' ',
  "AAABLYAS" varchar(10) NOT NULL default ' ',
  "AAABLXYU" varchar(100) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABLXYV" varchar(2) NOT NULL default '00',
  "AAABMAMX" varchar(1) NOT NULL default ' ',
  "AAABMLEQ" varchar(12) NOT NULL default ' ',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABLXZI" varchar(1) NOT NULL default ' ',
  "AAABLXZF" varchar(1) NOT NULL default ' ',
  "AAABLXYY" decimal(11,2) NOT NULL default '0.00',
  "AAABLXYZ" varchar(5) NOT NULL default ' ',
  "AAABLXZA" decimal(5,0) NOT NULL default '0',
  "AAABLXZB" varchar(3) NOT NULL default ' ',
  "AAABLXZC" decimal(5,0) NOT NULL default '0',
  "AAABLXZD" decimal(5,0) NOT NULL default '0',
  "AAABLWYK" varchar(1) NOT NULL default ' ',
  "AAABLWYL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABILWI","AAABMAMY","AAABMQIJ","AAABLVBH","AAABFVBF","AAABFVBG","AAABLYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABFVCA" varchar(3) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXV" varchar(4) NOT NULL default ' ',
  "AAABFVBU" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABMCRJ","AAABMREX","AAABMCPV","AAABFVCA","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCJZ" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMREX","AAABMCPV","AAABKOTY","AAABMCJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJVUZ" varchar(3) NOT NULL default ' ',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJVUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABFVDE" varchar(30) NOT NULL default ' ',
  "AAABFVDB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABFVDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVDF" (
  "AAABLUEU" varchar(20) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABFVDJ" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(30) NOT NULL default ' ',
  "AAABFVDO" varchar(1) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  "AAABFVDQ" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABFVDS" varchar(14) NOT NULL default ' ',
  "AAABFVDT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVDV" (
  "AAABLUEU" varchar(20) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMQXN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVEB" (
  "AAABLEND" varchar(10) NOT NULL default ' ',
  "AAABFVFJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBDG" varchar(4) NOT NULL default '0000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAABLTLR" varchar(3) NOT NULL default '000',
  "AAABHVRO" varchar(1) NOT NULL default ' ',
  "AAABFVER" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABFVET" varchar(1) NOT NULL default ' ',
  "AAABFVEU" varchar(1) NOT NULL default ' ',
  "AAABFVHW" varchar(1) NOT NULL default ' ',
  "AAABFVEW" varchar(1) NOT NULL default ' ',
  "AAABFVFZ" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABFVEZ" varchar(60) NOT NULL default ' ',
  "AAABFVFA" varchar(1) NOT NULL default '0',
  "AAABFVFX" varchar(40) NOT NULL default ' ',
  "AAABFVFC" varchar(1) NOT NULL default '0',
  "AAABFVFD" varchar(20) NOT NULL default ' ',
  "AAABFVFE" varchar(20) NOT NULL default ' ',
  "AAABFVFF" varchar(60) NOT NULL default ' ',
  "AAABFVFG" varchar(15) NOT NULL default ' ',
  "AAABFVFH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEND","AAABFVFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVFK" (
  "AAABLEND" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBDG" varchar(4) NOT NULL default '0000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAABFVFX" varchar(20) NOT NULL default ' ',
  "AAABMEGE" varchar(1) NOT NULL default ' ',
  "AAABFVFZ" varchar(1) NOT NULL default ' ',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLQHQ" varchar(60) NOT NULL default ' ',
  "AAABFVGC" varchar(40) NOT NULL default ' ',
  "AAABGIWJ" varchar(1) NOT NULL default ' ',
  "AAABFVGE" varchar(1) NOT NULL default ' ',
  "AAABFVGF" varchar(1) NOT NULL default '0',
  "AAABFVGG" varchar(1) NOT NULL default ' ',
  "AAABFVGH" varchar(1) NOT NULL default ' ',
  "AAABMKFU" varchar(20) NOT NULL default ' ',
  "AAABFVHW" varchar(1) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABFVGL" varchar(1) NOT NULL default ' ',
  "AAABFVGM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVGO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(22) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVGX" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABGEIN" varchar(1) NOT NULL default ' ',
  "AAABFVHD" varchar(1) NOT NULL default ' ',
  "AAABKNCK" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVHH" (
  "AAABLEND" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABLTJI" varchar(8) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABGBDG" varchar(4) NOT NULL default '0000',
  "AAABLRFC" varchar(22) NOT NULL default ' ',
  "AAABMLPV" varchar(10) NOT NULL default ' ',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABFVHW" varchar(1) NOT NULL default ' ',
  "AAABFVHX" varchar(1) NOT NULL default ' ',
  "AAABFVHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLEND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVIA" (
  "AAABLEND" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABHIJR" varchar(1) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABFVIF" varchar(8) NOT NULL default '00000000',
  "AAABIKCI" varchar(8) NOT NULL default '00000000',
  "AAABFVIH" varchar(8) NOT NULL default '00000000',
  "AAABMPMT" varchar(8) NOT NULL default '00000000',
  "AAABFVIJ" varchar(8) NOT NULL default '00000000',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABFVIM" varchar(1) NOT NULL default ' ',
  "AAABHEYE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABLEND","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVIQ" (
  "AAABLTOL" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJYKA" varchar(40) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTOL","AAABMRHS","AAABJYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVIY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABLTOL" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABLTOL","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVJG" (
  "AAABLFYD" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(5) NOT NULL default '00000',
  "AAABFVJJ" varchar(3) NOT NULL default ' ',
  "AAABFVJK" varchar(1) NOT NULL default ' ',
  "AAABFVJL" varchar(2) NOT NULL default '00',
  "AAABFVJM" varchar(2) NOT NULL default '00',
  "AAABFVJN" varchar(1) NOT NULL default '0',
  "AAABFVJO" decimal(3,0) NOT NULL default '0',
  "AAABFVJP" varchar(2) NOT NULL default ' ',
  "AAABFVJQ" varchar(1) NOT NULL default ' ',
  "AAABFVJR" varchar(1) NOT NULL default ' ',
  "AAABFVJS" varchar(3) NOT NULL default ' ',
  "AAABFVJT" varchar(3) NOT NULL default ' ',
  "AAABFVJU" varchar(1) NOT NULL default ' ',
  "AAABFVJV" varchar(30) NOT NULL default ' ',
  "AAABFVJW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYD","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLFK" varchar(3) NOT NULL default ' ',
  "AAABKRSA" varchar(3) NOT NULL default ' ',
  "AAABFVKL" varchar(2) NOT NULL default ' ',
  "AAABFZVT" varchar(1) NOT NULL default ' ',
  "AAABFWEU" varchar(4) NOT NULL default ' ',
  "AAABFVKG" decimal(13,3) NOT NULL default '0.000',
  "AAABFVKH" decimal(13,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABMLFK","AAABKRSA","AAABFVKL","AAABFZVT","AAABFWEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVKO" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABHWXN" varchar(16) NOT NULL default ' ',
  "AAABFVLA" varchar(30) NOT NULL default ' ',
  "AAABFVLB" varchar(30) NOT NULL default ' ',
  "AAABMLPO" smallint(6) NOT NULL default '0',
  "AAABMMJT" longtext NOT NULL,
  "AAABFVKW" longblob NOT NULL,
  PRIMARY KEY ("AAABMQDU","AAABMCRV","AAABHWXN","AAABFVLA","AAABFVLB","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVLD" (
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABHWXN" varchar(16) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDZ" varchar(50) NOT NULL default ' ',
  "AAABMLEA" varchar(50) NOT NULL default ' ',
  "AAABMLEB" varchar(50) NOT NULL default ' ',
  "AAABMLEC" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMQDU","AAABMCRV","AAABHWXN","AAABMLDY","AAABMLDW","AAABMLDX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVLU" (
  "AAABFVMJ" varchar(2) NOT NULL default ' ',
  "AAABFWOA" varchar(1) NOT NULL default ' ',
  "AAABFVMB" varchar(1) NOT NULL default ' ',
  "AAABFVLY" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABFVMJ","AAABFWOA","AAABFVMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVMC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFVMJ" varchar(2) NOT NULL default ' ',
  "AAABFWOA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  "AAABFVMH" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFVMJ","AAABFWOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVMV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFVNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVND" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVNI" varchar(4) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVNI","AAABMQHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFVXO" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFVXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXH" varchar(8) NOT NULL default ' ',
  "AAABFVXI" varchar(8) NOT NULL default ' ',
  "AAABFVOB" varchar(2) NOT NULL default '00',
  "AAABFVNX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXH","AAABFVXI","AAABFVOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVOO" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABHZJH" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABHZJL" varchar(10) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABHZJH","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXH" varchar(8) NOT NULL default ' ',
  "AAABFVPB" varchar(40) NOT NULL default ' ',
  "AAABFVPC" varchar(32) NOT NULL default ' ',
  "AAABFVPD" varchar(2) NOT NULL default '00',
  "AAABFVPE" varchar(4) NOT NULL default '0000',
  "AAABFVPF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABFVXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVPI" (
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
CREATE TABLE "AAABFVQD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLLLR" varchar(32) NOT NULL default ' ',
  "AAABLLLO" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABLLLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB"),
  KEY "AAABFVQY" ("AAABLLMD","AAABLLME","AAABLLMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVRC" (
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
CREATE TABLE "AAABFVRR" (
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
CREATE TABLE "AAABFVSG" (
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
CREATE TABLE "AAABFVSU" (
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
CREATE TABLE "AAABFVTG" (
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
CREATE TABLE "AAABFVUI" (
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
CREATE TABLE "AAABFVVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMBXY" varchar(32) NOT NULL default ' ',
  "AAABLLMD" varchar(25) NOT NULL default ' ',
  "AAABLLMB" varchar(5) NOT NULL default '00000',
  "AAABLLMF" varchar(10) NOT NULL default ' ',
  "AAABLLLX" varchar(64) NOT NULL default ' ',
  "AAABLLME" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMBXY","AAABLLMD","AAABLLMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVVU" (
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
CREATE TABLE "AAABFVWJ" (
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
CREATE TABLE "AAABFVWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXH" varchar(8) NOT NULL default ' ',
  "AAABFVXI" varchar(8) NOT NULL default ' ',
  "AAABFVXC" varchar(3) NOT NULL default ' ',
  "AAABFVXD" tinyint(3) unsigned NOT NULL default '0',
  "AAABFVXE" varchar(1) NOT NULL default ' ',
  "AAABFVXF" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABFVXH","AAABFVXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFVXO" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFVXO","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHQAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHQAS" varchar(4) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHQAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABLURS" varchar(4) NOT NULL default ' ',
  "AAABFVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJGC","AAABLURS","AAABFVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMALG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJGC" varchar(1) NOT NULL default ' ',
  "AAABHXNO" varchar(4) NOT NULL default ' ',
  "AAABFVZI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJGC","AAABHXNO","AAABFVZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVZJ" (
  "AAABKGBL" varchar(26) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVZN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKGBL" varchar(26) NOT NULL default ' ',
  "AAABFVZQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABKGBL"),
  KEY "AAABFVZT" ("AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFVZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWAC" varchar(20) NOT NULL default ' ',
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABFVZZ" varchar(10) NOT NULL default ' ',
  "AAABFWAA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABFWAC","AAABMPLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABLTQE" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABFWCS" varchar(120) NOT NULL default ' ',
  "AAABFWCT" int(11) NOT NULL default '0',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABHFCC" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABFWCQ" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABFWCS","AAABFWCT"),
  KEY "AAABFWAP" ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWAR" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABFWCS" varchar(120) NOT NULL default ' ',
  "AAABFWCQ" varchar(120) NOT NULL default ' ',
  "AAABLHPU" varchar(40) NOT NULL default ' ',
  "AAABFWAW" varchar(255) NOT NULL default ' ',
  "AAABKAHC" varchar(40) NOT NULL default ' ',
  "AAABJCXQ" varchar(20) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMKFH","AAABFWCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWBD" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABFWBS" varchar(4) NOT NULL default ' ',
  "AAABFWCS" varchar(120) NOT NULL default ' ',
  "AAABFWCQ" varchar(120) NOT NULL default ' ',
  "AAABIGWP" varchar(20) NOT NULL default ' ',
  "AAABKVKI" varchar(8) NOT NULL default '00000000',
  "AAABGHGT" varchar(6) NOT NULL default '000000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABFWBS","AAABFWCS"),
  KEY "AAABFWBR" ("AAABFWBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWBT" (
  "AAABJEXI" varchar(20) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFWCS" varchar(120) NOT NULL default ' ',
  "AAABFWCQ" varchar(120) NOT NULL default ' ',
  "AAABIGWP" varchar(20) NOT NULL default ' ',
  "AAABKVKI" varchar(8) NOT NULL default '00000000',
  "AAABGHGT" varchar(6) NOT NULL default '000000',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABLGCM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABJEXI","AAABMREL","AAABFWCS"),
  KEY "AAABFWCH" ("AAABFWCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWCJ" (
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABFWCS" varchar(120) NOT NULL default ' ',
  "AAABFWCT" int(11) NOT NULL default '0',
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABHFCC" varchar(1) NOT NULL default ' ',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABFWCQ" varchar(120) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH","AAABFWCS","AAABFWCT"),
  KEY "AAABFWCU" ("AAABILWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWEU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHFOL" (
  "AAABFWEL" varchar(4) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABKNBP" varchar(1) NOT NULL default ' ',
  "AAABFWDG" varchar(4) NOT NULL default ' ',
  "AAABFWDH" varchar(4) NOT NULL default ' ',
  "AAABFWDI" varchar(2) NOT NULL default '00',
  "AAABFWDJ" varchar(1) NOT NULL default ' ',
  "AAABFWDK" varchar(1) NOT NULL default ' ',
  "AAABFWDL" varchar(1) NOT NULL default ' ',
  "AAABFWDM" varchar(1) NOT NULL default ' ',
  "AAABFWDN" varchar(1) NOT NULL default ' ',
  "AAABJEDH" varchar(1) NOT NULL default ' ',
  "AAABFWDP" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABFWEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWDR" (
  "AAABFWEL" varchar(4) NOT NULL default ' ',
  "AAABFWEG" varchar(2) NOT NULL default '00',
  "AAABFWDU" varchar(4) NOT NULL default ' ',
  "AAABLQRP" varchar(4) NOT NULL default ' ',
  "AAABHRND" varchar(64) NOT NULL default ' ',
  "AAABFWDX" varchar(100) NOT NULL default ' ',
  "AAABFWDY" varchar(1) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABFWEA" varchar(1) NOT NULL default ' ',
  "AAABFWEB" smallint(6) NOT NULL default '0',
  "AAABFWEC" smallint(6) NOT NULL default '0',
  "AAABKMUF" smallint(6) NOT NULL default '0',
  "AAABFWEE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFWEL","AAABFWEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWEH" (
  "AAABFWEL" varchar(4) NOT NULL default ' ',
  "AAABLFZK" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABFWEL","AAABLFZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFWEU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFWEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWEV" (
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(4) NOT NULL default '0000',
  "AAABLZWZ" varchar(8) NOT NULL default '00000000',
  "AAABLZXA" varchar(6) NOT NULL default '000000',
  "AAABLZXB" decimal(14,4) NOT NULL default '0.0000',
  "AAABFWFC" varchar(3) NOT NULL default ' ',
  "AAABLZXC" int(11) NOT NULL default '0',
  "AAABLZXD" decimal(15,2) NOT NULL default '0.00',
  "AAABLZXE" varchar(5) NOT NULL default ' ',
  "AAABLZXF" varchar(1) NOT NULL default ' ',
  "AAABLZXG" varchar(26) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMKX","AAABLTYE","AAABLTYF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTYE" varchar(30) NOT NULL default ' ',
  "AAABLTYF" varchar(4) NOT NULL default '0000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLTYE","AAABLTYF","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABFWGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABFWGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABFWGL" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH","AAABFWGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWGO" varchar(1) NOT NULL default ' ',
  "AAABFWGP" varchar(1) NOT NULL default ' ',
  "AAABFWGQ" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWHR" varchar(10) NOT NULL default ' ',
  "AAABFWHS" varchar(12) NOT NULL default ' ',
  "AAABFWGX" varchar(6) NOT NULL default ' ',
  "AAABFWGY" varchar(1) NOT NULL default ' ',
  "AAABFWGZ" varchar(12) NOT NULL default ' ',
  "AAABFWHA" varchar(1) NOT NULL default ' ',
  "AAABFWHB" varchar(4) NOT NULL default ' ',
  "AAABFWHC" varchar(12) NOT NULL default ' ',
  "AAABFWHD" varchar(2) NOT NULL default ' ',
  "AAABFWHE" varchar(4) NOT NULL default ' ',
  "AAABFWHF" varchar(1) NOT NULL default '0',
  "AAABFWHG" varchar(1) NOT NULL default ' ',
  "AAABFWHH" varchar(1) NOT NULL default ' ',
  "AAABFWHI" varchar(1) NOT NULL default ' ',
  "AAABFWHJ" varchar(1) NOT NULL default '0',
  "AAABFWHK" varchar(25) NOT NULL default ' ',
  "AAABFWHL" varchar(25) NOT NULL default ' ',
  "AAABKZAW" varchar(65) NOT NULL default ' ',
  "AAABFWHN" varchar(5) NOT NULL default ' ',
  "AAABFWHO" varchar(16) NOT NULL default ' ',
  "AAABFWHP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWHR","AAABFWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWHT" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABGEIN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWHZ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWIC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWIF" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWII" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWIL" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWIO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWIR" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABLTQJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWIX" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWJA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWJD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABLTUD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWJJ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWJS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWJP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWJS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWJV" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLUSV" varchar(2) NOT NULL default '00',
  "AAABFWJY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWKB" (
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABFWKD" varchar(8) NOT NULL default '00000000',
  "AAABFWKE" varchar(55) NOT NULL default ' ',
  "AAABFWKF" varchar(55) NOT NULL default ' ',
  "AAABFWKG" varchar(1) NOT NULL default ' ',
  "AAABFWLV" varchar(1) NOT NULL default ' ',
  "AAABFWKI" varchar(2) NOT NULL default '00',
  "AAABFWKJ" varchar(1) NOT NULL default ' ',
  "AAABFWKK" varchar(1) NOT NULL default ' ',
  "AAABFWKL" varchar(1) NOT NULL default ' ',
  "AAABFWKM" varchar(43) NOT NULL default ' ',
  "AAABFWKN" varchar(63) NOT NULL default ' ',
  PRIMARY KEY ("AAABIISW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWKP" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABFWLB" varchar(4) NOT NULL default ' ',
  "AAABFWLC" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABFWKV" varchar(60) NOT NULL default ' ',
  "AAABFWKW" varchar(1) NOT NULL default ' ',
  "AAABFWKX" varchar(1) NOT NULL default ' ',
  "AAABFWKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABIISW","AAABFWLB","AAABFWLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWLD" (
  "AAABIISW" varchar(4) NOT NULL default ' ',
  "AAABFWMK" varchar(10) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMNJF" varchar(10) NOT NULL default ' ',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLEJH" varchar(12) NOT NULL default ' ',
  "AAABFWLQ" varchar(1) NOT NULL default ' ',
  "AAABFWLR" varchar(1) NOT NULL default ' ',
  "AAABFWLS" varchar(1) NOT NULL default ' ',
  "AAABFWLT" varchar(1) NOT NULL default ' ',
  "AAABFWLU" varchar(55) NOT NULL default ' ',
  "AAABFWLV" varchar(1) NOT NULL default ' ',
  "AAABFWLW" int(11) NOT NULL default '0',
  "AAABFWLX" int(11) NOT NULL default '0',
  "AAABFWLY" int(11) NOT NULL default '0',
  "AAABFWLZ" varchar(1) NOT NULL default ' ',
  "AAABFWMA" varchar(1) NOT NULL default ' ',
  "AAABFWMB" varchar(4) NOT NULL default ' ',
  "AAABFWMC" varchar(4) NOT NULL default ' ',
  "AAABFWMD" varchar(4) NOT NULL default ' ',
  "AAABLMNC" varchar(30) NOT NULL default ' ',
  "AAABKNBT" varchar(8) NOT NULL default ' ',
  "AAABFWMG" varchar(4) NOT NULL default '0000',
  "AAABFWMH" varchar(8) NOT NULL default '00000000',
  "AAABGUFZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIISW","AAABFWMK","AAABLSCE"),
  KEY "AAABFWMM" ("AAABLSCE","AAABMNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLEER" varchar(8) NOT NULL default '00000000',
  "AAABINIK" varchar(6) NOT NULL default '000000',
  "AAABGWWO" varchar(1) NOT NULL default ' ',
  "AAABJTHJ" varchar(1) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLEER","AAABINIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWNB" (
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAABFWNS" varchar(4) NOT NULL default ' ',
  "AAABLRST" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABFWNG" varchar(1) NOT NULL default ' ',
  "AAABLISU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABFWNS","AAABLRST","AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWNM" (
  "AAABFWNS" varchar(4) NOT NULL default ' ',
  "AAABMKFH" varchar(4) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFWNR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFWNS","AAABMKFH","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWNW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLCPA" varchar(8) NOT NULL default '00000000',
  "AAABFWOA" varchar(20) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWOD" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABFWOF" varchar(1) NOT NULL default ' ',
  "AAABJDMJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWOU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFWOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFWOU" varchar(16) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFWOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGWEX" varchar(4) NOT NULL default ' ',
  "AAABFWOY" varchar(8) NOT NULL default ' ',
  "AAABFWOZ" varchar(8) NOT NULL default ' ',
  "AAABFWPA" varchar(8) NOT NULL default ' ',
  "AAABFWPB" varchar(8) NOT NULL default ' ',
  "AAABFWPC" varchar(8) NOT NULL default ' ',
  "AAABFWPD" varchar(8) NOT NULL default ' ',
  "AAABFWPE" varchar(8) NOT NULL default ' ',
  "AAABFWPF" varchar(8) NOT NULL default ' ',
  "AAABFWPG" varchar(8) NOT NULL default ' ',
  "AAABFWPH" varchar(8) NOT NULL default ' ',
  "AAABFWPI" varchar(8) NOT NULL default ' ',
  "AAABFWPJ" varchar(8) NOT NULL default ' ',
  "AAABFWPK" varchar(8) NOT NULL default ' ',
  "AAABFWPL" varchar(8) NOT NULL default ' ',
  "AAABFWPM" varchar(8) NOT NULL default ' ',
  "AAABFWPN" varchar(8) NOT NULL default ' ',
  "AAABFWPO" varchar(8) NOT NULL default ' ',
  "AAABFWPP" varchar(8) NOT NULL default ' ',
  "AAABFWPQ" varchar(8) NOT NULL default ' ',
  "AAABFWPR" varchar(8) NOT NULL default ' ',
  "AAABFWPS" varchar(8) NOT NULL default ' ',
  "AAABFWPT" varchar(8) NOT NULL default ' ',
  "AAABFWPU" varchar(8) NOT NULL default ' ',
  "AAABFWPV" varchar(8) NOT NULL default ' ',
  "AAABFWPW" varchar(8) NOT NULL default ' ',
  "AAABFWPX" varchar(8) NOT NULL default ' ',
  "AAABFWPY" varchar(8) NOT NULL default ' ',
  "AAABFWPZ" varchar(8) NOT NULL default ' ',
  "AAABFWQA" varchar(8) NOT NULL default ' ',
  "AAABFWQB" varchar(8) NOT NULL default ' ',
  "AAABFWQC" varchar(8) NOT NULL default ' ',
  "AAABFWQD" varchar(8) NOT NULL default ' ',
  "AAABFWQE" varchar(8) NOT NULL default ' ',
  "AAABFWQF" varchar(8) NOT NULL default ' ',
  "AAABFWQG" varchar(8) NOT NULL default ' ',
  "AAABFWQH" varchar(8) NOT NULL default ' ',
  "AAABFWQI" varchar(8) NOT NULL default ' ',
  "AAABFWQJ" varchar(8) NOT NULL default ' ',
  "AAABFWQK" varchar(8) NOT NULL default ' ',
  "AAABFWQL" varchar(8) NOT NULL default ' ',
  "AAABFWQM" varchar(8) NOT NULL default ' ',
  "AAABFWQN" varchar(8) NOT NULL default ' ',
  "AAABFWQO" varchar(8) NOT NULL default ' ',
  "AAABFWQP" varchar(8) NOT NULL default ' ',
  "AAABFWQQ" varchar(8) NOT NULL default ' ',
  "AAABFWQR" varchar(8) NOT NULL default ' ',
  "AAABFWQS" varchar(8) NOT NULL default ' ',
  "AAABFWQT" varchar(8) NOT NULL default ' ',
  "AAABFWQU" varchar(8) NOT NULL default ' ',
  "AAABFWQV" varchar(8) NOT NULL default ' ',
  "AAABFWQW" varchar(8) NOT NULL default ' ',
  "AAABFWQX" varchar(8) NOT NULL default ' ',
  "AAABFWQY" varchar(8) NOT NULL default ' ',
  "AAABFWQZ" varchar(8) NOT NULL default ' ',
  "AAABFWRA" varchar(8) NOT NULL default ' ',
  "AAABFWRB" varchar(8) NOT NULL default ' ',
  "AAABFWRC" varchar(8) NOT NULL default ' ',
  "AAABFWRD" varchar(8) NOT NULL default ' ',
  "AAABFWRE" varchar(8) NOT NULL default ' ',
  "AAABFWRF" varchar(8) NOT NULL default ' ',
  "AAABFWRG" varchar(8) NOT NULL default ' ',
  "AAABFWRH" varchar(8) NOT NULL default ' ',
  "AAABFWRI" varchar(8) NOT NULL default ' ',
  "AAABFWRJ" varchar(8) NOT NULL default ' ',
  "AAABFWRK" varchar(8) NOT NULL default ' ',
  "AAABFWRL" varchar(8) NOT NULL default ' ',
  "AAABFWRM" varchar(8) NOT NULL default ' ',
  "AAABFWRN" varchar(8) NOT NULL default ' ',
  "AAABFWRO" varchar(8) NOT NULL default ' ',
  "AAABFWRP" varchar(8) NOT NULL default ' ',
  "AAABFWRQ" varchar(8) NOT NULL default ' ',
  "AAABFWRR" varchar(8) NOT NULL default ' ',
  "AAABFWRS" varchar(8) NOT NULL default ' ',
  "AAABFWRT" varchar(8) NOT NULL default ' ',
  "AAABFWRU" varchar(8) NOT NULL default ' ',
  "AAABFWRV" varchar(8) NOT NULL default ' ',
  "AAABFWRW" varchar(8) NOT NULL default ' ',
  "AAABFWRX" varchar(8) NOT NULL default ' ',
  "AAABFWRY" varchar(8) NOT NULL default ' ',
  "AAABFWRZ" varchar(8) NOT NULL default ' ',
  "AAABFWSA" varchar(8) NOT NULL default ' ',
  "AAABFWSB" varchar(8) NOT NULL default ' ',
  "AAABFWSC" varchar(8) NOT NULL default ' ',
  "AAABFWSD" varchar(8) NOT NULL default ' ',
  "AAABFWSE" varchar(8) NOT NULL default ' ',
  "AAABFWSF" varchar(8) NOT NULL default ' ',
  "AAABFWSG" varchar(8) NOT NULL default ' ',
  "AAABFWSH" varchar(8) NOT NULL default ' ',
  "AAABFWSI" varchar(8) NOT NULL default ' ',
  "AAABFWSJ" varchar(8) NOT NULL default ' ',
  "AAABFWSK" varchar(8) NOT NULL default ' ',
  "AAABFWSL" varchar(8) NOT NULL default ' ',
  "AAABFWSM" varchar(8) NOT NULL default ' ',
  "AAABFWSN" varchar(8) NOT NULL default ' ',
  "AAABFWSO" varchar(8) NOT NULL default ' ',
  "AAABFWSP" varchar(8) NOT NULL default ' ',
  "AAABFWSQ" varchar(8) NOT NULL default ' ',
  "AAABFWSR" varchar(8) NOT NULL default ' ',
  "AAABFWSS" varchar(8) NOT NULL default ' ',
  "AAABFWST" varchar(8) NOT NULL default ' ',
  "AAABFWSU" varchar(8) NOT NULL default ' ',
  "AAABFWSV" varchar(8) NOT NULL default ' ',
  "AAABFWSW" varchar(8) NOT NULL default ' ',
  "AAABFWSX" varchar(8) NOT NULL default ' ',
  "AAABFWSY" varchar(8) NOT NULL default ' ',
  "AAABFWSZ" varchar(8) NOT NULL default ' ',
  "AAABFWTA" varchar(8) NOT NULL default ' ',
  "AAABFWTB" varchar(8) NOT NULL default ' ',
  "AAABFWTC" varchar(8) NOT NULL default ' ',
  "AAABFWTD" varchar(8) NOT NULL default ' ',
  "AAABFWTE" varchar(8) NOT NULL default ' ',
  "AAABFWTF" varchar(8) NOT NULL default ' ',
  "AAABFWTG" varchar(8) NOT NULL default ' ',
  "AAABFWTH" varchar(8) NOT NULL default ' ',
  "AAABFWTI" varchar(8) NOT NULL default ' ',
  "AAABFWTJ" varchar(8) NOT NULL default ' ',
  "AAABFWTK" varchar(8) NOT NULL default ' ',
  "AAABFWTL" varchar(8) NOT NULL default ' ',
  "AAABFWTM" varchar(8) NOT NULL default ' ',
  "AAABFWTN" varchar(8) NOT NULL default ' ',
  "AAABFWTO" varchar(8) NOT NULL default ' ',
  "AAABFWTP" varchar(8) NOT NULL default ' ',
  "AAABFWTQ" varchar(8) NOT NULL default ' ',
  "AAABFWTR" varchar(8) NOT NULL default ' ',
  "AAABFWTS" varchar(8) NOT NULL default ' ',
  "AAABFWTT" varchar(8) NOT NULL default ' ',
  "AAABFWTU" varchar(8) NOT NULL default ' ',
  "AAABFWTV" varchar(8) NOT NULL default ' ',
  "AAABFWTW" varchar(8) NOT NULL default ' ',
  "AAABFWTX" varchar(8) NOT NULL default ' ',
  "AAABFWTY" varchar(8) NOT NULL default ' ',
  "AAABFWTZ" varchar(8) NOT NULL default ' ',
  "AAABFWUA" varchar(8) NOT NULL default ' ',
  "AAABFWUB" varchar(8) NOT NULL default ' ',
  "AAABFWUC" varchar(8) NOT NULL default ' ',
  "AAABFWUD" varchar(8) NOT NULL default ' ',
  "AAABFWUE" varchar(8) NOT NULL default ' ',
  "AAABFWUF" varchar(8) NOT NULL default ' ',
  "AAABFWUG" varchar(8) NOT NULL default ' ',
  "AAABFWUH" varchar(8) NOT NULL default ' ',
  "AAABFWUI" varchar(8) NOT NULL default ' ',
  "AAABFWUJ" varchar(8) NOT NULL default ' ',
  "AAABFWUK" varchar(8) NOT NULL default ' ',
  "AAABFWUL" varchar(8) NOT NULL default ' ',
  "AAABFWUM" varchar(8) NOT NULL default ' ',
  "AAABFWUN" varchar(8) NOT NULL default ' ',
  "AAABFWUO" varchar(8) NOT NULL default ' ',
  "AAABFWUP" varchar(8) NOT NULL default ' ',
  "AAABFWUQ" varchar(8) NOT NULL default ' ',
  "AAABFWUR" varchar(8) NOT NULL default ' ',
  "AAABFWUS" varchar(8) NOT NULL default ' ',
  "AAABFWUT" varchar(8) NOT NULL default ' ',
  "AAABFWUU" varchar(8) NOT NULL default ' ',
  "AAABFWUV" varchar(8) NOT NULL default ' ',
  "AAABFWUW" varchar(8) NOT NULL default ' ',
  "AAABFWUX" varchar(8) NOT NULL default ' ',
  "AAABFWUY" varchar(8) NOT NULL default ' ',
  "AAABFWUZ" varchar(8) NOT NULL default ' ',
  "AAABFWVA" varchar(8) NOT NULL default ' ',
  "AAABFWVB" varchar(8) NOT NULL default ' ',
  "AAABFWVC" varchar(8) NOT NULL default ' ',
  "AAABFWVD" varchar(8) NOT NULL default ' ',
  "AAABFWVE" varchar(8) NOT NULL default ' ',
  "AAABFWVF" varchar(8) NOT NULL default ' ',
  "AAABFWVG" varchar(8) NOT NULL default ' ',
  "AAABFWVH" varchar(8) NOT NULL default ' ',
  "AAABFWVI" varchar(8) NOT NULL default ' ',
  "AAABFWVJ" varchar(8) NOT NULL default ' ',
  "AAABFWVK" varchar(8) NOT NULL default ' ',
  "AAABFWVL" varchar(8) NOT NULL default ' ',
  "AAABFWVM" varchar(8) NOT NULL default ' ',
  "AAABFWVN" varchar(8) NOT NULL default ' ',
  "AAABFWVO" varchar(8) NOT NULL default ' ',
  "AAABFWVP" varchar(8) NOT NULL default ' ',
  "AAABFWVQ" varchar(8) NOT NULL default ' ',
  "AAABFWVR" varchar(8) NOT NULL default ' ',
  "AAABFWVS" varchar(8) NOT NULL default ' ',
  "AAABFWVT" varchar(8) NOT NULL default ' ',
  "AAABFWVU" varchar(8) NOT NULL default ' ',
  "AAABFWVV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGWEX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGWJ" varchar(8) NOT NULL default '00000000',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABJGWC" varchar(4) NOT NULL default '0000',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABJGWH" varchar(10) NOT NULL default ' ',
  "AAABJGWI" varchar(13) NOT NULL default ' ',
  "AAABKZRY" VARBINARY(255) NOT NULL,
  "AAABGVFO" varchar(13) NOT NULL default ' ',
  "AAABFWWL" decimal(21,2) NOT NULL default '0.00',
  "AAABFWWM" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABFWWO" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGWJ","AAABJGWB","AAABJGWC","AAABJGWD","AAABJGWE","AAABJGWF","AAABLIMM","AAABJGWH","AAABJGWI","AAABKZRY"(255),"AAABGVFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWYA" varchar(32) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABFWYB" tinyint(3) unsigned NOT NULL default '0',
  "AAABKZWL" varchar(8) NOT NULL default ' ',
  "AAABFWXI" varchar(4) NOT NULL default ' ',
  "AAABFWXJ" varchar(1) NOT NULL default ' ',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABGWAK" varchar(5) NOT NULL default ' ',
  "AAABGVQU" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEN" decimal(21,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABFWYA","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFWYA" varchar(32) NOT NULL default ' ',
  "AAABFWYB" tinyint(3) unsigned NOT NULL default '0',
  "AAABGTPN" varchar(3) NOT NULL default '000',
  "AAABJGVX" decimal(15,6) NOT NULL default '0.000000',
  "AAABFWXY" decimal(9,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAABFWYA","AAABFWYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGZKH" varchar(32) NOT NULL default ' ',
  "AAABJGWB" varchar(3) NOT NULL default ' ',
  "AAABGWEK" varchar(4) NOT NULL default ' ',
  "AAABGTXN" varchar(8) NOT NULL default '00000000',
  "AAABFWYI" varchar(8) NOT NULL default '00000000',
  "AAABFWZD" tinyint(3) unsigned NOT NULL default '0',
  "AAABFWYK" tinyint(3) unsigned NOT NULL default '0',
  "AAABFWYL" tinyint(3) unsigned NOT NULL default '0',
  "AAABGUCC" VARBINARY(255),
  "AAABFWYN" VARBINARY(255),
  "AAABFWYO" VARBINARY(255),
  "AAABFWYP" VARBINARY(255),
  "AAABFWYQ" varchar(1) NOT NULL default ' ',
  "AAABGZFQ" varchar(32) NOT NULL default ' ',
  "AAABGZFR" varchar(32) NOT NULL default ' ',
  "AAABGZIS" varchar(32) NOT NULL default ' ',
  "AAABGZIR" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGZKH"),
  KEY "AAABFWYX" ("AAABGZFQ","AAABGTXN","AAABFWZD"),
  KEY "AAABFWZB" ("AAABGUCC"(255),"AAABFWZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXBQ" varchar(2) NOT NULL default ' ',
  "AAABFXBR" varchar(2) NOT NULL default ' ',
  "AAABFWZJ" varchar(1) NOT NULL default ' ',
  "AAABFWZK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABFXBQ","AAABFXBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFWZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXBQ" varchar(2) NOT NULL default ' ',
  "AAABFXBR" varchar(2) NOT NULL default ' ',
  "AAABFXAC" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFXAU" varchar(30) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABFXBQ","AAABFXBR","AAABFXAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXAI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXAO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXAM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXAO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXAQ" (
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXBQ" varchar(2) NOT NULL default ' ',
  "AAABFXAT" varchar(30) NOT NULL default ' ',
  "AAABFXAU" varchar(30) NOT NULL default ' ',
  "AAABFXAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMY","AAABFXBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXAY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXBQ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMAMY","AAABFXBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXBQ" varchar(2) NOT NULL default ' ',
  "AAABFXBR" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAMY","AAABFXBQ","AAABFXBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXBS" (
  "AAABFXCN" varchar(4) NOT NULL default ' ',
  "AAABFYLB" varchar(1) NOT NULL default ' ',
  "AAABFXCP" varchar(1) NOT NULL default ' ',
  "AAABFXBW" varchar(1) NOT NULL default ' ',
  "AAABFXBX" varchar(1) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  "AAABFXBZ" varchar(1) NOT NULL default ' ',
  "AAABFXCA" varchar(5) NOT NULL default ' ',
  "AAABFXCB" varchar(5) NOT NULL default ' ',
  "AAABFXCC" varchar(5) NOT NULL default ' ',
  "AAABFXCD" varchar(5) NOT NULL default ' ',
  "AAABFXCE" smallint(6) NOT NULL default '0',
  "AAABFXCF" smallint(6) NOT NULL default '0',
  "AAABFXCG" smallint(6) NOT NULL default '0',
  "AAABFXCH" smallint(6) NOT NULL default '0',
  "AAABFXCI" smallint(6) NOT NULL default '0',
  "AAABFXCJ" smallint(6) NOT NULL default '0',
  "AAABFXCK" smallint(6) NOT NULL default '0',
  "AAABFXCL" VARBINARY(255),
  "AAABFXCM" VARBINARY(255),
  PRIMARY KEY ("AAABFXCN","AAABFYLB","AAABFXCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXCQ" (
  "AAABLLIO" varchar(4) NOT NULL default ' ',
  "AAABMIRX" varchar(30) NOT NULL default ' ',
  "AAABFXCT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLIO","AAABMIRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEJ" varchar(4) NOT NULL default ' ',
  "AAABFXCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXEJ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFXEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFXEI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXFM","AAABFXEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFXEI" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFXFM","AAABFXEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFXEI" varchar(2) NOT NULL default ' ',
  "AAABFXEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXFM","AAABFXEI","AAABFXEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXEK" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXEN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXES" (
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFXFO" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXFM","AAABFXFO","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXEZ" (
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFZYX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXFM","AAABFZYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXFM" varchar(2) NOT NULL default ' ',
  "AAABFZYX" varchar(2) NOT NULL default ' ',
  "AAABFXFO" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXFM","AAABFZYX","AAABFXFO","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHSU" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXFV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHSU","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHSU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXHO" varchar(4) NOT NULL default ' ',
  "AAABMLCZ" varchar(1) NOT NULL default ' ',
  "AAABMCEY" varchar(1) NOT NULL default ' ',
  "AAABMCVW" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(1) NOT NULL default ' ',
  "AAABMCEO" varchar(1) NOT NULL default ' ',
  "AAABJSIH" varchar(1) NOT NULL default ' ',
  "AAABLXSU" varchar(1) NOT NULL default ' ',
  "AAABKMUX" varchar(1) NOT NULL default ' ',
  "AAABLZMW" varchar(1) NOT NULL default ' ',
  "AAABLZBN" varchar(1) NOT NULL default ' ',
  "AAABMCSR" varchar(1) NOT NULL default ' ',
  "AAABMPLT" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(1) NOT NULL default ' ',
  "AAABMCRK" varchar(1) NOT NULL default ' ',
  "AAABJCHW" varchar(1) NOT NULL default ' ',
  "AAABJCHX" varchar(1) NOT NULL default ' ',
  "AAABMCGA" varchar(1) NOT NULL default ' ',
  "AAABMCGB" varchar(1) NOT NULL default ' ',
  "AAABFXGZ" varchar(1) NOT NULL default ' ',
  "AAABFXHA" varchar(1) NOT NULL default ' ',
  "AAABKRUF" varchar(1) NOT NULL default ' ',
  "AAABLFPM" varchar(1) NOT NULL default ' ',
  "AAABLLYU" varchar(1) NOT NULL default ' ',
  "AAABFXHE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXHO" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFXHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABFXHY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABFXIP" varchar(1) NOT NULL default '0',
  "AAABLMFC" varchar(1) NOT NULL default ' ',
  "AAABKQUS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXIX","AAABFXIP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXIX" varchar(1) NOT NULL default ' ',
  "AAABFXIU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFXIX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXIY" (
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXJA" varchar(1) NOT NULL default ' ',
  "AAABFXJB" varchar(30) NOT NULL default ' ',
  "AAABFXJC" varchar(1) NOT NULL default ' ',
  "AAABFXJD" varchar(1) NOT NULL default ' ',
  "AAABFXJE" varchar(30) NOT NULL default ' ',
  "AAABFXJF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABFXXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXJH" (
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXZH" varchar(3) NOT NULL default '000',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXLD" varchar(1) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFXYX" varchar(30) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXXG","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXJV" (
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXZH" varchar(3) NOT NULL default '000',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXLD" varchar(1) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFXYX" varchar(30) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABFYAQ" varchar(1) NOT NULL default ' ',
  "AAABFYAT" varchar(1) NOT NULL default ' ',
  "AAABLGJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXXG","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXKM" (
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXLD" varchar(1) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXXG","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXKX" (
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXLD" varchar(1) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFXXG","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXLI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXLL" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFXXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABFXLS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXLW" (
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXMB" varchar(30) NOT NULL default ' ',
  "AAABFXMC" varchar(30) NOT NULL default ' ',
  "AAABFXMD" varchar(30) NOT NULL default ' ',
  "AAABMLPN" varchar(30) NOT NULL default ' ',
  "AAABFXMF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXFC","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXMI" (
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABLQQF" varchar(40) NOT NULL default ' ',
  "AAABFXML" varchar(40) NOT NULL default ' ',
  "AAABHUVY" varchar(4) NOT NULL default ' ',
  "AAABFXMN" varchar(4) NOT NULL default ' ',
  "AAABFXMO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXMQ" (
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABLXFH" varchar(20) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXNO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXFC","AAABLXFD","AAABFXZV","AAABFYAS","AAABLXFH","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXNE" (
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFXUP" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABLXFH" varchar(20) NOT NULL default ' ',
  "AAABFXNO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXFC","AAABFXUP","AAABLXFD","AAABFXZV","AAABFYAS","AAABFXUZ","AAABFXVA","AAABFXZH","AAABLXFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXNY" (
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYAS","AAABLXFC","AAABLXFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXOH" (
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFXUP" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYAS","AAABLXFC","AAABFXUP","AAABLXFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXOS" (
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCV","AAABLXFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXOX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABFXPB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLVCV","AAABLXFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABFXTI" varchar(4) NOT NULL default '0000',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXYE" varchar(30) NOT NULL default ' ',
  "AAABFXYF" varchar(30) NOT NULL default ' ',
  "AAABFXTD" varchar(4) NOT NULL default ' ',
  "AAABFXSX" varchar(4) NOT NULL default ' ',
  "AAABFXQS" varchar(40) NOT NULL default ' ',
  "AAABFXQT" varchar(30) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXQW" varchar(1) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA","AAABFXTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABFXTI" varchar(4) NOT NULL default '0000',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXYE" varchar(30) NOT NULL default ' ',
  "AAABFXYF" varchar(30) NOT NULL default ' ',
  "AAABFXTD" varchar(4) NOT NULL default ' ',
  "AAABFXSX" varchar(4) NOT NULL default ' ',
  "AAABFXQS" varchar(40) NOT NULL default ' ',
  "AAABFXQT" varchar(30) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXQW" varchar(1) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA","AAABFXTI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXRD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXFF" varchar(1) NOT NULL default ' ',
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  "AAABLXFH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXFC","AAABLXFD","AAABLXSA","AAABLXFF","AAABLXFG","AAABLXFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABFXTI" varchar(4) NOT NULL default '0000',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXSX" varchar(4) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXTA" varchar(2) NOT NULL default '00',
  "AAABFXTB" varchar(2) NOT NULL default '00',
  "AAABFXTC" varchar(1) NOT NULL default ' ',
  "AAABFXTD" varchar(4) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA","AAABFXTI","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABFXTI" varchar(4) NOT NULL default '0000',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABFXSU" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXSX" varchar(4) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABFXTA" varchar(2) NOT NULL default '00',
  "AAABFXTB" varchar(2) NOT NULL default '00',
  "AAABFXTC" varchar(1) NOT NULL default ' ',
  "AAABFXTD" varchar(4) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA","AAABFXTI","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXTK" (
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABMRIL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABLXFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABLXSA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABLXFD" varchar(1) NOT NULL default ' ',
  "AAABLXFF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXFC","AAABLXSA","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXUJ" (
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  "AAABFXUO" varchar(1) NOT NULL default ' ',
  "AAABFXUP" varchar(2) NOT NULL default ' ',
  "AAABFXUQ" varchar(30) NOT NULL default ' ',
  "AAABFXUR" varchar(30) NOT NULL default ' ',
  "AAABHKSE" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXFC","AAABFXUZ","AAABFXVA","AAABHUVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXVC" (
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFYIH" varchar(2) NOT NULL default ' ',
  "AAABFXVQ" varchar(1) NOT NULL default ' ',
  "AAABFXVR" varchar(1) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCW","AAABMREF","AAABFYIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXVM" (
  "AAABLVCW" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFYIH" varchar(2) NOT NULL default ' ',
  "AAABFXVQ" varchar(1) NOT NULL default ' ',
  "AAABFXVR" varchar(1) NOT NULL default ' ',
  "AAABJLPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCW","AAABMREF","AAABFYIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABFXWA" varchar(30) NOT NULL default ' ',
  "AAABFXWB" varchar(30) NOT NULL default ' ',
  "AAABFYAJ" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABLXFF" varchar(1) NOT NULL default ' ',
  "AAABFXXI" varchar(1) NOT NULL default ' ',
  "AAABFZAP" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABFXXK" varchar(4) NOT NULL default ' ',
  "AAABFXWJ" varchar(1) NOT NULL default ' ',
  "AAABFXXL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  "AAABFXXQ" varchar(8) NOT NULL default ' ',
  "AAABFXXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXES","AAABHUVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFXXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXXE" (
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFXXG" varchar(30) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFXXI" varchar(1) NOT NULL default ' ',
  "AAABFZAP" varchar(1) NOT NULL default ' ',
  "AAABFXXK" varchar(4) NOT NULL default ' ',
  "AAABFXXL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXXN" (
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  "AAABFXXQ" varchar(8) NOT NULL default ' ',
  "AAABFXXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLXES","AAABHUVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXXU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFXXX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLXES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXYA" (
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABFXYD" varchar(1) NOT NULL default ' ',
  "AAABFXYE" varchar(30) NOT NULL default ' ',
  "AAABFXYF" varchar(30) NOT NULL default ' ',
  "AAABGPQK" varchar(26) NOT NULL default ' ',
  "AAABLXSA" varchar(6) NOT NULL default ' ',
  "AAABGONY" varchar(30) NOT NULL default ' ',
  "AAABHNLP" varchar(1) NOT NULL default ' ',
  "AAABFXYK" varchar(30) NOT NULL default ' ',
  "AAABFXZA" varchar(1) NOT NULL default ' ',
  "AAABLMOC" varchar(8) NOT NULL default '00000000',
  "AAABLMOD" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABLXFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXYP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXFC" varchar(2) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLXFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXYV" (
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFXYX" varchar(30) NOT NULL default ' ',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXZA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXZC" (
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFXZH" varchar(2) NOT NULL default '00',
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYAS","AAABFXZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXZI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABFYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXZO" (
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFXZV" varchar(2) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABLXFG" varchar(40) NOT NULL default ' ',
  "AAABLXFH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCV","AAABFXZV","AAABFYAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFXZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABLXES" varchar(8) NOT NULL default ' ',
  "AAABFYAC" varchar(40) NOT NULL default ' ',
  "AAABFYAD" varchar(40) NOT NULL default ' ',
  "AAABFYAE" varchar(40) NOT NULL default ' ',
  "AAABFYAF" varchar(40) NOT NULL default ' ',
  "AAABFYAG" varchar(40) NOT NULL default ' ',
  "AAABFYAH" varchar(30) NOT NULL default ' ',
  "AAABFYAI" varchar(4) NOT NULL default ' ',
  "AAABFYAJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCV","AAABFYTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYAN" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFYAQ" varchar(1) NOT NULL default ' ',
  "AAABKRIK" varchar(1) NOT NULL default ' ',
  "AAABFYAS" varchar(2) NOT NULL default ' ',
  "AAABFYAT" varchar(1) NOT NULL default ' ',
  "AAABLGJB" varchar(1) NOT NULL default ' ',
  "AAABFYAV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYAY" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFYBJ" varchar(8) NOT NULL default ' ',
  "AAABFYBK" varchar(30) NOT NULL default ' ',
  "AAABFYBL" varchar(8) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  "AAABFYBF" varchar(30) NOT NULL default ' ',
  "AAABFYBG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF","AAABFYBJ","AAABFYBK","AAABFYBL","AAABHUVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYBN" (
  "AAABFYBZ" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABFYCB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABFYBZ","AAABJUQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYBT" (
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYBZ" varchar(30) NOT NULL default ' ',
  "AAABJUQY" varchar(2) NOT NULL default '00',
  "AAABFYCB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLVCV","AAABFYBZ","AAABJUQY","AAABFYCB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABFYCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABFYTT" varchar(4) NOT NULL default ' ',
  "AAABFYCH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLVCV","AAABFYTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
